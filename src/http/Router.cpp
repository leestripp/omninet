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

void Router::addRoute(Method method, const std::string& pattern, Handler handler) {
    Route route;
    route.method = method;
    route.pattern = pattern;
    
    // Convert pattern to regex
    std::string regexPattern = patternToRegex(pattern, route.paramNames);
    
    // Apply case-insensitive flag if enabled
    std::regex::flag_type flags = std::regex::ECMAScript;
    if (m_caseInsensitive) {
        flags |= std::regex::icase;
    }
    route.regex = std::regex(regexPattern, flags);
    route.handler = std::move(handler);
    
    // Check for duplicate route registration
    for (const auto& existing : m_routes) {
        if (existing.method == method && existing.pattern == pattern) {
            std::cerr << "[Router] WARNING: Duplicate route registered: " 
                      << toString(method) << " " << pattern << std::endl;
            std::cerr << "[Router]   The first handler will be used, ignoring duplicate." << std::endl;
            return;
        }
    }
    
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

// v0.7.0 (Phase 11e): JSON handler overloads.
// The user's JsonHandler is wrapped in a plain Handler that parses
// the body once; on failure, returns 400 and the user handler is
// NOT called.
static Handler wrapJsonHandler(JsonHandler jh) {
    auto h = std::move(jh);
    return [h = std::move(h)](const Request& req, Response& resp) {
        try {
            auto body = req.bodyAsString();
            auto parsed = nlohmann::json::parse(body);
            h(parsed, resp);
        } catch (const nlohmann::json::exception& e) {
            resp = Response::badRequest(
                std::string("Invalid JSON: ") + e.what());
        } catch (...) {
            resp = Response::badRequest("Invalid request body");
        }
    };
}

void Router::get(std::string pattern, JsonHandler handler) {
    addRoute(Method::GET, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::post(std::string pattern, JsonHandler handler) {
    addRoute(Method::POST, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::put(std::string pattern, JsonHandler handler) {
    addRoute(Method::PUT, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::delete_(std::string pattern, JsonHandler handler) {
    addRoute(Method::DELETE, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::patch(std::string pattern, JsonHandler handler) {
    addRoute(Method::PATCH, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::head(std::string pattern, JsonHandler handler) {
    addRoute(Method::HEAD, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::options(std::string pattern, JsonHandler handler) {
    addRoute(Method::OPTIONS, std::move(pattern), wrapJsonHandler(std::move(handler)));
}

void Router::any(const std::string& pattern, Handler handler) {
    auto h = std::move(handler);
    for (int m = static_cast<int>(Method::GET); m <= static_cast<int>(Method::TRACE); ++m) {
        addRoute(static_cast<Method>(m), pattern, h);
    }
}

void Router::group(const std::string& prefix, std::function<void(Router&)> configure) {
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
    return matchImpl(method, path, false);
}

bool Router::hasConcreteRoute(Method method, std::string_view path) {
    return matchImpl(method, path, true).has_value();
}

std::optional<Router::Match> Router::matchImpl(Method method, std::string_view path,
                                               bool concreteOnly) {
    std::string pathStr(path);
    
    ROUTER_DEBUG("match() - Trying to match " << pathStr << " with method " << toString(method));
    
    // Per HTTP spec, HEAD requests should be handled by GET handlers
    // If no explicit HEAD route exists, fall back to matching GET routes
    Method effectiveMethod = method;
    if (method == Method::HEAD) {
        // Check if there's any explicit HEAD route for this path first
        bool hasExplicitHead = false;
        for (const auto& route : m_routes) {
            if (concreteOnly && route.pattern.find('*') != std::string::npos) {
                continue;
            }
            if (route.method == Method::HEAD) {
                std::smatch routeMatch;
                if (std::regex_match(pathStr, routeMatch, route.regex)) {
                    hasExplicitHead = true;
                    break;
                }
            }
        }
        
        // If no explicit HEAD route, treat HEAD as GET
        if (!hasExplicitHead) {
            ROUTER_DEBUG("  No explicit HEAD route found, using GET handler");
            effectiveMethod = Method::GET;
        }
    }
    
    for (const auto& route : m_routes) {
        // v1.0.3 fix (bug #6): wildcard catch-all routes (e.g. /*, which
        // compiles to /.*/ and matches EVERYTHING) must not count when the
        // trailing-slash redirect logic probes whether a path "has a route"
        // — otherwise both redirect directions always succeed and undo each
        // other in an infinite 301 ping-pong.
        if (concreteOnly && route.pattern.find('*') != std::string::npos) {
            continue;
        }

        ROUTER_DEBUG("  Checking route: " << toString(route.method) << " " << route.pattern);
        
        // Match the effective method (HEAD falls back to GET, or exact match)
        if (route.method != effectiveMethod && route.method != Method::UNKNOWN) {
            ROUTER_DEBUG("    Method mismatch, skipping");
            continue;
        }
        
        ROUTER_DEBUG("    Method matches, testing regex...");
        std::smatch routeMatch;
        bool matched = std::regex_match(pathStr, routeMatch, route.regex);
        ROUTER_DEBUG("    regex_match result: " << (matched ? "MATCH" : "NO MATCH"));
        
        if (matched) {
            Match result;
            result.handler = route.handler;
            
            // Extract captured groups
            for (size_t i = 1; i < routeMatch.size(); ++i) {
                result.captures.push_back(routeMatch[i].str());
            }
            
            ROUTER_DEBUG("    MATCHED! captures=" << result.captures.size());
            return result;
        }
    }
    
    ROUTER_DEBUG("match() - No route matched");
    return std::nullopt;
}

void Router::serveStatic(std::string urlPrefix, const std::string& directory) {
    // Store directory before it's moved
    std::string dirCopy = directory;
    
    // Use StaticHandler for serving static files
    std::shared_ptr<StaticHandler> handler(new StaticHandler(dirCopy));
    
    // Register a prefix-based route instead of catch-all
    // This prevents shadowing other routes like /omninet/*
    std::string prefixCopy = std::move(urlPrefix);
    
    // Build a regex that matches paths starting with the prefix
    // e.g., prefix "/static" matches "/static", "/static/", "/static/file.js"
    std::string regexPattern = "^" + prefixCopy;
    if (!prefixCopy.empty() && prefixCopy.back() != '/') {
        // If prefix doesn't end with /, also match the prefix itself followed by optional /
        regexPattern += "(/.*)?$";
    } else {
        // If prefix ends with /, match anything starting with that prefix
        regexPattern += ".*$";
    }
    
    get(regexPattern, [handler, prefixCopy](const Request& req, Response& resp) {
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

void Router::setDefaultFile(const std::string& /*filename*/) {
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


    
    // Handle trailing slash redirect if enabled, in BOTH directions:
    //
    //  a) Request ends in '/' but the matching route does not
    //     (GET /blog/  -> 301 /blog).  This is the case real apps hit:
    //     routes are virtually never registered with a trailing slash.
    //  b) Request has no trailing slash but the matching route does
    //     (GET /docs   -> 301 /docs/).
    //
    // v1.0.2 fix (bug #5): direction (a) was missing entirely, and the
    // old direction-(b) code compared the raw pattern string
    // (route.pattern == pathWithSlash) so it could never fire for
    // parameterised patterns and ignored the request method.  Both
    // directions now test the route's compiled regex via match() for the
    // request's own method, preserve the query string (dropping it
    // silently breaks filtered API requests like /api/v1/contents?type=blog),
    // and answer non-GET/HEAD requests with 308 Permanent Redirect so the
    // method and body survive (browsers/fetch historically rewrite 301/302
    // POSTs to GET, silently dropping the body).  Redirects fire before
    // middleware by design.
    //
    // v1.0.3 fix (bug #6): all redirect decisions use hasConcreteRoute()
    // (wildcard catch-alls like /* excluded), and the whole block is skipped
    // when the request's OWN path already matches a concrete route.  v1.0.2
    // probed with match(), which counts the catch-all: GET /blog redirected
    // to /blog/ (catch-all "matched" the probe) and /blog/ redirected back
    // to /blog — an infinite 301 ping-pong (ERR_TOO_MANY_REDIRECTS).
    std::string pathStr(req.path());
    if (m_redirectTrailingSlash && pathStr.size() > 1 &&
        !hasConcreteRoute(req.method(), pathStr)) {
        auto sendRedirect = [&](std::string location) {
            std::string_view qs = req.queryString();
            if (!qs.empty()) {
                location += '?';
                location += qs;
            }
            ROUTER_DEBUG("Redirecting " << pathStr << " to " << location);
            resp.setStatus(req.method() == Method::GET || req.method() == Method::HEAD
                               ? StatusCode::MOVED_PERMANENTLY    // 301
                               : StatusCode::PERMANENT_REDIRECT); // 308 keeps method+body
            resp.setHeader("Location", std::move(location));
        };

        if (pathStr.back() == '/') {
            // Direction (a): strip the trailing slash and redirect if a
            // concrete route matches the stripped path for this method.
            std::string stripped = pathStr.substr(0, pathStr.size() - 1);
            if (hasConcreteRoute(req.method(), stripped)) {
                sendRedirect(std::move(stripped));
                return;
            }
        } else {
            // Direction (b): append a slash and redirect if a concrete
            // route matches the slashed path for this method.
            if (hasConcreteRoute(req.method(), pathStr + '/')) {
                sendRedirect(pathStr + '/');
                return;
            }
        }
    }
    
    // List all registered routes
    ROUTER_DEBUG("Registered routes: " << m_routes.size());
    for (const auto& r : m_routes) {
        (void)r;
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
    
    // Ensure response has a valid status code for unmatched routes
    // If a route was matched but the handler didn't set a proper response (status still OK, no body),
    // treat it as 404 Not Found to prevent ambiguous empty 200 responses.
    // v0.9.0 (prompt.txt A2A streaming): don't clobber a streaming or
    // protocol-switch response — the handler intentionally left the body
    // empty because the socket will be handed off after the headers.
    if (resp.status() == StatusCode::OK &&
        !resp.isStreaming() &&
        !resp.isProtocolSwitch()) {
        if (matchResult && resp.bodySize() == 0) {
            // Route matched but no body was set - treat as 404
            resp.setStatus(StatusCode::NOT_FOUND);
            resp.setBody("Not Found");
            ROUTER_DEBUG("Route matched but no body set, forcing 404");
        } else if (!matchResult) {
            // No route matched at all - definitely 404
            resp.setStatus(StatusCode::NOT_FOUND);
            resp.setBody("Not Found");
            ROUTER_DEBUG("No route matched, forcing 404");
        }
    }
}

const std::vector<Route>& Router::routes() const {
    return m_routes;
}

void Router::setCaseInsensitive(bool value) {
    m_caseInsensitive = value;
}

void Router::setRedirectTrailingSlash(bool value) {
    m_redirectTrailingSlash = value;
}

std::string Router::patternToRegex(const std::string& pattern, std::vector<std::string>& paramNames) {
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