// OmniNet - WebSocket Implementation
#include "utils/Utils.h"
#include "WebSocket.h"
#include "http/HttpServer.h"

#include <thread>

namespace OmniNet::WebSocket {

// ============================================================================
// Connection
// ============================================================================

Connection::Connection(std::unique_ptr<Socket> socket, std::string_view resource)
    : m_socket(std::move(socket))
    , m_resource(resource)
    , m_isServer(true) {}

Connection::Connection(const std::string& url, std::string_view protocol)
    : m_resource("/")
    , m_isServer(false) {
    // Parse URL and connect
    auto parsed = Handshake::parseUrl(url);
    if (parsed) {
        m_resource = parsed->path;
        m_socket = createConnectedSocket(parsed->host, parsed->port);
        performClientHandshake(url);
    }
}

Connection::~Connection() {
    close(CloseCode::GOING_AWAY);
    if (m_pingThread.joinable()) {
        m_pingRunning = false;
        m_pingCv.notify_all();
        m_pingThread.join();
    }
}

void Connection::sendText(const std::string& message) {
    Frame frame;
    frame.opcode = Opcode::TEXT;
    frame.payload.assign(message.begin(), message.end());
    sendFrame(frame);
    ++m_stats.messagesSent;
}

void Connection::sendBinary(std::span<const uint8_t> data) {
    Frame frame;
    frame.opcode = Opcode::BINARY;
    frame.payload.assign(data.begin(), data.end());
    sendFrame(frame);
    ++m_stats.messagesSent;
}

void Connection::sendPing(std::span<const uint8_t> payload) {
    Frame frame;
    frame.opcode = Opcode::PING;
    frame.payload.assign(payload.begin(), payload.end());
    sendFrame(frame);
}

void Connection::sendPong(std::span<const uint8_t> payload) {
    Frame frame;
    frame.opcode = Opcode::PONG;
    frame.payload.assign(payload.begin(), payload.end());
    sendFrame(frame);
}

void Connection::close(CloseCode code, std::string_view reason) {
    if (!m_open) return;
    
    Frame frame;
    frame.opcode = Opcode::CLOSE;
    auto closeData = buildCloseFrame({code, std::string(reason)});
    frame.payload.assign(closeData.begin(), closeData.end());
    sendFrame(frame);
    
    m_open = false;
    
    if (m_closeHandler) {
        m_closeHandler(code, std::string(reason));
    }
}

std::optional<Frame> Connection::receive() {
    while (m_open) {
        // Read frame header
        std::vector<uint8_t> header(2);
        size_t n = m_socket->recv(header);
        if (n == 0) {
            m_open = false;
            return std::nullopt;
        }
        
        // Decode frame
        size_t consumed = 0;
        auto frame = FrameCodec::decode(header, consumed);
        if (!frame) continue;
        
        // Handle frame
        processFrame(*frame);
        
        // For data frames, return them
        if (!frame->isControl()) {
            return frame;
        }
    }
    
    return std::nullopt;
}

std::optional<std::string> Connection::receiveText() {
    while (auto frame = receive()) {
        if (frame->isText()) {
            return std::string(frame->payload.begin(), frame->payload.end());
        }
        handleControlFrame(*frame);
    }
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> Connection::receiveBinary() {
    while (auto frame = receive()) {
        if (frame->isBinary()) {
            return frame->payload;
        }
        handleControlFrame(*frame);
    }
    return std::nullopt;
}

bool Connection::isOpen() const {
    return m_open.load();
}

std::string_view Connection::resource() const {
    return m_resource;
}

std::string Connection::remoteAddress() const {
    if (m_socket) {
        return m_socket->peerAddress();
    }
    return "";
}

void Connection::setAutoPing(bool enabled) {
    m_autoPing = enabled;
    if (enabled && !m_pingThread.joinable()) {
        m_pingRunning = true;
        m_pingThread = std::thread([this] { pingWorker(); });
    }
}

void Connection::setPingInterval(std::chrono::seconds interval) {
    m_pingInterval = interval;
}

void Connection::setPongTimeout(std::chrono::seconds timeout) {
    m_pongTimeout = timeout;
}

void Connection::sendFragmented(Opcode opcode, std::span<const uint8_t> data, size_t chunkSize) {
    // Cap chunk size to prevent memory issues
    size_t maxChunk = std::min(chunkSize, MAX_FRAGMENT_SIZE);
    
    size_t offset = 0;
    bool first = true;
    
    while (offset < data.size()) {
        Frame frame;
        size_t remaining = data.size() - offset;
        size_t toSend = std::min(maxChunk, remaining);
        
        if (first) {
            frame.opcode = opcode;
            first = false;
        } else {
            frame.opcode = Opcode::CONTINUATION;
        }
        
        frame.fin = (offset + toSend >= data.size());
        frame.payload.assign(data.begin() + offset, data.begin() + offset + toSend);
        
        sendFrame(frame);
        offset += toSend;
    }
}

void Connection::onText(TextHandler handler) {
    m_textHandler = std::move(handler);
}

void Connection::onBinary(BinaryHandler handler) {
    m_binaryHandler = std::move(handler);
}

void Connection::onClose(CloseHandler handler) {
    m_closeHandler = std::move(handler);
}

void Connection::onError(ErrorHandler handler) {
    m_errorHandler = std::move(handler);
}

void Connection::onPing(PingHandler handler) {
    m_pingHandler = std::move(handler);
}

const Connection::Stats& Connection::stats() const {
    return m_stats;
}

void Connection::performServerHandshake() {
    // Server handshake is done by HttpServer
    m_open = true;
}

void Connection::performClientHandshake(const std::string& url) {
    auto parsed = Handshake::parseUrl(url);
    if (!parsed) return;
    
    auto key = Handshake::generateClientKey();
    auto request = Handshake::generateClientRequest(
        parsed->host, parsed->port, parsed->path, key
    );
    
    m_socket->sendAll(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(request.data()),
        request.size()
    ));
    
    // Read response
    auto response = m_socket->recvAll();
    std::string responseStr(response.begin(), response.end());
    
    std::string error;
    if (!Handshake::validateServerResponse(responseStr, key, error)) {
        if (m_errorHandler) {
            m_errorHandler(error);
        }
        return;
    }
    
    m_open = true;
}

void Connection::processFrame(const Frame& frame) {
    if (frame.isControl()) {
        handleControlFrame(frame);
        return;
    }
    
    // Data frames
    if (frame.isText() || frame.isBinary()) {
        if (frame.fin) {
            // Complete message
            if (frame.isText()) {
                std::string msg(frame.payload.begin(), frame.payload.end());
                if (m_textHandler) {
                    m_textHandler(msg);
                }
            } else if (m_binaryHandler) {
                m_binaryHandler(frame.payload);
            }
        } else {
            // Fragmentation start
            m_fragmentOpcode = frame.opcode;
            m_fragmentStarted = true;
            m_fragmentPayload = frame.payload;
        }
    } else if (frame.isContinuation()) {
        if (m_fragmentStarted) {
            m_fragmentPayload.insert(m_fragmentPayload.end(),
                frame.payload.begin(), frame.payload.end());
            
            if (frame.fin) {
                // Complete fragmented message
                if (m_fragmentOpcode == Opcode::TEXT) {
                    std::string msg(m_fragmentPayload.begin(), m_fragmentPayload.end());
                    if (m_textHandler) {
                        m_textHandler(msg);
                    }
                } else if (m_binaryHandler) {
                    m_binaryHandler(m_fragmentPayload);
                }
                m_fragmentStarted = false;
                m_fragmentPayload.clear();
            }
        }
    }
}

void Connection::handleControlFrame(const Frame& frame) {
    if (frame.isPing()) {
        if (m_pingHandler) {
            m_pingHandler(frame.payload);
        }
        sendPong(frame.payload);
    } else if (frame.isPong()) {
        ++m_stats.pongsReceived;
    } else if (frame.isClose()) {
        auto closeFrame = parseCloseFrame(frame.payload);
        m_open = false;
        if (m_closeHandler) {
            m_closeHandler(closeFrame.code, closeFrame.reason);
        }
    }
}

void Connection::sendFrame(const Frame& frame) {
    auto encoded = FrameCodec::encode(frame);
    m_socket->sendAll(encoded);
    m_stats.bytesSent += encoded.size();
}

void Connection::pingWorker() {
    while (m_pingRunning) {
        std::unique_lock<std::mutex> lock(m_pingMutex);
        m_pingCv.wait_for(lock, m_pingInterval, [this] { return !m_pingRunning; });
        
        if (!m_pingRunning) break;
        
        sendPing();
        ++m_stats.pingsSent;
    }
}

// ============================================================================
// Server
// ============================================================================

Server::Server() {
    OmniNet::HTTP::Server::Config config;
    m_httpServer = std::make_unique<OmniNet::HTTP::Server>(config);
}

Server::Server(const OmniNet::HTTP::Server::Config& config) {
    m_httpServer = std::make_unique<OmniNet::HTTP::Server>(config);
    m_router = std::make_unique<OmniNet::HTTP::Router>();
}

void Server::on(std::string path, std::function<void(Connection&)> handler) {
    Endpoint endpoint;
    endpoint.path = std::move(path);
    endpoint.handler = std::move(handler);
    m_endpoints.push_back(std::move(endpoint));
    
    // Store WebSocket handler for this path
    auto& wsHandler = m_endpoints.back().handler;
    std::string wsPath = m_endpoints.back().path;
    
    // Register with HTTP server to handle WebSocket upgrade
    m_httpServer->any(wsPath, [this, wsPath, wsHandler](const OmniNet::HTTP::Request& req, 
                                                       OmniNet::HTTP::Response& resp) {
        // Handle WebSocket upgrade
        auto upgrade = req.header("Upgrade");
        if (!upgrade || Utils::toLower(*upgrade) != "websocket") {
            resp = OmniNet::HTTP::Response::badRequest("WebSocket upgrade required");
            return;
        }
        
        // Get the Sec-WebSocket-Key
        auto key = req.header("Sec-WebSocket-Key");
        if (!key) {
            resp = OmniNet::HTTP::Response::badRequest("Missing Sec-WebSocket-Key");
            return;
        }
        
        // Compute accept key and build response
        auto acceptKey = Handshake::computeAcceptKey(std::string(*key));
        
        std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: Upgrade\r\n";
        response += "Sec-WebSocket-Accept: " + acceptKey + "\r\n";
        response += "\r\n";
        
        // We need to get the socket from the connection
        // Since the handler is set after connection creation, we need a different approach
        // For now, create a WebSocket connection and pass a null socket (will be handled in real implementation)
        // This is a placeholder - in a full implementation, the HTTP server would expose the socket
        
        // Note: Full WebSocket integration requires modifying HttpServer to support upgrade
        // For now, return a response indicating WebSocket upgrade is needed
        resp.setStatus(static_cast<OmniNet::HTTP::StatusCode>(101));
        resp.setHeader("Upgrade", std::string("websocket"));
        resp.setHeader("Connection", std::string("Upgrade"));
        resp.setHeader("Sec-WebSocket-Accept", acceptKey);
        
        // The actual WebSocket handling would happen here after response is sent
        // For demo purposes, we'll leave the connection intact for external WebSocket libraries
    });
}

void Server::start() {
    m_httpServer->start();
}

void Server::stop() {
    m_httpServer->stop();
}

bool Server::isRunning() const {
    return m_httpServer->isRunning();
}

// ----------------------------------------------------------------------
// v0.7.0 (Phase 11b): client tracking + broadcast
// ----------------------------------------------------------------------

Server::ClientId Server::addClient(Connection& conn, std::string_view path) {
    if (!conn.isOpen()) return 0;
    ClientId id = m_nextClientId.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients.emplace(id, std::make_pair(std::string(path), &conn));
    return id;
}

void Server::removeClient(ClientId id) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients.erase(id);
}

void Server::sendTo(ClientId id, std::string_view text) {
    Connection* conn = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        auto it = m_clients.find(id);
        if (it == m_clients.end()) return;  // unknown id = no-op
        conn = it->second.second;
    }
    if (conn && conn->isOpen()) {
        try {
            conn->sendText(std::string(text));
        } catch (...) {
            // Send failed: remove the dead client.
            removeClient(id);
        }
    } else {
        removeClient(id);
    }
}

void Server::broadcast(std::string_view text) {
    // Snapshot the ids under the lock, then send outside the lock to
    // minimize contention. A dead client is removed on send failure.
    std::vector<ClientId> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        snapshot.reserve(m_clients.size());
        for (const auto& [id, _] : m_clients) snapshot.push_back(id);
    }
    for (ClientId id : snapshot) {
        sendTo(id, text);
    }
}

void Server::broadcastText(std::string_view text) {
    broadcast(text);
}

void Server::broadcast(std::string_view text, std::string_view path) {
    std::vector<ClientId> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        snapshot.reserve(m_clients.size());
        for (const auto& [id, entry] : m_clients) {
            if (entry.first == path) snapshot.push_back(id);
        }
    }
    for (ClientId id : snapshot) {
        sendTo(id, text);
    }
}

std::vector<Server::ClientId> Server::clients() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    std::vector<ClientId> out;
    out.reserve(m_clients.size());
    for (const auto& [id, _] : m_clients) out.push_back(id);
    return out;
}

std::vector<Server::ClientId> Server::clients(std::string_view path) const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    std::vector<ClientId> out;
    for (const auto& [id, entry] : m_clients) {
        if (entry.first == path) out.push_back(id);
    }
    return out;
}

size_t Server::connectionCount() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clients.size();
}

size_t Server::connectionCount(std::string_view path) const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    size_t count = 0;
    for (const auto& [_, entry] : m_clients) {
        if (entry.first == path) ++count;
    }
    return count;
}

} // namespace OmniNet::WebSocket