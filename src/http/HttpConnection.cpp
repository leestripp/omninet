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
    // Read until we have complete headers (double CRLF)
    // Then optionally read body based on Content-Length
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);
    
    enum class State { READING_HEADERS, READING_BODY };
    State state = State::READING_HEADERS;
    size_t contentLength = 0;
    bool haveSeenDoubleCrlf = false;
    
    while (m_socket->isOpen()) {
        // Read available data
        std::vector<uint8_t> chunk = m_socket->recvAll(65536);
        if (chunk.empty()) {
            // Connection closed or timeout
            if (buffer.empty()) {
                return false;
            }
            break;
        }
        
        buffer.insert(buffer.end(), chunk.begin(), chunk.end());
        
        // Check if we have complete headers
        if (state == State::READING_HEADERS) {
            // Look for double CRLF (end of headers)
            for (size_t i = 0; i + 3 < buffer.size(); ++i) {
                if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                    buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                    haveSeenDoubleCrlf = true;
                    break;
                }
            }
            
            if (haveSeenDoubleCrlf) {
                // Find the end of headers
                size_t headerEnd = 0;
                for (size_t i = 0; i + 3 < buffer.size(); ++i) {
                    if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                        buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                        headerEnd = i + 4;
                        break;
                    }
                }
                
                // Parse headers
                std::string headerStr(buffer.begin(), buffer.begin() + headerEnd);
                size_t consumed = m_currentRequest.parse(headerStr);
                (void)consumed; // May not equal headerEnd due to parsing
                
                // Get content length
                contentLength = m_currentRequest.contentLength();
                
                // Remove headers from buffer
                buffer.erase(buffer.begin(), buffer.begin() + headerEnd);
                
                if (contentLength > 0) {
                    state = State::READING_BODY;
                } else {
                    // No body, we're done
                    m_stats.bytesReceived += buffer.size() + headerEnd;
                    ++m_stats.requestsReceived;
                    return true;
                }
            }
        }
        
        // Read body if needed
        if (state == State::READING_BODY && buffer.size() >= contentLength) {
            m_currentRequest.setBody(std::move(buffer));
            m_stats.bytesReceived += m_currentRequest.contentLength();
            ++m_stats.requestsReceived;
            return true;
        }
        
        // Limit buffer size to prevent memory exhaustion
        if (buffer.size() > m_maxRequestSize) {
            return false;
        }
    }
    
    // Handle case where we have partial data
    if (!buffer.empty()) {
        m_stats.bytesReceived += buffer.size();
        ++m_stats.requestsReceived;
        m_currentRequest.parse(buffer);
        return true;
    }
    
    return false;
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