# OmniNet - Full Plan

GitLab: git remote add origin git@gitlab.com:leestripp/omninet.git

## Overview

Complete OmniNet from scratch using only:
- **C++20** (no external dependencies except system libraries)
- **Unix/POSIX sockets** (no library dependencies)
- **OpenSSL** (for TLS/SSL and crypto)
- **SQLite3** (for data persistence)

**Target:** A lightweight, modular HTTP/WebSocket server library with TLS support, session management, and optional admin dashboard.

---

## Phase 1: Foundation (Core Infrastructure)

### 1.1 Build System
```
Directory Structure:
/
├── CMakeLists.txt
├── src/
│   ├── OmniNet.h              # Main include
│   ├── OmniNet.cpp            # Version info
│   ├── config/
│   │   ├── Config.h           # Configuration singleton
│   │   └── Config.cpp
│   ├── utils/
│   │   ├── Exception.h        # Base exception class
│   │   ├── Logger.h           # Logging with levels
│   │   ├── Logger.cpp
│   │   └── Utils.h            # String, encoding helpers
│   ├── crypto/
│   │   ├── Crypto.h           # OpenSSL wrapper (TLS init, etc.)
│   │   ├── Crypto.cpp
│   │   ├── Password.h         # PBKDF2 password hashing
│   │   └── Password.cpp
│   └── net/
│       ├── Socket.h           # Base socket wrapper
│       ├── Socket.cpp
│       ├── TLSSocket.h        # OpenSSL TLS socket
│       └── TLSSocket.cpp
├── tests/
└── demos/
```

**Tasks:**
- [ ] Create CMakeLists.txt with C++20, find OpenSSL, find SQLite3
- [ ] Define OMNINET_VERSION macro
- [ ] Setup include directories properly

### 1.2 Exception & Logging

**Exception.h:**
```cpp
namespace OmniNet {

class Exception : public std::exception {
public:
    explicit Exception(const std::string& msg);
    const char* what() const noexcept override;
    int code() const;  // Error code
private:
    std::string m_message;
    int m_code;
};

// Subclasses
class IOException : public Exception { ... };  // I/O errors
class SocketException : public IOException { ... };
class TLSException : public IOException { ... };
class ParseException : public Exception { ... };  // Parsing errors
class DatabaseException : public Exception { ... };
}
```

**Logger.h:**
```cpp
namespace OmniNet {

enum class LogLevel { DEBUG, INFO, WARN, ERROR, CRITICAL };

class Logger {
public:
    static Logger& instance();
    
    void log(LogLevel level, const std::string& msg);
    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);
    
    void setLevel(LogLevel level);
    void setLogFile(const std::string& path);
    
    // Formatter
    std::string format(LogLevel level, const std::string& msg);
};
}
```

**Tasks:**
- [ ] Implement Exception hierarchy with error codes
- [ ] Implement Logger singleton with file output
- [ ] Add timestamps, thread ID to log entries
- [ ] Implement log rotation (optional)

### 1.3 Utilities

**Utils.h:**
```cpp
// String trimming, splitting
std::vector<std::string> split(const std::string& str, char delim);
std::string trim(const std::string& str);

// URL encoding/decoding
std::string urlEncode(const std::string& str);
std::string urlDecode(const std::string& str);

// Base64 (OpenSSL BIO)
std::string base64Encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64Decode(const std::string& encoded);

// Hex conversion
std::string bytesToHex(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> hexToBytes(const std::string& hex);
```

**Tasks:**
- [ ] String utilities (split, trim, join)
- [ ] URL encoding/decoding (percent-encoding)
- [ ] Base64 encode/decode
- [ ] Hex conversion utilities

---

## Phase 2: Networking (Sockets & TLS)

### 2.1 Socket Base Class

**Socket.h:**
```cpp
class Socket {
public:
    Socket();
    explicit Socket(int fd);  // Wrap existing fd
    virtual ~Socket();
    
    // Non-copyable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // Movable
    Socket(Socket&&) noexcept;
    Socket& operator=(Socket&&) noexcept;
    
    // Connection
    void connect(const std::string& host, uint16_t port);
    void bind(const std::string& address, uint16_t port);
    void listen(int backlog = 128);
    std::unique_ptr<Socket> accept();
    
    // I/O
    size_t send(const void* data, size_t len);
    size_t recv(void* buffer, size_t len);
    std::string recvAll(size_t maxSize = 65536);
    
    // Options
    void setNonBlocking(bool enabled);
    void setTimeout(std::chrono::seconds timeout);
    void setReuseAddr(bool enabled);
    
    // State
    bool isOpen() const;
    void close();
    int fd() const { return m_fd; }
    
protected:
    int m_fd = -1;
    bool m_owns_fd = true;
};
```

**Tasks:**
- [ ] Implement TCP socket wrapper
- [ ] `connect()` - client connection
- [ ] `bind()` + `listen()` - server binding
- [ ] `accept()` - accept connections
- [ ] `send()` / `recv()` with timeout support
- [ ] `recvAll()` - read until buffer full or connection closed
- [ ] `setNonBlocking()` - for async I/O
- [ ] `setReuseAddr()` - SO_REUSEADDR
- [ ] Move semantics properly

### 2.2 TLS/SSL Socket

**TLSSocket.h:**
```cpp
class TLSSocket : public Socket {
public:
    // Client mode
    TLSSocket();
    
    // Server mode (from accepted socket)
    explicit TLSSocket(int accepted_fd);
    
    ~TLSSocket() override;
    
    // Configuration
    void setCertFile(const std::string& path);
    void setKeyFile(const std::string& path);
    void setCaFile(const std::string& path);  // For client cert auth
    void setVerifyPeer(bool verify);
    
    // Connection
    void connect(const std::string& host, uint16_t port) override;  // TLS handshake
    void accept() override;  // Server-side TLS handshake
    
    // State
    bool isSecure() const { return m_ssl != nullptr; }
    std::string getCipher();
    bool hasPeerCertificate();
    std::string getPeerCertificateCN();
    
private:
    void initContext(TLSMode mode);
    void cleanupContext();
    
    SSL_CTX* m_ctx = nullptr;
    SSL* m_ssl = nullptr;
    bool m_server_mode = false;
};
```

**Tasks:**
- [ ] Initialize OpenSSL globally (thread-safe)
- [ ] Implement SSL context creation
- [ ] Client: `connect()` performs TLS handshake
- [ ] Server: `accept()` performs TLS handshake
- [ ] Proper certificate verification
- [ ] Memory cleanup on destruction

### 2.3 Network Utilities

**Tasks:**
- [ ] Hostname resolution (`getaddrinfo`)
- [ ] IP parsing (IPv4/IPv6)
- [ ] CIDR matching for IP allowlisting

---

## Phase 3: HTTP Protocol

### 3.1 HTTP Request

**HttpRequest.h:**
```cpp
enum class HttpMethod {
    GET, POST, PUT, DELETE, PATCH,
    HEAD, OPTIONS, CONNECT, TRACE,
    UNKNOWN
};

class HttpRequest {
public:
    HttpRequest();
    
    // Parsing
    bool parse(const char* data, size_t len);
    bool parse(const std::string& data);
    
    // Method
    HttpMethod method() const;
    std::string methodString() const;
    
    // Path
    std::string_view path() const;
    std::string_view query() const;
    std::string pathParam(size_t index) const;  // For /users/:id style
    
    // Headers
    std::optional<std::string> header(std::string_view key) const;
    std::string contentType() const;
    size_t contentLength() const;
    bool hasBody() const;
    
    // Body
    std::string_view body() const;
    std::span<const uint8_t> bodyBinary() const;
    
    // Connection
    bool keepAlive() const;  // Check Connection header
    
private:
    HttpMethod m_method;
    std::string m_path;
    std::string m_query;
    std::string m_version;
    std::unordered_map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
    
    // Parsing helpers
    bool parseRequestLine(std::string_view line);
    bool parseHeaderLine(std::string_view line);
    bool parseBody(std::span<const uint8_t> data);
};
```

**Tasks:**
- [ ] Parse HTTP/1.1 request line
- [ ] Parse headers (case-insensitive lookup)
- [ ] Handle chunked transfer encoding
- [ ] Parse query string parameters
- [ ] Support URL-encoded body (POST form data)
- [ ] Detect keep-alive from Connection header

### 3.2 HTTP Response

**HttpResponse.h:**
```cpp
class HttpResponse {
public:
    HttpResponse();
    
    // Status
    void setStatus(int code);  // Auto-set reason phrase
    int status() const;
    std::string reasonPhrase() const;
    
    // Headers
    void setHeader(std::string_view key, std::string_view value);
    void setContentType(std::string_view type);
    void setContentLength(size_t len);
    void setConnectionClose();
    void setLocation(std::string_view uri);  // For redirects
    
    // Body
    void setBody(std::string_view data);
    void setBody(std::span<const uint8_t> data);
    std::span<const uint8_t> body() const;
    
    // File serving
    void sendFile(const std::string& path);
    bool hasFile() const;
    
    // Build raw HTTP response
    std::vector<uint8_t> build() const;
    
private:
    int m_status = 200;
    std::unordered_map<std::string, std::string> m_headers;
    std::vector<uint8_t> m_body;
    std::optional<std::string> m_file_path;
    
    static std::string statusReason(int code);
};
```

**Tasks:**
- [ ] Implement status code to reason phrase mapping
- [ ] `build()` serializes to HTTP/1.1 wire format
- [ ] Support chunked transfer encoding for large bodies
- [ ] Support Range requests for file serving

### 3.3 HTTP Connection Handler

**HttpConnection.h:**
```cpp
class HttpConnection {
public:
    HttpConnection(std::unique_ptr<Socket> socket);
    ~HttpConnection();
    
    // Process incoming request(s)
    // Returns true if connection should stay open (keep-alive)
    bool handleNextRequest();
    
    // Set request handler
    using Handler = std::function<void(const HttpRequest&, HttpResponse&)>;
    void setHandler(Handler handler);
    
    // Timeout settings
    void setTimeout(std::chrono::seconds timeout);
    
    // Close connection
    void close();
    
private:
    bool readRequest();
    bool sendResponse(const HttpResponse& resp);
    
    std::unique_ptr<Socket> m_socket;
    Handler m_handler;
    std::chrono::seconds m_timeout;
    
    // Buffer for reading
    std::vector<uint8_t> m_buffer;
    size_t m_buffer_pos = 0;
};
```

**Tasks:**
- [ ] Read until complete HTTP request received
- [ ] Handle pipelined requests (keep-alive)
- [ ] Timeout on slow clients
- [ ] Handle connection errors gracefully

---

## Phase 4: HTTP Server

### 4.1 Router

**Router.h:**
```cpp
class Router {
public:
    // Register routes
    void get(std::string pattern, Handler handler);
    void post(std::string pattern, Handler handler);
    void put(std::string pattern, Handler handler);
    void delete_(std::string pattern, Handler handler);  // delete is keyword
    void patch(std::string pattern, Handler handler);
    void any(std::string pattern, Handler handler);  // Any method
    
    // Serve static files
    void serveStatic(std::string urlPrefix, std::string directory);
    
    // Match incoming request
    struct Match {
        Handler handler;
        std::vector<std::string> params;  // Captured path segments
    };
    std::optional<Match> match(HttpMethod method, std::string_view path);
    
private:
    struct Route {
        HttpMethod method;
        std::string pattern;
        std::regex regex;
        Handler handler;
    };
    std::vector<Route> m_routes;
    
    // Parse pattern like /users/:id into regex
    static std::string patternToRegex(std::string pattern);
};
```

**Tasks:**
- [ ] Pattern matching with path parameters (`/users/:id`)
- [ ] Static file serving with MIME types
- [ ] Directory listing (optional)
- [ ] 404 handler for unmatched routes

### 4.2 HTTP Server

**HttpServer.h:**
```cpp
class HttpServer {
public:
    struct Config {
        std::string address = "0.0.0.0";
        uint16_t port = 8080;
        size_t threadCount = std::thread::hardware_concurrency();
        std::chrono::seconds requestTimeout = 30s;
        std::chrono::seconds keepAliveTimeout = 5s;
        size_t maxRequestSize = 1024 * 1024;  // 1MB
        
        // TLS
        bool tlsEnabled = false;
        std::string tlsCertFile;
        std::string tlsKeyFile;
        
        // CORS
        bool corsEnabled = false;
        std::string corsAllowedOrigins;
        std::string corsAllowedMethods;
        std::string corsAllowedHeaders;
    };
    
    HttpServer(const Config& config = Config());
    ~HttpServer();
    
    // Non-copyable
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    // Start/stop
    void start();
    void stop();
    bool isRunning() const;
    
    // Routes
    Router& router() { return m_router; }
    void route(HttpMethod method, std::string pattern, Handler handler);
    void get(std::string pattern, Handler handler);
    void post(std::string pattern, Handler handler);
    void put(std::string pattern, Handler handler);
    void delete_(std::string pattern, Handler handler);
    
    // Middleware (before/after handlers)
    void use(Middleware middleware);  // Runs on all requests
    
    // Error handlers
    void setNotFoundHandler(Handler handler);
    void setErrorHandler(Handler handler);
    
    // Stats
    struct Stats {
        std::atomic<uint64_t> requestsTotal{0};
        std::atomic<uint64_t> requestsActive{0};
        std::atomic<uint64_t> errors{0};
    };
    const Stats& stats() const { return m_stats; }
    
private:
    void runAcceptLoop();
    void handleConnection(std::unique_ptr<Socket> socket);
    void handleRequest(const HttpRequest& req, HttpResponse& resp);
    
    Config m_config;
    Router m_router;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_running{false};
    std::unique_ptr<Socket> m_listenSocket;
    
    // Middleware chain
    std::vector<Middleware> m_middleware;
    
    // Error handlers
    Handler m_notFoundHandler;
    Handler m_errorHandler;
    
    Stats m_stats;
};
```

**Tasks:**
- [ ] `start()` - bind and listen
- [ ] Accept loop in main thread
- [ ] Dispatch connections to worker threads
- [ ] `stop()` - graceful shutdown with timeout
- [ ] Thread pool management

### 4.3 Static File Serving

**StaticHandler.h:**
```cpp
class StaticHandler {
public:
    explicit StaticHandler(std::string rootDir);
    
    void handle(const HttpRequest& req, HttpResponse& resp);
    
    // Configuration
    void setDefaultFile(std::string filename);  // "index.html"
    void setIndexFiles(std::vector<std::string> files);
    void allowDirectoryListing(bool allow);
    
    // MIME types
    static std::string mimeType(std::string_view path);
    
private:
    std::string m_rootDir;
    std::vector<std::string> m_indexFiles = {"index.html", "index.htm"};
    bool m_allowDirectoryListing = false;
    
    std::string resolvePath(std::string_view urlPath);
    bool isHidden(std::string_view path);
    HttpResponse serveFile(const std::string& realPath);
    HttpResponse serveDirectory(const std::string& dirPath);
    HttpResponse serveIndex(const std::string& dirPath);
};
```

**Tasks:**
- [ ] Map URL to filesystem path (with traversal protection)
- [ ] MIME type detection from extension
- [ ] ETag generation for caching
- [ ] Range request support
- [ ] Directory listing (HTML)

---

## Phase 5: WebSocket Protocol

### 5.1 WebSocket Frame

**WebSocketFrame.h:**
```cpp
enum class WebSocketOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

struct WebSocketFrame {
    bool fin = true;
    WebSocketOpcode opcode;
    std::vector<uint8_t> payload;
    
    bool isText() const { return opcode == WebSocketOpcode::TEXT; }
    bool isBinary() const { return opcode == WebSocketOpcode::BINARY; }
    bool isClose() const { return opcode == WebSocketOpcode::CLOSE; }
    bool isPing() const { return opcode == WebSocketOpcode::PING; }
    bool isPong() const { return opcode == WebSocketOpcode::PONG; }
};

class WebSocketFrameCodec {
public:
    // Encode
    static std::vector<uint8_t> encode(const WebSocketFrame& frame);
    
    // Decode (handles fragmentation)
    static std::optional<WebSocketFrame> decode(std::span<const uint8_t> data, size_t& bytesConsumed);
};
```

**Tasks:**
- [ ] Encode frames (masked for client, unmasked for server)
- [ ] Decode frames (validate masking key)
- [ ] Handle fragmentation (multi-frame messages)
- [ ] Handle control frames (PING/PONG/CLOSE)

### 5.2 WebSocket Handshake

**WebSocketHandshake.h:**
```cpp
class WebSocketHandshake {
public:
    // Server: generate accept key from client key
    static bool processClientRequest(
        std::string_view request,
        std::string& responseKey,
        std::string& error
    );
    
    // Client: validate server response
    static bool validateServerResponse(
        std::string_view response,
        std::string_view clientKey
    );
    
    // Generate WebSocket URL for client
    static std::string makeUrl(bool secure, std::string host, uint16_t port, std::string path);
    
private:
    static std::string computeAcceptKey(const std::string& clientKey);
    static std::string generateNonce();
};
```

**Tasks:**
- [ ] Parse client's upgrade request (verify headers)
- [ ] Generate Sec-WebSocket-Accept response key
- [ ] Client: validate server's response

### 5.3 WebSocket Connection

**WebSocket.h:**
```cpp
class WebSocket {
public:
    // Server: wrap accepted socket after HTTP upgrade
    WebSocket(std::unique_ptr<Socket> socket, std::string_view resource);
    
    // Client: connect and perform handshake
    WebSocket(const std::string& url);  // ws:// or wss://
    
    ~WebSocket();
    
    // Send messages
    void sendText(std::string message);
    void sendBinary(std::span<const uint8_t> data);
    void sendPing(std::span<const uint8_t> payload = {});
    
    // Receive messages
    // Returns nullopt if closed
    std::optional<WebSocketFrame> receive();
    
    // Close
    void close(uint16_t code = 1000, std::string reason = {});
    
    // State
    bool isOpen() const;
    std::string remoteAddress() const;
    
    // Callbacks
    using TextHandler = std::function<void(std::string)>;
    using BinaryHandler = std::function<void(std::span<const uint8_t>)>;
    using CloseHandler = std::function<void(uint16_t, std::string)>;
    using ErrorHandler = std::function<void(std::error_code)>;
    
    void onText(TextHandler handler);
    void onBinary(BinaryHandler handler);
    void onClose(CloseHandler handler);
    void onError(ErrorHandler handler);
    
private:
    void handleFrame(const WebSocketFrame& frame);
    void sendFrame(const WebSocketFrame& frame);
    void handleControlFrame(const WebSocketFrame& frame);
    
    std::unique_ptr<Socket> m_socket;
    std::string m_resource;
    std::atomic<bool> m_open{false};
    bool m_is_server;
    
    // Callbacks
    TextHandler m_textHandler;
    BinaryHandler m_binaryHandler;
    CloseHandler m_closeHandler;
    ErrorHandler m_errorHandler;
    
    // Fragmentation state
    std::optional<WebSocketFrame> m_fragments;
};
```

**Tasks:**
- [ ] Server-side: receive HTTP upgrade request
- [ ] Client-side: connect, send upgrade request
- [ ] Send/receive text and binary messages
- [ ] Handle fragmentation
- [ ] Auto-respond to PING with PONG
- [ ] Close handshake (opcode 0x8)

### 5.4 WebSocket Server

**WebSocketServer.h:**
```cpp
class WebSocketServer : private HttpServer::Handler {
public:
    WebSocketServer(const HttpServer::Config& config);
    
    // Register WebSocket endpoint
    // handler is called for each new connection
    void on(std::string path, 
            std::function<void(WebSocket& ws)> handler);
    
    void start();
    void stop();
    
private:
    void operator()(const HttpRequest& req, HttpResponse& resp) override;
    
    struct Endpoint {
        std::string path;
        std::function<void(WebSocket&)> handler;
    };
    std::vector<Endpoint> m_endpoints;
    HttpServer m_http;
};
```

**Tasks:**
- [ ] Register upgrade handler on HttpServer
- [ ] Match URL path to WebSocket endpoint
- [ ] Perform handshake
- [ ] Create WebSocket object and invoke handler

---

## Phase 6: Data Persistence (SQLite)

### 6.1 Database Core

**Database.h:**
```cpp
class Database {
public:
    Database();
    ~Database();
    
    // Open/close
    void open(const std::string& path);
    void close();
    bool isOpen() const;
    
    // Transaction support
    void transaction(std::function<void()> fn);
    
    // Execute (for CREATE, INSERT, UPDATE, DELETE)
    void execute(const std::string& sql);
    
    // Query (for SELECT) - with callback for each row
    using RowCallback = std::function<bool(const std::vector<std::string>& columns)>;
    void query(const std::string& sql, RowCallback callback);
    
    // Prepared statements
    class Statement {
    public:
        void bind(int index, const std::string& value);
        void bind(int index, int64_t value);
        void bind(int index, double value);
        void bind(int index, std::nullopt_t);
        
        bool step();  // Move to next row
        std::string text(int column);
        int64_t integer(int column);
        double real(int column);
        bool isNull(int column);
        
        void reset();
    };
    
    Statement prepare(const std::string& sql);
    
private:
    sqlite3* m_db = nullptr;
};
```

**Tasks:**
- [ ] SQLite connection management
- [ ] Transaction support (BEGIN/COMMIT/ROLLBACK)
- [ ] Prepared statement creation
- [ ] Parameter binding (string, int64, double, null)
- [ ] Result extraction

### 6.2 Session Store

**SessionStore.h:**
```cpp
class SessionStore {
public:
    SessionStore(Database& db);
    
    // Create new session, returns session ID
    std::string create(std::string userId, 
                       std::chrono::seconds ttl = std::chrono::hours(24));
    
    // Get session data
    struct Session {
        std::string id;
        std::string userId;
        std::chrono::system_clock::time_point created;
        std::chrono::system_clock::time_point expires;
        std::string data;  // JSON payload
    };
    std::optional<Session> get(const std::string& sessionId);
    
    // Delete session
    void remove(const std::string& sessionId);
    
    // Touch (extend expiry)
    void touch(const std::string& sessionId);
    
    // Cleanup expired
    void cleanup();
    
private:
    Database& m_db;
};
```

**Tasks:**
- [ ] Sessions table with expiry index
- [ ] Secure random session ID generation
- [ ] TTL enforcement
- [ ] Periodic cleanup of expired sessions

### 6.3 User Store (for dashboard)

**UserStore.h:**
```cpp
class UserStore {
public:
    struct User {
        int64_t id;
        std::string username;
        std::string passwordHash;  // PBKDF2
        std::string passwordSalt;
        std::string role;  // "admin" or "user"
        std::chrono::system_clock::time_point created;
        std::optional<std::chrono::system_clock::time_point> lastLogin;
    };
    
    UserStore(Database& db);
    
    // Create user (hash password automatically)
    int64_t createUser(const std::string& username, 
                       const std::string& password,
                       const std::string& role = "user");
    
    // Verify password
    std::optional<User> verify(const std::string& username, 
                               const std::string& password);
    
    // Get user by ID
    std::optional<User> getById(int64_t id);
    
    // List all users
    std::vector<User> list();
    
    // Delete user
    void remove(int64_t id);
    
    // Update user role
    void setRole(int64_t id, const std::string& role);
    
private:
    Database& m_db;
    PasswordHasher m_hasher;
};
```

**Tasks:**
- [ ] Users table with indexes
- [ ] PBKDF2 password hashing using Crypto/Password
- [ ] Admin role for first user (special handling)
- [ ] Session association

---

## Phase 7: Dashboard (Admin UI)

### 7.1 Dashboard Router

**DashboardHandler.h:**
```cpp
class DashboardHandler {
public:
    struct Config {
        std::string urlPath = "/omninet";
        std::string apiPath = "/omninet/api";
        std::string dbPath;  // Empty = ~/.omninet/dashboard.db
        std::vector<std::string> allowedIpPrefixes = {"127.", "::1"};
        int sessionTtlSeconds = 3600;
    };
    
    explicit DashboardHandler(const Config& config);
    
    // Mount on HttpServer
    void mount(HttpServer& server);
    
    // Or, for integration with existing server
    void handleRequest(const HttpRequest& req, HttpResponse& resp);
    
private:
    Config m_config;
    SessionStore m_sessions;
    UserStore m_users;
    StatsStore m_stats;  // Simple key-value stats
    
    // Route handlers
    void handleLogin(const HttpRequest& req, HttpResponse& resp);
    void handleLogout(const HttpRequest& req, HttpResponse& resp);
    void handleGetUsers(const HttpRequest& req, HttpResponse& resp);
    void handleCreateUser(const HttpRequest& req, HttpResponse& resp);
    void handleDeleteUser(const HttpRequest& req, HttpResponse& resp);
    void handleGetStats(const HttpRequest& req, HttpResponse& resp);
    void handleRecordStat(const HttpRequest& req, HttpResponse& resp);
    void serveDashboardPage(const HttpRequest& req, HttpResponse& resp);
    
    // Auth helpers
    std::optional<std::string> extractSessionId(const HttpRequest& req);
    std::optional<User> getAuthenticatedUser(const HttpRequest& req);
    void requireAdmin(const HttpRequest& req);
    
    // HTML generation
    std::string renderLoginPage();
    std::string renderDashboardPage(const User& user);
};
```

**Tasks:**
- [ ] Login/logout endpoints
- [ ] User management (CRUD)
- [ ] Auth middleware for protected endpoints
- [ ] First-user-becomes-admin logic
- [ ] Login rate limiting

### 7.2 Login Rate Limiting

**RateLimiter.h:**
```cpp
class RateLimiter {
public:
    // Block after maxAttempts within windowSeconds
    explicit RateLimiter(size_t maxAttempts, 
                        std::chrono::seconds windowSeconds);
    
    // Check if action is allowed
    bool allow(const std::string& key);
    
    // Record failed attempt
    void recordFailure(const std::string& key);
    
    // Record success, clears failures
    void recordSuccess(const std::string& key);
    
    // Check if blocked
    bool isBlocked(const std::string& key);
    
    // Get remaining attempts
    size_t remainingAttempts(const std::string& key);

private:
    struct Attempt {
        size_t count;
        std::chrono::steady_clock::time_point windowStart;
    };
    
    size_t m_maxAttempts;
    std::chrono::seconds m_windowSeconds;
    std::unordered_map<std::string, Attempt> m_attempts;
    std::mutex m_mutex;
};
```

**Tasks:**
- [ ] In-memory rate limiting (per-process)
- [ ] Sliding window counter
- [ ] Auto-reset after window expires

### 7.3 Dashboard HTML

**Dashboard HTML generation (single file):**
- Login page: username/password form
- Dashboard: stats cards, user table, logout
- GitHub dark mode styling
- Inline CSS (no external dependencies)

**Tasks:**
- [ ] Login page with error messages
- [ ] Dashboard with:
  - Stats cards (requests, connections, etc.)
  - User table (admin can add/delete)
  - Logout button
- [ ] Responsive dark theme

---

## Phase 8: Server-Sent Events (SSE)

### 8.1 SSE Handler

**SSEHandler.h:**
```cpp
class SSEHandler {
public:
    struct Event {
        std::string id;      // Event ID
        std::string event;   // Event type
        std::string data;    // Event data
        
        std::string format() const;  // Format as SSE wire format
    };
    
    SSEHandler(const std::string& path);
    
    // Send event to all connected clients
    void broadcast(const Event& event);
    
    // Subscribe/unsubscribe callbacks
    using Subscriber = std::function<void()>;
    void onSubscribe(Subscriber fn);
    void onUnsubscribe(Subscriber fn);
    
    void handleRequest(const HttpRequest& req, HttpResponse& resp);

private:
    std::string m_path;
    std::mutex m_clientsMutex;
    std::vector<std::weak_ptr<Client>> m_clients;
    std::vector<Subscriber> m_subscribeCallbacks;
    std::vector<Subscriber> m_unsubscribeCallbacks;
    
    struct Client {
        std::function<bool(const std::string&)> send;
        std::atomic<bool> connected{true};
    };
};
```

**Tasks:**
- [ ] HTTP endpoint that returns text/event-stream
- [ ] Keep-alive comments (heartbeat every 30s)
- [ ] Broadcast to all subscribers
- [ ] Handle client disconnect

---

## Phase 9: Intrusion Detection

### 9.1 Intrusion Detector

**IntrusionDetector.h:**
```cpp
class IntrusionDetector {
public:
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string clientIp;
        std::string method;
        std::string path;
        std::string reason;  // Why flagged
    };
    
    IntrusionDetector(Database& db);
    
    // Check request, return true if blocked
    // Also logs suspicious activity
    bool checkAndBlock(const std::string& clientIp,
                       const std::string& method,
                       const std::string& path);
    
    // Get recent logs
    std::vector<LogEntry> getRecentLogs(size_t limit = 100);
    
    // Get blocked IPs
    std::vector<std::string> getBlockedIPs();
    
    // Unblock IP
    void unblockIP(const std::string& ip);
    
    // Clear logs
    void clearLogs();
    
    // Detection rules
    void addRule(std::string name, std::regex pattern);
    void setAutoBlock(bool enabled);
    void setBlockDuration(std::chrono::minutes duration);

private:
    bool matchesRules(const std::string& method, const std::string& path);
    void blockIP(const std::string& ip);
    
    Database& m_db;
    std::vector<std::pair<std::string, std::regex>> m_rules;
    std::mutex m_blockedMutex;
    std::unordered_set<std::string> m_blockedCache;
    std::chrono::minutes m_blockDuration{30};
    bool m_autoBlock = true;
};
```

**Tasks:**
- [ ] Predefined rules for common attacks:
  - SQL injection patterns
  - Path traversal (`../`)
  - Common exploit paths (`/admin`, `/.env`, `/config`)
- [ ] In-memory block cache
- [ ] Block duration and cleanup
- [ ] Logging to database

---

## Phase 10: Testing

### 10.1 Test Structure

```
tests/
├── CMakeLists.txt
├── test_main.cpp
├── test_utils.cpp
├── test_socket.cpp
├── test_tls.cpp
├── test_http_request.cpp
├── test_http_response.cpp
├── test_router.cpp
├── test_websocket.cpp
├── test_database.cpp
├── test_session.cpp
├── test_password.cpp
├── test_dashboard.cpp
└── test_integration.cpp
```

### 10.2 Test Categories

**Unit Tests:**
- [ ] Utils (string, URL encoding)
- [ ] HTTP Request parsing
- [ ] HTTP Response building
- [ ] Router pattern matching
- [ ] WebSocket frame encoding/decoding

**Integration Tests:**
- [ ] Socket connect/disconnect
- [ ] TLS handshake
- [ ] HTTP request/response cycle
- [ ] WebSocket echo
- [ ] Database CRUD

**End-to-End Tests:**
- [ ] Start server, make HTTP requests
- [ ] WebSocket client/server
- [ ] Session creation and validation

---

## Phase 11: Demos

### 11.1 Demo Applications

```
demos/
├── CMakeLists.txt
├── http_server.cpp        # Basic HTTP server
├── https_server.cpp       # HTTPS with self-signed cert
├── websocket_echo.cpp     # WebSocket echo server
├── websocket_chat.cpp     # Multi-user chat
├── sse_clock.cpp          # SSE time stream
├── static_files.cpp       # Static file server
├── dashboard.cpp          # Admin dashboard
└── load_test.cpp          # HTTP load testing tool
```

### 11.2 Demo: HTTP Server

```cpp
#include "omnet/OmniNet.h"

using namespace OmniNet;

int main() {
    HttpServer server;
    server.get("/hello", [](const HttpRequest& req, HttpResponse& resp) {
        resp.setStatus(200);
        resp.setContentType("text/plain");
        resp.setBody("Hello, World!");
    });
    server.get("/time", [](const HttpRequest& req, HttpResponse& resp) {
        auto now = std::chrono::system_clock::now();
        resp.setBody(std::to_string(
            std::chrono::system_clock::to_time_t(now)
        ));
    });
    
    server.start();
    std::this_thread::sleep_for(std::chrono::hours(24));
    server.stop();
}
```

---

## Implementation Order (Dependency Graph)

```
Week 1: Foundation
├── Build system (CMake)
├── Exception & Logging
└── Utils (string, encoding)

Week 2: Networking
├── Socket (TCP)
├── TLSSocket (OpenSSL)
└── Network utilities

Week 3: HTTP Protocol
├── HttpRequest parsing
├── HttpResponse building
└── HttpConnection handler

Week 4: HTTP Server
├── Router
├── HttpServer
└── Static file serving

Week 5: WebSocket
├── Frame codec
├── Handshake
├── WebSocket connection
└── WebSocket server

Week 6: Database & Auth
├── Database core
├── Session store
├── Password hashing
└── User store

Week 7: Dashboard
├── Dashboard handler
├── Rate limiter
├── Login/logout flows
└── Dashboard HTML

Week 8: SSE & Intrusion
├── SSE handler
├── Intrusion detector
└── Blocking logic

Week 9: Testing
├── Unit tests
├── Integration tests
└── Fix bugs

Week 10: Demos & Polish
├── Demo applications
├── Documentation
└── Performance tuning
```

---

## Key Design Decisions

### 1. No External HTTP Libraries
- Parse HTTP manually for full control
- No libcurl, no Boost.Asio
- Use POSIX sockets directly

### 2. Single-Threaded Server Option
- `HttpServer` can run single-threaded for simplicity
- Thread pool is optional (configurable)
- Reduces complexity for low-traffic servers

### 3. Modular Headers
- Each class in its own `.h` file
- No "god headers" that include everything
- Users can include only what they need

### 4. No Exceptions in Hot Paths
- Parse errors return `std::optional` or error codes
- Exceptions reserved for truly exceptional cases
- `std::expected<T, E>` (C++23) - use `std::optional` for now

### 5. Move Semantics Everywhere
- `Socket` wraps fd, moves cleanly
- `HttpRequest` uses `std::vector<uint8_t>` for body
- Avoid unnecessary copies in I/O paths

### 6. Configuration via Struct
- `HttpServer::Config` struct for all settings
- No global config singleton
- Each component gets config in constructor

---

## File Count Estimate

**Core Library (~40 files):**
- 3 foundation (exception, logger, utils)
- 2 crypto (crypto, password)
- 3 network (socket, tls socket, utils)
- 2 HTTP request/response
- 1 HTTP connection
- 2 HTTP server + router
- 1 static handler
- 4 WebSocket (frame, handshake, connection, server)
- 2 Database (core, session)
- 1 User store
- 3 Dashboard (handler, rate limiter, HTML)
- 1 SSE handler
- 1 Intrusion detector
- 4 Config, main include, version

**Tests (~15 files):**
- One test file per component

**Demos (~8 files):**
- Simple, standalone examples

**Total: ~65 source files**

---

## Success Criteria

- [ ] Builds with single `cmake --build`
- [ ] All tests pass
- [ ] HTTP server handles basic requests
- [ ] WebSocket echo works
- [ ] TLS/HTTPS works with self-signed cert
- [ ] Dashboard login/logout works
- [ ] Session persists across restarts
- [ ] No memory leaks (valgrind)
- [ ] No compiler warnings

---

## Future Enhancements (Out of Scope for v1)

- HTTP/2 support
- WebSocket compression (permessage-deflate)
- Load balancing
- Hot reload configuration
- WebDAV support
- GraphQL endpoint
- Metrics endpoint (Prometheus format)