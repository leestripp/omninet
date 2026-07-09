// OmniNet - SSE Handler Implementation (v0.8.3 refactor — simplified)
//
// v0.8.3 (MonkeyByte Bug 1) refresher:
//   Previously this file was a ~500-line staircase of subscriptions,
//   keep-alive workers, dead-letter-drop, and forked broadcast paths.
//   The complexity introduced subtle race conditions that made the
//   test suite flake. This rewrite reduces the implementation to
//   the minimum surface needed:
//
//     - addClient  : register a sender
//     - removeClient: forget a sender
//     - broadcast  : write the event to every registered sender
//     - runClientStream: worker entry that owns one socket, blocks
//                        in recv() until the client disconnects
//
//   No subscriptions, no dead-letter-drop (the worker thread's recv
//   loop detects disconnect and removes the client itself), no
//   keep-alive thread. Keep-alive is sent lazily by broadcast()
//   when there are events; if there are no events for a long time,
//   the connection just sits idle — that's fine for SSE clients
//   which auto-reconnect with the Last-Event-ID header.

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

    if (!id.empty())    oss << "id: "    << id    << "\r\n";
    if (!event.empty()) oss << "event: " << event << "\r\n";
    if (retry > 0)      oss << "retry: " << retry << "\r\n";

    auto lines = Utils::split(data, '\n');
    for (const auto& line : lines) {
        oss << "data: " << line << "\r\n";
    }

    oss << "\r\n";
    return oss.str();
}

Event Event::comment(const std::string& text) {
    Event e;
    e.data = ":" + text;  // SSE comment line starts with ":"
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
    e.data  = data;
    return e;
}

// ============================================================================
// Handler
// ============================================================================

Handler::Handler(const std::string& path) : m_path(path) {}

Handler::~Handler() {
    // Detached worker threads check m_running (no — actually they
    // check Handler ptr lifetime via the captured `self`). They'll
    // see the map disappear when this Handler dies and exit on
    // their next disconnect.
    //
    // In practice, callers should `Server::stop()` BEFORE destroying
    // the Handler so the post-upgrade callback (which captured
    // `self`) fires from detached threads that no longer reference
    // destroyed state.
}

// v0.8.3 (MonkeyByte Bug 1): register this handler as an SSE endpoint.
void Handler::attach(HTTP::Server& server) {
    Handler* self = this;

    server.registerProtocolSwitch(m_path, {
        // on_upgrade: 200 OK + chunked headers; mark streaming.
        .on_upgrade = [](const HTTP::Request& req, HTTP::Response& resp) {
            (void)req;
            resp.setStatus(HTTP::StatusCode::OK);
            resp.setContentType("text/event-stream");
            resp.setHeader("Cache-Control", "no-cache");
            resp.setHeader("Connection", "keep-alive");
            resp.setHeader("X-Accel-Buffering", "no");
            resp.setTransferEncoding("chunked");
            resp.markStreaming();
        },

        // on_post_upgrade: spawn worker thread per client.
        .on_post_upgrade = [self](std::unique_ptr<Socket> socket,
                                  const HTTP::Request& req) {
            std::thread([self, sock = std::move(socket), req]() mutable {
                try {
                    self->runClientStream(std::move(sock), req);
                } catch (const std::exception& e) {
                    OMNINET_LOG_ERROR(
                        std::string("SSE worker exception: ") + e.what());
                } catch (...) {
                    OMNINET_LOG_ERROR(std::string("SSE worker unknown exception"));
                }
            }).detach();
        },
    });
}

// v0.8.3 (MonkeyByte Bug 1): run one client connection until disconnect.
// Owns the socket for the duration.
void Handler::runClientStream(std::unique_ptr<Socket> socket,
                              const HTTP::Request& req) {
    if (!socket || !socket->isOpen()) return;

    // Allocate client ID.
    std::string clientId = OmniNet::SecureToken::generateUrlSafe(16);
    if (m_connectHandler) {
        if (auto overrideId = m_connectHandler(req); !overrideId.empty()) {
            clientId = overrideId;
        }
    }

    // Sender: closure-captures the raw socket pointer.
    auto* sock = socket.get();
    auto sender = [sock](const std::string& data) -> bool {
        return writeChunked(*sock, data);
    };

    addClient(clientId, sender);

    // Initial ":connected" comment so the client's EventSource
    // opens immediately (proxies don't time out the connect).
    std::string initial = Event::comment("connected").format();
    if (!sender(initial)) {
        removeClient(clientId);
        if (socket) socket->close();
        return;
    }

    OMNINET_LOG_DEBUG(std::string("SSE client connected: ") + clientId +
                      " on " + m_path);

    // Block in recv() until the client disconnects.
    // recvAll() returns empty on EOF (FIN) or throws on socket error.
    while (socket->isOpen()) {
        try {
            auto data = socket->recvAll(1);
            if (data.empty()) break;  // EOF
            // SSE is server→client; ignore client→server.
        } catch (...) {
            break;
        }
    }

    // Cleanup.
    removeClient(clientId);

    if (socket && socket->isOpen()) {
        try { writeChunkedEnd(*socket); } catch (...) {}
        try { socket->close(); } catch (...) {}
    }

    OMNINET_LOG_DEBUG(std::string("SSE client disconnected: ") + clientId);
}

// Write one chunked frame. Hex-size + CRLF + data + CRLF.
bool Handler::writeChunked(Socket& sock, std::string_view data) {
    if (!sock.isOpen()) return false;
    try {
        std::ostringstream oss;
        oss << std::hex << data.size() << "\r\n";
        auto header = oss.str();
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(header.data()), header.size()));
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(data.data()), data.size()));
        sock.sendAll(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>("\r\n"), 2));
    } catch (...) {
        return false;
    }
    return true;
}

// Write the chunked terminator: "0\r\n\r\n".
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
    for (auto& [id, client] : m_clients) {
        if (!client->connected) continue;
        // Best-effort send. If the socket is dead, the worker's
        // recv() will detect EOF and remove the client itself on
        // the next pass — we don't try to erase here (would
        // require careful re-locking under the iterator).
        try {
            client->sender(formatted);
        } catch (...) {
            // ignore — worker will clean up
        }
    }
}

void Handler::broadcast(const std::string& eventType, const std::string& data) {
    broadcast(Event::named(eventType, data));
}

size_t Handler::clientCount() const {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clients.size();
}

void Handler::addClient(const std::string& clientId, Sender sender) {
    auto client = std::make_shared<Client>();
    client->sender = std::move(sender);
    client->lastActivity = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_clients[clientId] = std::move(client);
}

void Handler::removeClient(const std::string& clientId) {
    DisconnectHandler dh;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        auto it = m_clients.find(clientId);
        if (it == m_clients.end()) return;
        dh = m_disconnectHandler;
        m_clients.erase(it);
    }
    // Invoke user callback OUTSIDE the lock to avoid deadlocks.
    if (dh) dh(clientId);
}

void Handler::onConnect(ConnectHandler handler) {
    m_connectHandler = std::move(handler);
}

void Handler::onDisconnect(DisconnectHandler handler) {
    m_disconnectHandler = std::move(handler);
}

// ============================================================================
// Client
// ============================================================================

Client::Client(const std::string& url) : m_url(url) {}

Client::~Client() { disconnect(); }

bool Client::connect() {
    m_socket = createConnectedSocket(m_url, 80);
    m_connected = true;
    m_receiveThread = std::thread([this] { runReceiveLoop(); });
    if (m_openHandler) m_openHandler();
    return true;
}

void Client::disconnect() {
    m_connected = false;
    if (m_receiveThread.joinable()) m_receiveThread.join();
    if (m_closeHandler) m_closeHandler();
}

bool Client::isConnected() const { return m_connected.load(); }

std::optional<Event> Client::receive() {
    std::unique_lock<std::mutex> lock(m_eventMutex);
    if (m_eventQueue.empty()) m_eventCv.wait_for(lock, std::chrono::seconds(30));
    if (!m_eventQueue.empty()) {
        auto event = m_eventQueue.front();
        m_eventQueue.erase(m_eventQueue.begin());
        return event;
    }
    return std::nullopt;
}

std::optional<Event> Client::receive(std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(m_eventMutex);
    if (m_eventQueue.empty()) m_eventCv.wait_for(lock, timeout);
    if (!m_eventQueue.empty()) {
        auto event = m_eventQueue.front();
        m_eventQueue.erase(m_eventQueue.begin());
        return event;
    }
    return std::nullopt;
}

void Client::onMessage(MessageHandler handler) { m_messageHandler = std::move(handler); }
void Client::onError(ErrorHandler handler)    { m_errorHandler    = std::move(handler); }
void Client::onOpen(OpenHandler handler)      { m_openHandler      = std::move(handler); }
void Client::onClose(CloseHandler handler)    { m_closeHandler     = std::move(handler); }

std::string Client::lastEventId() const { return m_lastEventId; }
std::string Client::url() const { return m_url; }

void Client::runReceiveLoop() {
    while (m_connected && m_socket) {
        auto data = m_socket->recvAll(65536);
        if (data.empty()) break;
        std::string str(data.begin(), data.end());
        parseEventStream(str);
    }
    m_connected = false;
}

void Client::parseEventStream(const std::string& data) {
    auto lines = Utils::split(data, '\n');
    Event event;
    bool inData = false;
    for (const auto& line : lines) {
        if (line.empty()) {
            if (!event.data.empty() && m_messageHandler) m_messageHandler(event);
            event = Event();
            inData = false;
            continue;
        }
        if (line.starts_with("data:")) {
            auto value = Utils::trim(line.substr(5));
            if (inData) { event.data += '\n'; event.data += value; }
            else        { event.data  = value; inData = true; }
        } else if (line.starts_with("id:")) {
            event.id = Utils::trim(line.substr(3));
            m_lastEventId = event.id;
        } else if (line.starts_with("event:")) {
            event.event = Utils::trim(line.substr(6));
        } else if (line.starts_with("retry:")) {
            event.retry = std::stoi(std::string(Utils::trim(line.substr(6))));
        } else if (line.starts_with(":")) {
            // comment, ignore
        }
    }
}

// SSEMiddleware was removed in v0.8.3 (MonkeyByte Bug 1) — the
// HTTP::Server::registerProtocolSwitch mechanism supersedes the
// old middleware-based approach. See Handler::attach() above.

// ============================================================================
// Client (outbound SSE)
// ============================================================================

} // namespace OmniNet::SSE