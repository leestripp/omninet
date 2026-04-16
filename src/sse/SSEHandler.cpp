// OmniNet - SSE Handler Implementation
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

Event Event::data(const std::string& data) {
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
    
    // Build SSE response
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("text/event-stream");
    resp.setHeader("Cache-Control", "no-cache");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Access-Control-Allow-Origin", "*");
    
    // Generate client ID
    auto clientId = SecureToken::generateUrlSafe(16);
    
    // Send initial comment
    std::string initial = ": connected\r\n\r\n";
    
    // Create client entry
    auto client = std::make_shared<Client>();
    client->lastActivity = std::chrono::steady_clock::now();
    
    auto sender = [&initial](const std::string& data) -> bool {
        // In real implementation, would send to actual socket
        (void)data;
        return true;
    };
    
    addClient(clientId, sender);
    
    // Send initial event
    client->sender(initial);
    
    // Note: In a real implementation, the request would hang until client disconnects
    // This is handled by the HTTP server
    
    return true;
}

void Handler::broadcast(const Event& event) {
    auto formatted = event.format();
    
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto& [id, client] : m_clients) {
        if (client->connected) {
            client->sender(formatted);
        }
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
    for (auto& [id, client] : m_clients) {
        if (client->connected && 
            client->subscriptions.count(eventType) > 0) {
            client->sender(formatted);
        }
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
    
    if (m_connectHandler) {
        m_connectHandler(clientId);
    }
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