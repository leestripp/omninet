// OmniNet - HTTP Connection Implementation
#include "HttpConnection.h"

#include <iostream>

// Debug logging
#define DEBUG_CONNECTION 1
#if DEBUG_CONNECTION
#define CONN_DEBUG(msg) do { \
    std::cerr << "[HttpConnection] " << msg << std::endl; \
} while(0)
#else
#define CONN_DEBUG(msg) do {} while(0)
#endif

namespace OmniNet::HTTP {

// ============================================================================
// Connection
// ============================================================================

Connection::Connection(std::unique_ptr<Socket> socket)
    : m_socket(std::move(socket)) {}

Connection::~Connection() = default;

bool Connection::handleNextRequest() {
    CONN_DEBUG("handleNextRequest() called");
    
    // Read request
    if (!readRequest()) {
        CONN_DEBUG("readRequest() returned false - no request data");
        return false;
    }
    
    CONN_DEBUG("readRequest() succeeded - path: " << m_currentRequest.path());
    
    // Find handler and execute
    if (m_handler) {
        CONN_DEBUG("Calling handler for path: " << m_currentRequest.path());
        Response resp;
        try {
            m_handler(m_currentRequest, resp);
        } catch (const std::exception& e) {
            CONN_DEBUG("Handler threw exception: " << e.what());
            handleError(std::current_exception());
        }
        
        CONN_DEBUG("Sending response, status=" << static_cast<int>(resp.status()));
        // Send response
        return sendResponse(resp);
    }
    
    CONN_DEBUG("No handler set!");
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
    CONN_DEBUG("readRequest() starting");
    
    // Read until we have complete headers (double CRLF)
    // Then optionally read body based on Content-Length
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);
    
    enum class State { READING_HEADERS, READING_BODY };
    State state = State::READING_HEADERS;
    size_t contentLength = 0;
    bool haveSeenDoubleCrlf = false;
    
    while (m_socket->isOpen()) {
        CONN_DEBUG("  Reading chunk, buffer size=" << buffer.size());
        
        // Read available data
        std::vector<uint8_t> chunk = m_socket->recvAll(65536);
        
        if (chunk.empty()) {
            CONN_DEBUG("  recvAll returned empty - connection closed or timeout");
            // Connection closed or timeout
            if (buffer.empty()) {
                CONN_DEBUG("  Buffer empty, returning false");
                return false;
            }
            CONN_DEBUG("  Have partial data, breaking");
            break;
        }
        
        CONN_DEBUG("  Received " << chunk.size() << " bytes");
        buffer.insert(buffer.end(), chunk.begin(), chunk.end());
        
        // Check if we have complete headers
        if (state == State::READING_HEADERS) {
            // Look for double CRLF (end of headers)
            for (size_t i = 0; i + 3 < buffer.size(); ++i) {
                if (buffer[i] == '\r' && buffer[i+1] == '\n' && 
                    buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                    haveSeenDoubleCrlf = true;
                    CONN_DEBUG("  Found double CRLF at position " << i);
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
                
                CONN_DEBUG("  Header end at position " << headerEnd);
                
                // Parse headers
                std::string headerStr(buffer.begin(), buffer.begin() + headerEnd);
                CONN_DEBUG("  Parsing headers: " << headerStr.size() << " bytes");
                size_t consumed = m_currentRequest.parse(headerStr);
                CONN_DEBUG("  Parse consumed " << consumed << " bytes");
                (void)consumed; // May not equal headerEnd due to parsing
                
                // Get content length
                contentLength = m_currentRequest.contentLength();
                CONN_DEBUG("  Content-Length: " << contentLength);
                
                // Remove headers from buffer
                buffer.erase(buffer.begin(), buffer.begin() + headerEnd);
                
                if (contentLength > 0) {
                    state = State::READING_BODY;
                    CONN_DEBUG("  Need to read " << contentLength << " bytes of body");
                } else {
                    // No body, we're done
                    m_stats.bytesReceived += buffer.size() + headerEnd;
                    ++m_stats.requestsReceived;
                    CONN_DEBUG("  No body needed, returning true");
                    return true;
                }
            } else {
                CONN_DEBUG("  No double CRLF found yet, buffer size=" << buffer.size());
            }
        }
        
        // Read body if needed
        if (state == State::READING_BODY && buffer.size() >= contentLength) {
            CONN_DEBUG("  Body complete, setting body of " << buffer.size() << " bytes");
            m_currentRequest.setBody(std::move(buffer));
            m_stats.bytesReceived += m_currentRequest.contentLength();
            ++m_stats.requestsReceived;
            return true;
        }
        
        // Limit buffer size to prevent memory exhaustion
        if (buffer.size() > m_maxRequestSize) {
            CONN_DEBUG("  Buffer too large, returning false");
            return false;
        }
    }
    
    // Handle case where we have partial data
    if (!buffer.empty()) {
        CONN_DEBUG("  Partial data, parsing " << buffer.size() << " bytes");
        m_stats.bytesReceived += buffer.size();
        ++m_stats.requestsReceived;
        m_currentRequest.parse(buffer);
        return true;
    }
    
    CONN_DEBUG("  Returning false (no data)");
    return false;
}

bool Connection::sendResponse(const Response& resp) {
    CONN_DEBUG("sendResponse() called, status=" << static_cast<int>(resp.status()));
    
    auto data = resp.buildFull();
    CONN_DEBUG("buildFull returned " << data.size() << " bytes");
    
    if (data.empty()) {
        CONN_DEBUG("buildFull returned empty data!");
        return false;
    }
    
    // Print first few bytes of response for debugging
    std::string preview(data.begin(), data.begin() + std::min(size_t(200), data.size()));
    CONN_DEBUG("Response preview: " << preview);
    
    CONN_DEBUG("Calling socket->sendAll()");
    try {
        m_socket->sendAll(data);
        CONN_DEBUG("sendAll() succeeded");
    } catch (const std::exception& e) {
        CONN_DEBUG("sendAll() failed: " << e.what());
        return false;
    }
    
    m_stats.bytesSent += data.size();
    ++m_stats.requestsProcessed;
    
    // Check for connection close
    m_keepAlive = m_currentRequest.keepAlive();
    if (resp.header("Connection") == "close") {
        m_keepAlive = false;
        CONN_DEBUG("Response has Connection: close, setting keepAlive=false");
    }
    
    CONN_DEBUG("sendResponse() returning " << m_keepAlive << ", keepAlive=" << m_keepAlive);
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