// OmniNet - SSE Handler Implementation
#include "utils/Utils.h"
#include "utils/Logger.h"
#include "crypto/Crypto.h"
#include "net/Socket.h"
#include "SSEHandler.h"

#include <thread>

namespace OmniNet::SSE {

// ============================================================================
// Event
// ============================================================================

std::string Event::format() const {
    std::ostringstream oss;
    
    // ID
    if (!id.empty()) {
        oss << "id: " << id << "\r\n";
    }
    
    // Event type
    if (!event.empty()) {
        oss << "event: " << event << "\r\n";
    }
    
    // Retry
    if (retry > 0) {
        oss << "retry: " << retry << "\r\n";
    }
    
    // Data (may be multiple lines)
    auto lines = Utils::split(data, '\n');
    for (const auto& line : lines) {
        oss << "data: " << line << "\r\n";
    }
    
    oss << "\r\n";
    return oss.str();
}

Event Event::comment(const std::string& text) {
    Event e;
    e.data = ":" + text;
    return e;
}

Event Event::make(const std::string& data) {
    Event e;
    e.data = data;
    return e;
}

Event Event::named(const std::string& type, const std::string& data) {
    Event e;
    e.event = type;
    e.data = data;
    return e;
}

// ============================================================================
// Handler
// ============================================================================

Handler::Handler(const std::string& path)
    : m_path(path), m_keepAliveInterval(std::chrono::seconds(30)) {
    m_running = true;
    m_keepAliveThread = std::thread([this] { keepAliveWorker(); });
}

Handler::~Handler() {
    m_running = false;
    if (m_keepAliveThread.joinable()) {
        m_keepAliveThread.join();
    }
}

// v0.8.3 (MonkeyByte Bug 1): deprecated as a route handler.
// The HTTP server now uses on_upgrade (in attach()) to set the
// response, then the on_post_upgrade callback spawns a worker
// thread that owns the socket and writes chunks. This method is
// kept only for backward compatibility - it does NOT actually
// stream anything.
bool Handler::handleRequest(const HTTP::Request& req, HTTP::Response& resp) {
    // Check path
    if (req.path() != m_path) {
        return false;
    }

    // Must be GET
    if (req.method() != HTTP::Method::GET) {
        resp = HTTP::Response::methodNotAllowed("GET");
        return true;
    }

    // Set up the response headers. Note: NO body - the body
    // arrives later as chunked frames written by the worker thread.
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("text/event-stream");
    resp.setHeader("Cache-Control", "no-cache");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Access-Control-Allow-Origin", "*");
    resp.setTransferEncoding("chunked");
    resp.markStreaming();

    return true;
}

// v0.8.3 (MonkeyByte Bug 1): register this handler as an SSE
// endpoint. The HTTP server wires the protocol switch and runs
// runClientStream() in a detached worker thread per client.
void Handler::attach(HTTP::Server& server) {
    Handler* self = this;
    std::string path = m_path;

    server.registerProtocolSwitch(path, {
        // on_upgrade: set the response to 200 OK + chunked.
        // Server writes the headers, then fires on_post_upgrade.
        .on_upgrade = [path](const HTTP::Request& req, HTTP::Response& resp) {
            (void)path; (void)req;
            resp.setStatus(HTTP::StatusCode::OK);
            resp.setContentType("text/event-stream");
            resp.setHeader("Cache-Control", "no-cache");
            resp.setHeader("Connection", "keep-alive");
            resp.setHeader("X-Accel-Buffering", "no");
            resp.setTransferEncoding("chunked");
            resp.markStreaming();
        },

        // on_post_upgrade: spawn worker thread that owns the
        // socket and writes SSE chunks until client disconnects.
        .on_post_upgrade = [self](std::unique_ptr<Socket> socket,
                                  const HTTP::Request& req) {
            std::thread([self, sock = std::move(socket), req]() mutable {
                try {
                    self->runClientStream(std::move(sock), req);
                } catch (const std::exception& e) {
                    OMNINET_LOG_ERROR(std::string("SSE worker exception: ") + e.what());
                } catch (...) {
                    OMNINET_LOG_ERROR(std::string("SSE worker unknown exception"));
                }
            }).detach();
        },
    });
}

// v0.8.3 (MonkeyByte Bug 1): run one client connection until
// the client disconnects. Owns the socket.
void Handler::runClientStream(std::unique_ptr<Socket> socket,
                              const HTTP::Request& req) {
    if (!socket || !socket->isOpen()) return;

    // Generate client ID.
    std::string clientId = OmniNet::SecureToken::generateUrlSafe(16);
    if (m_connectHandler) {
        if (auto overrideId = m_connectHandler(req); !overrideId.empty()) {
            clientId = overrideId;
        }
    }

    // Set a short recv timeout so we periodically check if the
    // broadcast map still has us (and avoid burning a CPU in a
    // tight loop on disconnect detection).
    constexpr auto kPollTimeout = std::chrono::milliseconds(50);
    try { socket->setRecvTimeout(kPollTimeout); } catch (...) {}

    // Sender: write one chunked frame to the socket. Returns
    // false on any socket error so broadcast() can dead-letter-drop.
    auto sock = socket.get();
    auto sender = [sock](const std::string& data) -> bool {
        return writeChunked(*sock, data);
    };

    addClient(clientId, sender);

    // Send an initial comment so the client's EventSource opens
    // immediately and proxies don't time out the connect.
    std::string initial = Event::comment("connected").format();
    if (!sender(initial)) {
        removeClient(clientId);
        return;
    }

    OMNINET_LOG_DEBUG(std::string("SSE client connected: ") + clientId +
        " on " + m_path);

    // Wait for client disconnect. recv() returns empty (or
    // throws) when the client closes or the timeout expires.
    // We poll on the recv timeout; the keep-alive worker handles
    // periodic comment writes via broadcast().
    while (m_running && socket && socket->isOpen()) {
        try {
            auto data = socket->recvAll(1);
            // Any data from the client means client is alive but
            // doesn't speak the SSE wire protocol - we ignore it.
            // Empty data after timeout means client is still alive
            // (we just timed out the recv, no real disconnect).
            (void)data;
        } catch (...) {
            // Real disconnect.
            break;
        }
    }

    // Cleanup.
    removeClient(clientId);

    if (socket) {
        try { writeChunkedEnd(*socket); } catch (...) {}
        try { socket->close(); } catch (...) {}
    }

    OMNINET_LOG_DEBUG(std::string("SSE client disconnected: ") + clientId);
}

// v0.8.3: write one chunked frame. Hex-encodes the size, then
// writes data, then the trailing CRLF. Returns false on socket error.
bool Handler::writeChunked(Socket& sock, std::string_view data) {
    if (!sock.isOpen()) return false;
    std::ostringstream oss;
    oss << std::hex << data.size() << "\r\n";
    auto header = oss.str();
    try {
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(header.data()),
            header.size()));
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()),
            data.size()));
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>("\r\n"), 2));
    } catch (...) {
        return false;
    }
    return true;
}

// v0.8.3: write the chunked terminator (0\r\n\r\n).
void Handler::writeChunkedEnd(Socket& sock) {
    if (!sock.isOpen()) return;
    try {
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>("0\r\n\r\n"), 5));
    } catch (...) {}
}

void Handler::broadcast(const Event& event) {
    auto formatted = event.format();

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    // v0.8.3 (MonkeyByte Bug 1, C.4): dead-letter-drop. If a
    // sender returns false, the socket is dead — erase the
    // client so subsequent broadcasts skip it.
    for (auto it = m_clients.begin(); it != m_clients.end(); ) {
        if (!it->second->connected) {
            it = m_clients.erase(it);
            continue;
        }
        if (!it->second->sender(formatted)) {
            it = m_clients.erase(it);
            continue;
        }
        ++it;
    }
}

void Handler::broadcast(const std::string& eventType, const std::string& data) {
    broadcast(Event::named(eventType, data));
}

size_t Handler::clientCount() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    size_t count = 0;
    for (const auto& [id, client] : m_clients) {
        if (client->connected) {
            ++count;
        }
    }
    return count;
}

void Handler::removeClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        if (m_disconnectHandler) {
            m_disconnectHandler(clientId);
        }
        m_clients.erase(it);
    }
}

void Handler::setKeepAliveInterval(std::chrono::seconds interval) {
    m_keepAliveInterval = interval;
}

std::chrono::seconds Handler::keepAliveInterval() const {
    return m_keepAliveInterval;
}

void Handler::setMaxQueueSize(size_t size) {
    m_maxQueueSize = size;
}

size_t Handler::maxQueueSize() const {
    return m_maxQueueSize;
}

void Handler::onConnect(ConnectHandler handler) {
    m_connectHandler = std::move(handler);
}

void Handler::onDisconnect(DisconnectHandler handler) {
    m_disconnectHandler = std::move(handler);
}

void Handler::broadcastTo(const std::string& eventType, const Event& event) {
    auto formatted = event.format();

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    // v0.8.3 (MonkeyByte Bug 1, C.4): dead-letter-drop on send failure.
    for (auto it = m_clients.begin(); it != m_clients.end(); ) {
        if (!it->second->connected ||
            it->second->subscriptions.count(eventType) == 0) {
            // Skip (don't drop just because of filter mismatch).
            ++it;
            continue;
        }
        if (!it->second->sender(formatted)) {
            it = m_clients.erase(it);
            continue;
        }
        ++it;
    }
}

void Handler::subscribe(const std::string& clientId, const std::string& eventType) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        it->second->subscriptions.insert(eventType);
    }
}

void Handler::unsubscribe(const std::string& clientId, const std::string& eventType) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientId);
    if (it != m_clients.end()) {
        it->second->subscriptions.erase(eventType);
    }
}

void Handler::addClient(const std::string& clientId, std::function<bool(const std::string&)> sender) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto client = std::make_shared<Client>();
    client->sender = std::move(sender);
    m_clients[clientId] = client;
}

void Handler::keepAliveWorker() {
    while (m_running) {
        std::this_thread::sleep_for(m_keepAliveInterval);
        
        if (!m_running) break;
        
        // Send keep-alive comment
        broadcast(Event::comment("keep-alive"));
    }
}

// ============================================================================
// Client
// ============================================================================

Client::Client(const std::string& url) : m_url(url) {}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    m_socket = createConnectedSocket(m_url, 80);
    m_connected = true;
    
    m_receiveThread = std::thread([this] { runReceiveLoop(); });
    
    if (m_openHandler) {
        m_openHandler();
    }
    
    return true;
}

void Client::disconnect() {
    m_connected = false;
    
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
    
    if (m_closeHandler) {
        m_closeHandler();
    }
}

bool Client::isConnected() const {
    return m_connected.load();
}

std::optional<Event> Client::receive() {
    std::unique_lock<std::mutex> lock(m_eventMutex);
    
    if (m_eventQueue.empty()) {
        m_eventCv.wait_for(lock, std::chrono::seconds(30));
    }
    
    if (!m_eventQueue.empty()) {
        auto event = m_eventQueue.front();
        m_eventQueue.erase(m_eventQueue.begin());
        return event;
    }
    
    return std::nullopt;
}

std::optional<Event> Client::receive(std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(m_eventMutex);
    
    if (m_eventQueue.empty()) {
        m_eventCv.wait_for(lock, timeout);
    }
    
    if (!m_eventQueue.empty()) {
        auto event = m_eventQueue.front();
        m_eventQueue.erase(m_eventQueue.begin());
        return event;
    }
    
    return std::nullopt;
}

void Client::onMessage(MessageHandler handler) {
    m_messageHandler = std::move(handler);
}

void Client::onError(ErrorHandler handler) {
    m_errorHandler = std::move(handler);
}

void Client::onOpen(OpenHandler handler) {
    m_openHandler = std::move(handler);
}

void Client::onClose(CloseHandler handler) {
    m_closeHandler = std::move(handler);
}

std::string Client::lastEventId() const {
    return m_lastEventId;
}

std::string Client::url() const {
    return m_url;
}

void Client::runReceiveLoop() {
    while (m_connected && m_socket) {
        auto data = m_socket->recvAll(65536);
        if (data.empty()) {
            break;
        }
        
        std::string str(data.begin(), data.end());
        parseEventStream(str);
    }
    
    m_connected = false;
}

void Client::parseEventStream(const std::string& data) {
    // Simple SSE parser
    auto lines = Utils::split(data, '\n');
    
    Event event;
    bool inData = false;
    
    for (const auto& line : lines) {
        if (line.empty()) {
            // End of event
            if (!event.data.empty() && m_messageHandler) {
                m_messageHandler(event);
            }
            
            event = Event();
            inData = false;
            continue;
        }
        
        if (line.starts_with("data:")) {
            auto value = Utils::trim(line.substr(5));
            if (inData) {
                event.data += '\n';
                event.data += value;
            } else {
                event.data = value;
                inData = true;
            }
        } else if (line.starts_with("id:")) {
            event.id = Utils::trim(line.substr(3));
            m_lastEventId = event.id;
        } else if (line.starts_with("event:")) {
            event.event = Utils::trim(line.substr(6));
        } else if (line.starts_with("retry:")) {
            event.retry = std::stoi(std::string(Utils::trim(line.substr(6))));
        } else if (line.starts_with(":")) {
            // Comment - ignore
        }
    }
}

// ============================================================================
// Middleware
// ============================================================================

SSEMiddleware::SSEMiddleware(Handler& handler) : m_handler(handler) {}

bool SSEMiddleware::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    return m_handler.handleRequest(req, resp);
}

} // namespace OmniNet::SSE