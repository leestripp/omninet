// OmniNet - Router Implementation
#include "Router.h"
#include "StaticHandler.h"

#include <algorithm>
#include <iostream>

// Debug logging - set to 0 to disable verbose debug output
#define DEBUG_ROUTER 0
#if DEBUG_ROUTER
#define ROUTER_DEBUG(msg) do { \
    std::cerr << "[Router] " << msg << std::endl; \
} while(0)
#else
#define ROUTER_DEBUG(msg) do {} while(0)
#endif

namespace OmniNet::HTTP {

// ============================================================================
// Router
// ============================================================================

Router::Router() = default;

void Router::addRoute(Method method, std::string pattern, Handler handler) {
    Route route;
    route.method = method;
    route.pattern = pattern;
    route.regex = std::regex(patternToRegex(pattern, route.paramNames), std::regex::ECMAScript);
    route.handler = std::move(handler);
    m_routes.push_back(std::move(route));
}

void Router::get(std::string pattern, Handler handler) {
    addRoute(Method::GET, std::move(pattern), std::move(handler));
}

void Router::post(std::string pattern, Handler handler) {
    addRoute(Method::POST, std::move(pattern), std::move(handler));
}

void Router::put(std::string pattern, Handler handler) {
    addRoute(Method::PUT, std::move(pattern), std::move(handler));
}

void Router::delete_(std::string pattern, Handler handler) {
    addRoute(Method::DELETE, std::move(pattern), std::move(handler));
}

void Router::patch(std::string pattern, Handler handler) {
    addRoute(Method::PATCH, std::move(pattern), std::move(handler));
}

void Router::head(std::string pattern, Handler handler) {
    addRoute(Method::HEAD, std::move(pattern), std::move(handler));
}

void Router::options(std::string pattern, Handler handler) {
    addRoute(Method::OPTIONS, std::move(pattern), std::move(handler));
}

void Router::any(std::string pattern, Handler handler) {
    auto h = std::move(handler);
    for (int m = static_cast<int>(Method::GET); m <= static_cast<int>(Method::TRACE); ++m) {
        addRoute(static_cast<Method>(m), pattern, h);
    }
}

void Router::group(std::string prefix, std::function<void(Router&)> configure) {
    // Create a sub-router for the group
    Router subRouter;
    
    // Let the callback configure the sub-router
    configure(subRouter);
    
    // Copy routes from sub-router, prepending the prefix
    for (auto& route : subRouter.m_routes) {
        // Combine prefix with the route pattern
        // Ensure single slash between prefix and pattern
        std::string combinedPattern;
        if (!prefix.empty() && prefix.back() == '/') {
            // Prefix ends with /, remove leading / from pattern if present
            if (!route.pattern.empty() && route.pattern.front() == '/') {
                combinedPattern = prefix + route.pattern.substr(1);
            } else {
                combinedPattern = prefix + route.pattern;
            }
        } else if (!route.pattern.empty() && route.pattern.front() == '/') {
            // No trailing slash on prefix, pattern starts with /
            combinedPattern = prefix + route.pattern;
        } else if (!prefix.empty()) {
            // No trailing slash on prefix, pattern doesn't start with /
            combinedPattern = prefix + "/" + route.pattern;
        } else {
            combinedPattern = route.pattern;
        }
        
        // Add the route with the combined pattern
        addRoute(route.method, combinedPattern, route.handler);
    }
    
    // Copy middleware from sub-router
    for (auto& mw : subRouter.m_middleware) {
        m_middleware.push_back(std::move(mw));
    }
    
    // Copy path-specific middleware
    for (auto& [pathPrefix, mwList] : subRouter.m_pathMiddleware) {
        std::string combinedPrefix;
        if (!prefix.empty() && prefix.back() == '/') {
            combinedPrefix = prefix + pathPrefix;
        } else if (!prefix.empty()) {
            combinedPrefix = prefix + "/" + pathPrefix;
        } else {
            combinedPrefix = pathPrefix;
        }
        m_pathMiddleware[combinedPrefix].insert(
            m_pathMiddleware[combinedPrefix].end(),
            mwList.begin(),
            mwList.end()
        );
    }
}

void Router::use(Middleware middleware) {
    m_middleware.push_back(std::move(middleware));
}

void Router::use(std::string pathPrefix, Middleware middleware) {
    m_pathMiddleware[std::move(pathPrefix)].push_back(std::move(middleware));
}

std::optional<Router::Match> Router::match(Method method, std::string_view path) {
    std::string pathStr(path);
    
    ROUTER_DEBUG("match() - Trying to match " << pathStr << " with method " << toString(method));
    
    for (const auto& route : m_routes) {
        ROUTER_DEBUG("  Checking route: " << toString(route.method) << " " << route.pattern);
        
        if (route.method != method && route.method != Method::UNKNOWN) {
            ROUTER_DEBUG("    Method mismatch, skipping");
            continue;
        }
        
        ROUTER_DEBUG("    Method matches, testing regex...");
        std::smatch match;
        bool matched = std::regex_match(pathStr, match, route.regex);
        ROUTER_DEBUG("    regex_match result: " << (matched ? "MATCH" : "NO MATCH"));
        
        if (matched) {
            Match result;
            result.handler = route.handler;
            
            // Extract captured groups
            for (size_t i = 1; i < match.size(); ++i) {
                result.captures.push_back(match[i].str());
            }
            
            ROUTER_DEBUG("    MATCHED! captures=" << result.captures.size());
            return result;
        }
    }
    
    ROUTER_DEBUG("match() - No route matched");
    return std::nullopt;
}

void Router::serveStatic(std::string urlPrefix, std::string directory) {
    // Store directory before it's moved
    std::string dirCopy = directory;
    
    // Use StaticHandler for serving static files
    std::shared_ptr<StaticHandler> handler(new StaticHandler(dirCopy));
    
    // Register a catch-all route for the URL prefix
    std::string prefixCopy = std::move(urlPrefix);
    
    get(".*", [handler, prefixCopy](const Request& req, Response& resp) {
        // Get the URL path and strip the prefix if needed
        std::string urlPath(req.path());
        
        // Remove the prefix from the URL path for the handler
        if (!prefixCopy.empty()) {
            if (urlPath.rfind(prefixCopy, 0) == 0) {
                urlPath = urlPath.substr(prefixCopy.length());
            }
        }
        
        // Ensure path starts with /
        if (urlPath.empty()) {
            urlPath = "/";
        } else if (urlPath[0] != '/') {
            urlPath = "/" + urlPath;
        }
        
        // Serve the static file
        auto& mutableReq = const_cast<Request&>(req);
        handler->handle(mutableReq, resp);
    });
}

void Router::setDefaultFile(std::string filename) {
    (void)filename;
}

void Router::allowDirectoryListing(bool allow) {
    (void)allow;
    // Directory listing is handled by StaticHandler
}

void Router::setNotFoundHandler(Handler handler) {
    m_notFoundHandler = std::move(handler);
}

void Router::setErrorHandler(Handler handler) {
    m_errorHandler = std::move(handler);
}

Handler Router::notFoundHandler() {
    return m_notFoundHandler;
}

Handler Router::errorHandler() {
    return m_errorHandler;
}

void Router::handle(const Request& req, Response& resp) {
    ROUTER_DEBUG("handle() called - Method: " << toString(req.method()) << ", Path: " << req.path());
    
    // List all registered routes
    ROUTER_DEBUG("Registered routes: " << m_routes.size());
    for (const auto& r : m_routes) {
        ROUTER_DEBUG("  " << toString(r.method) << " " << r.pattern);
    }
    
    // Run middleware
    for (const auto& mw : m_middleware) {
        ROUTER_DEBUG("Running middleware...");
        if (!mw(req, resp)) {
            ROUTER_DEBUG("Middleware returned false, stopping");
            return;
        }
    }
    
    // Find matching route
    ROUTER_DEBUG("Looking for matching route...");
    auto matchResult = match(req.method(), req.path());
    if (matchResult) {
        ROUTER_DEBUG("Route MATCHED! Calling handler");
        // Set path params
        Request& mutableReq = const_cast<Request&>(req);
        mutableReq.setPathParams(matchResult->captures);
        
        // Call handler
        matchResult->handler(req, resp);
        ROUTER_DEBUG("Handler returned, response status=" << static_cast<int>(resp.status()));
    } else {
        ROUTER_DEBUG("Route NOT MATCHED");
        if (m_notFoundHandler) {
            ROUTER_DEBUG("Calling notFoundHandler");
            m_notFoundHandler(req, resp);
        } else {
            ROUTER_DEBUG("No notFoundHandler, returning 404");
            resp = Response::notFound();
        }
    }
}

std::vector<Route> Router::routes() const {
    return m_routes;
}

void Router::setCaseInsensitive(bool value) {
    m_caseInsensitive = value;
}

void Router::setRedirectTrailingSlash(bool value) {
    m_redirectTrailingSlash = value;
}

std::string Router::patternToRegex(std::string pattern, std::vector<std::string>& paramNames) {
    std::string regex;
    
    size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == ':') {
            // Path parameter
            size_t start = i + 1;
            size_t end = start;
            while (end < pattern.size() && 
                   (std::isalnum(pattern[end]) || pattern[end] == '_')) {
                ++end;
            }
            
            if (end > start) {
                paramNames.push_back(pattern.substr(start, end - start));
                regex += "([^/]+)";
                i = end;
            } else {
                regex += ":";
                ++i;
            }
        } else if (pattern[i] == '*') {
            // Wildcard * matches everything (greedy) - used for catch-all routes
            // This should be registered AFTER specific routes in the router
            regex += ".*";
            ++i;
        } else {
            regex += pattern[i];
            ++i;
        }
    }
    
    return regex;
}

} // namespace OmniNet::HTTP