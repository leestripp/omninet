// OmniNet - HTTP Connection Implementation
#include "HttpConnection.h"

#include <algorithm>
#include <iostream>

#include "net/Socket.h"
#include "utils/Exception.h"

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
        } catch (const SocketException& e) {
            CONN_DEBUG("Handler threw SocketException: " << e.what());
            m_socket->close();
            return false;
        } catch (const std::exception& e) {
            CONN_DEBUG("Handler threw exception: " << e.what());
            // If error handler is set, let it produce a response
            if (m_errorHandler) {
                m_errorHandler(m_currentRequest, resp, std::current_exception());
            } else {
                resp = Response::internalError(e.what());
            }
        } catch (...) {
            CONN_DEBUG("Handler threw unknown exception type");
            if (m_errorHandler) {
                m_errorHandler(m_currentRequest, resp, std::current_exception());
            } else {
                resp = Response::internalError("Unknown error occurred");
            }
        }
        
        CONN_DEBUG("Sending response, status=" << static_cast<int>(resp.status()));
        // Send response
        bool keepAlive = sendResponse(resp);

        // v0.7.0 (prompt.txt Bug #2 + #5): post-upgrade handoff.
        // If the handler marked the response as a protocol switch
        // (e.g. 101 Switching Protocols for WebSocket) AND a
        // post-upgrade callback was registered, release the socket
        // and hand it to the callback. The Connection stops
        // processing more HTTP requests on this connection.
        // v0.8.3 (MonkeyByte Bug 1, SSE streaming): same handoff
        // for `isStreaming()` — a 200 OK with Transfer-Encoding:
        // chunked. The post-upgrade callback owns the socket and
        // writes SSE chunks until the client disconnects.
        if ((resp.isProtocolSwitch() || resp.isStreaming()) && m_postUpgradeCallback) {
            CONN_DEBUG((resp.isStreaming() ? "Streaming: " : "Protocol switch: ")
                       << "handing socket to post-upgrade callback");
            auto cb = std::move(m_postUpgradeCallback);
            auto sock = releaseSocket();
            cb(std::move(sock), m_currentRequest);
            return false;  // socket is now owned by the callback
        }

        return keepAlive;
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
    // v0.7.0 fix: also apply to socket so a slow client can't block
    // the connection thread forever on the very first request.
    // If keepAliveTimeout was set first, it's shorter and wins.
    if (m_socket && m_keepAliveTimeout.count() == 0) {
        m_socket->setRecvTimeout(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }
}

void Connection::setKeepAliveTimeout(std::chrono::seconds timeout) {
    m_keepAliveTimeout = timeout;
    // v0.7.0 fix: apply the timeout to the socket so recv() doesn't
    // block forever in keep-alive mode. Without this, the server's
    // recv() in readRequest() never wakes up when the client closes
    // the socket, causing the test binary to hang at server.stop().
    if (m_socket) {
        m_socket->setRecvTimeout(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
    }
}

void Connection::setMaxRequestSize(size_t maxSize) {
    m_maxRequestSize = maxSize;
}

// v0.7.0 (prompt.txt Bug #2 + #5): register a post-upgrade
// handoff. After writing the next response, if it's a protocol
// switch (101), the socket is released and passed to this
// callback instead of looping back to read more HTTP requests.
void Connection::setPostUpgradeCallback(PostUpgradeCallback cb) {
    m_postUpgradeCallback = std::move(cb);
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

Socket* Connection::socketPtr() {
    return m_socket.get();
}

std::unique_ptr<Socket> Connection::releaseSocket() {
    return std::move(m_socket);
}

const Connection::Stats& Connection::stats() const {
    return m_stats;
}

bool Connection::readRequest() {
    CONN_DEBUG("readRequest() starting");
    
    // Safety limit - prevent unbounded buffer growth
    static constexpr size_t SAFETY_HEADER_LIMIT = 1024 * 1024;  // 1MB for headers
    static constexpr size_t SAFETY_BODY_LIMIT = 10 * 1024 * 1024; // 10MB floor for body
    // Effective body cap: follows the configured m_maxRequestSize when that
    // is larger (servers accepting big uploads, e.g. audio), else the floor.
    const size_t bodyLimit = std::max(m_maxRequestSize, SAFETY_BODY_LIMIT);
    
    // Read until we have complete headers (double CRLF)
    // Then optionally read body based on Content-Length or Transfer-Encoding
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);
    
    enum class State { READING_HEADERS, READING_BODY, READING_CHUNKED };
    State state = State::READING_HEADERS;
    size_t contentLength = 0;
    bool haveSeenDoubleCrlf = false;
    
    // Track total received for overflow detection
    size_t totalReceived = 0;
    
    while (m_socket->isOpen()) {
        CONN_DEBUG("  Reading chunk, buffer size=" << buffer.size() << ", totalReceived=" << totalReceived);
        
        // Safety check: prevent infinite loop from small increments
        if (totalReceived > m_maxRequestSize * 2) {
            CONN_DEBUG("  ERROR: Total received exceeds 2x maxRequestSize, possible attack");
            return false;
        }
        
        // Read available data with explicit size limit
        size_t chunkSize = std::min(size_t(65536), m_maxRequestSize);
        std::vector<uint8_t> chunk;
        try {
            chunk = m_socket->recvAll(chunkSize);
        } catch (const SocketException& e) {
            CONN_DEBUG("  recvAll threw SocketException: " << e.what());
            return false;
        } catch (const std::exception& e) {
            CONN_DEBUG("  recvAll threw unexpected exception: " << e.what());
            return false;
        }
        
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
        
        // Safety: check if adding this chunk would exceed limits
        if (state == State::READING_HEADERS) {
            // Still reading headers - check against header limit
            if (buffer.size() + chunk.size() > SAFETY_HEADER_LIMIT) {
                CONN_DEBUG("  ERROR: Headers would exceed safety limit (" << SAFETY_HEADER_LIMIT << " bytes)");
                return false;
            }
        } else {
            // Reading body
            if (buffer.size() + chunk.size() > bodyLimit) {
                CONN_DEBUG("  ERROR: Body would exceed safety limit (" << bodyLimit << " bytes)");
                return false;
            }
        }
        
        buffer.insert(buffer.end(), chunk.begin(), chunk.end());
        totalReceived += chunk.size();
        
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
                
                // Check for chunked encoding
                if (m_currentRequest.isChunked()) {
                    state = State::READING_CHUNKED;
                    CONN_DEBUG("  Transfer-Encoding: chunked detected");
                } else if (contentLength > 0) {
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
        
        // Read chunked body
        if (state == State::READING_CHUNKED) {
            // Chunked transfer encoding format:
            // <size in hex>\r\n
            // <data>\r\n
            // ...
            // 0\r\n
            // \r\n

            // Accumulator for chunked body data (v0.6.0 fix: was discarded
            // by the previous code, which only set the body on the final
            // 0-chunk using the entire remaining buffer).
            std::vector<uint8_t> chunkedBody;

            // Process all chunks currently in the buffer.
            // The outer loop will read more data if we don't have the
            // final 0-chunk yet.
            while (state == State::READING_CHUNKED) {
                // Look for end of current chunk ( CRLF )
                size_t crlfPos = 0;
                bool foundCrlf = false;
                for (size_t i = 0; i + 1 < buffer.size(); ++i) {
                    if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
                        crlfPos = i;
                        foundCrlf = true;
                        break;
                    }
                }

                if (!foundCrlf) {
                    // Need more data to find the chunk size line
                    break;
                }

                // Try to parse chunk size
                std::string sizeStr(buffer.begin(), buffer.begin() + crlfPos);
                size_t parsedChunkSize = 0;
                try {
                    parsedChunkSize = std::stoull(sizeStr, nullptr, 16);
                } catch (...) {
                    // Invalid chunk size
                    CONN_DEBUG("  Invalid chunk size, returning false");
                    return false;
                }

                CONN_DEBUG("  Chunk size: " << parsedChunkSize);

                if (parsedChunkSize == 0) {
                    // Final chunk - set accumulated body and finish.
                    m_currentRequest.setBody(std::move(chunkedBody));
                    ++m_stats.requestsReceived;
                    return true;
                }

                // Need at least: size line + CRLF + chunk data + CRLF
                size_t neededSize = crlfPos + 2 + parsedChunkSize + 2;
                if (buffer.size() < neededSize) {
                    // Not enough data for this chunk's payload yet
                    break;
                }

                // Extract chunk data (skip size line and trailing CRLF)
                // and ACCUMULATE it into chunkedBody
                chunkedBody.insert(
                    chunkedBody.end(),
                    buffer.begin() + crlfPos + 2,
                    buffer.begin() + crlfPos + 2 + parsedChunkSize
                );
                buffer.erase(buffer.begin(), buffer.begin() + neededSize);

                CONN_DEBUG("  Processed chunk of " << parsedChunkSize
                          << " bytes (total=" << chunkedBody.size() << ")");
            }
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

    // Check for gzip compression
    // Note: resp.compressBody is non-const, so we work with a copy
    Response finalResp = resp;
    auto acceptEncoding = m_currentRequest.header("Accept-Encoding");
    if (acceptEncoding) {
        std::string ae(*acceptEncoding);
        if (finalResp.compressBody(ae)) {
            CONN_DEBUG("Response compressed: " << resp.bodySize()
                      << " -> " << finalResp.bodySize() << " bytes");
        }
    }

    auto data = finalResp.buildFull();
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
    if (finalResp.header("Connection") == "close") {
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