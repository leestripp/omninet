// OmniNet - HTTP Connection Implementation
#include "HttpConnection.h"

namespace OmniNet::HTTP {

// ============================================================================
// Connection
// ============================================================================

Connection::Connection(std::unique_ptr<Socket> socket)
    : m_socket(std::move(socket)) {}

Connection::~Connection() = default;

bool Connection::handleNextRequest() {
    // Read request
    if (!readRequest()) {
        return false;
    }
    
    // Find handler and execute
    if (m_handler) {
        Response resp;
        try {
            m_handler(m_currentRequest, resp);
        } catch (const std::exception& e) {
            handleError(std::current_exception());
        }
        
        // Send response
        return sendResponse(resp);
    }
    
    return false;
}

void Connection::processRequests() {
    while (handleNextRequest() && m_keepAlive) {
        m_currentRequest.reset();
    }
}

void Connection::setHandler(Handler handler) {
    m_handler = std::move(handler);
}

void Connection::setErrorHandler(ErrorHandler handler) {
    m_errorHandler = std::move(handler);
}

void Connection::setRequestTimeout(std::chrono::seconds timeout) {
    m_requestTimeout = timeout;
}

void Connection::setKeepAliveTimeout(std::chrono::seconds timeout) {
    m_keepAliveTimeout = timeout;
}

void Connection::setMaxRequestSize(size_t maxSize) {
    m_maxRequestSize = maxSize;
}

void Connection::onRequest(Handler handler) {
    setHandler(handler);
}

void Connection::onError(ErrorHandler handler) {
    setErrorHandler(handler);
}

bool Connection::isOpen() const {
    return m_socket && m_socket->isOpen();
}

void Connection::close() {
    if (m_socket) {
        m_socket->close();
    }
}

const Socket& Connection::socket() const {
    return *m_socket;
}

const Connection::Stats& Connection::stats() const {
    return m_stats;
}

bool Connection::readRequest() {
    // Read data
    auto data = m_socket->recvAll(m_maxRequestSize);
    if (data.empty()) {
        return false;
    }
    
    m_stats.bytesReceived += data.size();
    ++m_stats.requestsReceived;
    
    // Parse request
    size_t consumed = m_currentRequest.parse(data);
    return consumed > 0;
}

bool Connection::sendResponse(const Response& resp) {
    auto data = resp.buildFull();
    if (data.empty()) {
        return false;
    }
    
    m_socket->sendAll(data);
    m_stats.bytesSent += data.size();
    ++m_stats.requestsProcessed;
    
    // Check for connection close
    m_keepAlive = m_currentRequest.keepAlive();
    if (resp.header("Connection") == "close") {
        m_keepAlive = false;
    }
    
    return m_keepAlive;
}

void Connection::handleError(std::exception_ptr e) {
    ++m_stats.errors;
    if (m_errorHandler) {
        Response resp;
        m_errorHandler(m_currentRequest, resp, e);
    }
}

// ============================================================================
// DefaultConnectionFactory
// ============================================================================

DefaultConnectionFactory::DefaultConnectionFactory() = default;

std::unique_ptr<Connection> DefaultConnectionFactory::create(std::unique_ptr<Socket> socket) {
    auto conn = std::make_unique<Connection>(std::move(socket));
    
    if (m_handler) {
        conn->setHandler(m_handler);
    }
    if (m_errorHandler) {
        conn->setErrorHandler(m_errorHandler);
    }
    conn->setRequestTimeout(m_requestTimeout);
    conn->setKeepAliveTimeout(m_keepAliveTimeout);
    conn->setMaxRequestSize(m_maxRequestSize);
    
    return conn;
}

void DefaultConnectionFactory::setHandler(Connection::Handler handler) {
    m_handler = std::move(handler);
}

void DefaultConnectionFactory::setErrorHandler(Connection::ErrorHandler handler) {
    m_errorHandler = std::move(handler);
}

void DefaultConnectionFactory::setRequestTimeout(std::chrono::seconds timeout) {
    m_requestTimeout = timeout;
}

void DefaultConnectionFactory::setKeepAliveTimeout(std::chrono::seconds timeout) {
    m_keepAliveTimeout = timeout;
}

void DefaultConnectionFactory::setMaxRequestSize(size_t maxSize) {
    m_maxRequestSize = maxSize;
}

} // namespace OmniNet::HTTP