// OmniNet - Router Implementation
#include "Router.h"

#include <algorithm>

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
    Router subRouter;
    configure(subRouter);
    // TODO: Implement grouping with prefix
}

void Router::use(Middleware middleware) {
    m_middleware.push_back(std::move(middleware));
}

void Router::use(std::string pathPrefix, Middleware middleware) {
    m_pathMiddleware[std::move(pathPrefix)].push_back(std::move(middleware));
}

std::optional<Router::Match> Router::match(Method method, std::string_view path) {
    std::string pathStr(path);
    
    for (const auto& route : m_routes) {
        if (route.method != method && route.method != Method::UNKNOWN) {
            continue;
        }
        
        std::smatch match;
        if (std::regex_match(pathStr, match, route.regex)) {
            Match result;
            result.handler = route.handler;
            
            // Extract captured groups
            for (size_t i = 1; i < match.size(); ++i) {
                result.captures.push_back(match[i].str());
            }
            
            return result;
        }
    }
    
    return std::nullopt;
}

void Router::serveStatic(std::string urlPrefix, std::string directory) {
    // TODO: Implement static file serving
    (void)urlPrefix;
    (void)directory;
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
    // Run middleware
    for (const auto& mw : m_middleware) {
        if (!mw(req, resp)) {
            return;
        }
    }
    
    // Find matching route
    auto matchResult = match(req.method(), req.path());
    if (matchResult) {
        // Set path params
        Request& mutableReq = const_cast<Request&>(req);
        mutableReq.setPathParams(matchResult->captures);
        
        // Call handler
        matchResult->handler(req, resp);
    } else if (m_notFoundHandler) {
        m_notFoundHandler(req, resp);
    } else {
        resp = Response::notFound();
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