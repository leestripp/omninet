// OmniNet - HTTP Router
#ifndef OMNINET_HTTP_ROUTER_H
#define OMNINET_HTTP_ROUTER_H

#include <functional>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include <nlohmann/json.hpp>

namespace OmniNet::HTTP {

// ============================================================================
// Route Handler Type
// ============================================================================

using Handler = std::function<void(const Request&, Response&)>;

// v0.7.0 (Phase 11e): JSON handler — body is auto-parsed; 400 on bad JSON.
// The application doesn't need to call Json::parse() themselves.
using JsonHandler = std::function<void(const nlohmann::json&, Response&)>;

// ============================================================================
// Route Definition
// ============================================================================

struct Route {
    Method method;
    std::string pattern;
    std::regex regex;
    std::vector<std::string> paramNames;
    Handler handler;
    std::string middleware;
};

// ============================================================================
// Router
// ============================================================================

class Router {
public:
    Router();
    
    // --- Route Registration ---
    
    // Register route for specific method
    void addRoute(Method method, std::string pattern, Handler handler);
    
    // Convenience methods
    void get(std::string pattern, Handler handler);
    void post(std::string pattern, Handler handler);
    void put(std::string pattern, Handler handler);
    void delete_(std::string pattern, Handler handler);
    void patch(std::string pattern, Handler handler);
    void head(std::string pattern, Handler handler);
    void options(std::string pattern, Handler handler);
    
    // Any method
    void any(std::string pattern, Handler handler);

    // v0.7.0 (Phase 11e): JSON handler overloads. Body is auto-parsed;
    // 400 Bad Request is returned on parse failure before the user's
    // handler is called.
    void get(std::string pattern, JsonHandler handler);
    void post(std::string pattern, JsonHandler handler);
    void put(std::string pattern, JsonHandler handler);
    void delete_(std::string pattern, JsonHandler handler);
    void patch(std::string pattern, JsonHandler handler);
    void head(std::string pattern, JsonHandler handler);
    void options(std::string pattern, JsonHandler handler);
    void any(std::string pattern, JsonHandler handler);
    
    // Group routes with shared prefix
    void group(std::string prefix, std::function<void(Router&)> configure);
    
    // --- Middleware ---
    
    // Add middleware (runs before handler)
    using Middleware = std::function<bool(const Request&, Response&)>;
    void use(Middleware middleware);
    
    // Add middleware for specific path prefix
    void use(std::string pathPrefix, Middleware middleware);
    
    // --- Matching ---
    
    struct Match {
        Handler handler;
        std::vector<std::string> params;
        std::vector<std::string> captures;
    };
    
    std::optional<Match> match(Method method, std::string_view path);
    
    // --- Static Files ---
    
    // Serve static files from a directory
    void serveStatic(std::string urlPrefix, std::string directory);
    
    // Set default file for directory requests
    void setDefaultFile(std::string filename);
    
    // Allow directory listing
    void allowDirectoryListing(bool allow);
    
    // --- Error Handling ---
    
    void setNotFoundHandler(Handler handler);
    void setErrorHandler(Handler handler);
    
    Handler notFoundHandler();
    Handler errorHandler();
    
    // --- Processing ---
    
    // Handle request (calls middleware, then routes)
    void handle(const Request& req, Response& resp);
    
    // --- Debugging ---
    
    // List all registered routes
    std::vector<Route> routes() const;
    
    // --- Configuration ---
    
    // Case insensitive matching
    void setCaseInsensitive(bool value);
    
    // Trailing slash handling
    void setRedirectTrailingSlash(bool value);

private:
    std::vector<Route> m_routes;
    std::vector<Middleware> m_middleware;
    std::unordered_map<std::string, std::vector<Middleware>> m_pathMiddleware;
    
    Handler m_notFoundHandler;
    Handler m_errorHandler;
    
    bool m_caseInsensitive = false;
    bool m_redirectTrailingSlash = true;

    static std::string patternToRegex(
        std::string pattern,
        std::vector<std::string>& paramNames
    );
};

// ============================================================================
// Route Guards
// ============================================================================

// Require specific HTTP method
inline bool requireMethod(const Request& req, Response& resp, Method method) {
    if (req.method() != method) {
        resp = Response::methodNotAllowed(toString(method));
        return false;
    }
    return true;
}

// Require specific content type
inline bool requireContentType(const Request& req, Response& resp,
                               std::string_view expectedType) {
    auto contentType = req.contentType();
    if (contentType.substr(0, expectedType.size()) != expectedType) {
        resp = Response::unsupportedMediaType(
            "Expected Content-Type: " + std::string(expectedType)
        );
        return false;
    }
    return true;
}

// Require minimum content length
inline bool requireContentLength(const Request& req, Response& resp, size_t min) {
    if (req.contentLength() < min) {
        resp = Response::badRequest("Content too short");
        return false;
    }
    return true;
}

} // namespace OmniNet::HTTP

#endif // OMNINET_HTTP_ROUTER_H
