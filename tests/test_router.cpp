// OmniNet Router Tests
#include "http/Router.h"
#include <iostream>

using namespace OmniNet::HTTP;

bool test_static_route() {
    Router router;
    bool called = false;
    
    router.get("/test", [&called](const Request& req, Response& resp) {
        (void)req;
        (void)resp;
        called = true;
    });
    
    auto match = router.match(Method::GET, "/test");
    if (!match) return false;
    if (!match->handler) return false;
    
    // Call the handler
    Request req;
    Response resp;
    match->handler(req, resp);
    
    return called;
}

bool test_parameter_route() {
    Router router;
    std::string capturedId;
    
    router.get("/users/:id", [&capturedId](const Request& req, Response& resp) {
        (void)resp;
        capturedId = std::string(req.pathParam(0));
    });
    
    auto match = router.match(Method::GET, "/users/123");
    if (!match) return false;
    
    Request req;
    req.setPathParams(match->captures);  // Router::handle() does this before dispatch
    Response resp;
    match->handler(req, resp);
    
    if (capturedId != "123") return false;
    
    return true;
}

bool test_404_handler() {
    Router router;
    
    router.get("/exists", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
    });
    
    router.setNotFoundHandler([](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::NOT_FOUND);
    });
    
    // No route matches /notfound; handle() must invoke the notFoundHandler.
    if (router.match(Method::GET, "/notfound")) return false;
    
    Request req;
    req.parse("GET /notfound HTTP/1.1\r\nHost: test\r\n\r\n");
    Response resp;
    router.handle(req, resp);
    
    if (resp.status() != StatusCode::NOT_FOUND) return false;
    
    return true;
}

bool test_method_mismatch() {
    Router router;
    
    router.get("/test", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
    });
    
    // POST to a GET-only route should not match
    auto match = router.match(Method::POST, "/test");
    if (match) return false;  // Should not match
    
    return true;
}

bool test_multiple_params() {
    Router router;
    std::vector<std::string> captured;
    
    router.get("/api/:version/:resource/:id", [&captured](const Request& req, Response& resp) {
        (void)resp;
        captured.push_back(std::string(req.pathParam(0)));
        captured.push_back(std::string(req.pathParam(1)));
        captured.push_back(std::string(req.pathParam(2)));
    });
    
    auto match = router.match(Method::GET, "/api/v1/users/456");
    if (!match) return false;
    
    Request req;
    req.setPathParams(match->captures);  // Router::handle() does this before dispatch
    Response resp;
    match->handler(req, resp);
    
    if (captured.size() != 3) return false;
    if (captured[0] != "v1") return false;
    if (captured[1] != "users") return false;
    if (captured[2] != "456") return false;
    
    return true;
}


// ============================================================================
// Bug #5 regression tests — trailing-slash redirect, both directions
// ============================================================================

namespace {

Request makeRequest(const char* requestLine) {
    Request req;
    std::string raw = requestLine;
    raw += " HTTP/1.1\r\nHost: test\r\n\r\n";
    req.parse(raw);
    return req;
}

// Returns true if resp is a redirect to expectedLocation with expectedStatus.
bool checkRedirect(const Response& resp, StatusCode expectedStatus,
                   const char* expectedLocation) {
    if (resp.status() != expectedStatus) return false;
    auto loc = resp.header("Location");
    if (!loc) return false;
    return *loc == expectedLocation;
}

} // namespace


// ============================================================================
// Bug #6 regression tests — catch-all route must not cause redirect ping-pong
//
// These replicate lscppdev.com's real setup: concrete routes PLUS a /*
// catch-all (logging 404 handler).  v1.0.2 looped forever here because the
// redirect probes counted the wildcard match.  followRedirects() simulates a
// real client (curl -L) so a loop CANNOT slip through again.
// ============================================================================

namespace {

// Simulates a client following redirects. Returns the final status code,
// or -1 if the redirect limit is exceeded (i.e. a redirect loop).
int followRedirects(Router& router, const std::string& method, std::string target,
                    int maxRedirects = 10) {
    for (int i = 0; i <= maxRedirects; ++i) {
        Request req;
        req.parse(method + " " + target + " HTTP/1.1\r\nHost: test\r\n\r\n");
        Response resp;
        router.handle(req, resp);

        int status = static_cast<int>(resp.status());
        if (status != 301 && status != 307 && status != 308) {
            return status;  // terminal response
        }
        auto loc = resp.header("Location");
        if (!loc) return status;  // redirect without Location: terminal
        target = std::string(*loc);
    }
    return -1;  // redirect loop
}

// Router shaped like lscppdev.com: concrete routes + logging catch-all.
Router makeCatchallRouter() {
    Router router;
    router.get("/blog",      [](const Request&, Response& resp) { resp.setBody("blog"); });
    router.get("/blog/:slug",[](const Request&, Response& resp) { resp.setBody("post"); });
    router.post("/contact",  [](const Request&, Response& resp) { resp.setBody("thanks"); });
    router.get("/*", [](const Request&, Response& resp) {
        resp.setStatus(StatusCode::NOT_FOUND);
        resp.setBody("Not Found");
    });
    return router;
}

} // namespace

// GET /blog with a concrete /blog route: serve 200, never redirect
// (v1.0.2 answered 301 /blog/ because the catch-all "matched" the probe).
bool test_catchall_concrete_route_not_redirected() {
    Router router = makeCatchallRouter();
    Request req = makeRequest("GET /blog");
    Response resp;
    router.handle(req, resp);
    return resp.status() == StatusCode::OK && !resp.header("Location");
}

// GET /blog/ still redirects once to /blog with the catch-all present.
bool test_catchall_slash_redirects_to_concrete() {
    Router router = makeCatchallRouter();
    Request req = makeRequest("GET /blog/");
    Response resp;
    router.handle(req, resp);
    return checkRedirect(resp, StatusCode::MOVED_PERMANENTLY, "/blog");
}

// Full client behaviour: following redirects must terminate at 200/404,
// never loop.  Covers every row of the Bug #6 table.
bool test_catchall_no_redirect_loop() {
    {
        Router router = makeCatchallRouter();
        if (followRedirects(router, "GET", "/blog") != 200) return false;
    }
    {
        Router router = makeCatchallRouter();
        if (followRedirects(router, "GET", "/blog/") != 200) return false;
    }
    {
        Router router = makeCatchallRouter();
        if (followRedirects(router, "GET", "/blog/my-post") != 200) return false;
    }
    {
        Router router = makeCatchallRouter();
        if (followRedirects(router, "GET", "/blog/my-post/") != 200) return false;
    }
    {
        Router router = makeCatchallRouter();
        if (followRedirects(router, "POST", "/contact/") != 200) return false;
    }
    return true;
}

// Unknown paths (catch-all territory) must NOT be redirected at all:
// the catch-all handler serves them directly.
bool test_catchall_unknown_path_not_redirected() {
    Router router = makeCatchallRouter();
    Request req = makeRequest("GET /rss.xml");
    Response resp;
    router.handle(req, resp);
    return resp.status() == StatusCode::NOT_FOUND && !resp.header("Location");
}

bool test_catchall_unknown_slashed_path_not_redirected() {
    Router router = makeCatchallRouter();
    Request req = makeRequest("GET /rss.xml/");
    Response resp;
    router.handle(req, resp);
    return resp.status() == StatusCode::NOT_FOUND && !resp.header("Location");
}

// Redirect followed through keeps the query string end-to-end.
bool test_catchall_redirect_preserves_query_end_to_end() {
    Router router = makeCatchallRouter();

    Request req = makeRequest("GET /blog/?page=1");
    Response resp;
    router.handle(req, resp);
    if (!checkRedirect(resp, StatusCode::MOVED_PERMANENTLY, "/blog?page=1")) return false;

    // Follow it: query string survives, final answer is 200.
    Request req2 = makeRequest("GET /blog?page=1");
    Response resp2;
    router.handle(req2, resp2);
    if (resp2.status() != StatusCode::OK) return false;
    return req2.queryParameter("page") == std::optional<std::string>("1");
}

// Bug #4 claim check: the /* catch-all DOES match dot-paths like /.env
// at Router level (pattern compiles to /.* which covers them).
bool test_catchall_matches_dot_path() {
    Router router;
    router.get("/*", [](const Request&, Response& resp) { resp.setBody("caught"); });

    Request req = makeRequest("GET /.env");
    Response resp;
    router.handle(req, resp);
    return resp.status() == StatusCode::OK && resp.bodySize() > 0;
}

// Direction (a): request ends in '/', route does not -> 301 to stripped path.
bool test_trailing_slash_redirect_get() {
    Router router;
    router.get("/blog", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("blog index");
    });

    Request req = makeRequest("GET /blog/");
    Response resp;
    router.handle(req, resp);

    return checkRedirect(resp, StatusCode::MOVED_PERMANENTLY, "/blog");
}

// Direction (a) preserves the query string.
bool test_trailing_slash_redirect_preserves_query() {
    Router router;
    router.get("/blog", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("blog index");
    });

    Request req = makeRequest("GET /blog/?page=1");
    Response resp;
    router.handle(req, resp);

    return checkRedirect(resp, StatusCode::MOVED_PERMANENTLY, "/blog?page=1");
}

// Direction (a) works for parameterised routes.
bool test_trailing_slash_redirect_param_route() {
    Router router;
    router.get("/blog/:slug", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("post");
    });

    Request req = makeRequest("GET /blog/hello-world/");
    Response resp;
    router.handle(req, resp);

    return checkRedirect(resp, StatusCode::MOVED_PERMANENTLY, "/blog/hello-world");
}

// Non-GET methods get 308 so the method and body survive the redirect.
bool test_trailing_slash_redirect_post_uses_308() {
    Router router;
    router.post("/contact", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("thanks");
    });

    Request req = makeRequest("POST /contact/");
    Response resp;
    router.handle(req, resp);

    return checkRedirect(resp, StatusCode::PERMANENT_REDIRECT, "/contact");
}

// The redirect is method-aware: no POST route, so no redirect for POST.
bool test_trailing_slash_redirect_method_mismatch() {
    Router router;
    router.get("/blog", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("blog index");
    });

    Request req = makeRequest("POST /blog/");
    Response resp;
    router.handle(req, resp);

    return resp.status() == StatusCode::NOT_FOUND;
}

// Direction (b): route registered WITH a trailing slash redirects requests
// without it (now via regex match rather than raw pattern comparison).
bool test_redirect_to_slashed_route() {
    Router router;
    router.get("/docs/", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("docs");
    });

    Request req = makeRequest("GET /docs");
    Response resp;
    router.handle(req, resp);

    return checkRedirect(resp, StatusCode::MOVED_PERMANENTLY, "/docs/");
}

// Root path "/" is never redirected (would be a loop).
bool test_trailing_slash_root_not_redirected() {
    Router router;
    router.get("/", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("home");
    });

    Request req = makeRequest("GET /");
    Response resp;
    router.handle(req, resp);

    return resp.status() == StatusCode::OK && resp.bodySize() > 0;
}

// setRedirectTrailingSlash(false) disables the redirect -> 404.
bool test_trailing_slash_redirect_disabled() {
    Router router;
    router.setRedirectTrailingSlash(false);
    router.get("/blog", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("blog index");
    });

    Request req = makeRequest("GET /blog/");
    Response resp;
    router.handle(req, resp);

    return resp.status() == StatusCode::NOT_FOUND;
}

// A slashed request that matches nothing (even stripped) still 404s.
bool test_trailing_slash_no_match_404() {
    Router router;
    router.get("/blog", [](const Request& req, Response& resp) {
        (void)req;
        resp.setBody("blog index");
    });

    Request req = makeRequest("GET /nope/");
    Response resp;
    router.handle(req, resp);

    return resp.status() == StatusCode::NOT_FOUND;
}

int main() {
    std::cout << "Running Router tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_static_route()) {
        std::cout << "[PASS] Static route matching" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Static route matching" << std::endl;
        failed++;
    }
    
    if (test_parameter_route()) {
        std::cout << "[PASS] Parameter route matching" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Parameter route matching" << std::endl;
        failed++;
    }
    
    if (test_404_handler()) {
        std::cout << "[PASS] 404 handler" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] 404 handler" << std::endl;
        failed++;
    }
    
    if (test_method_mismatch()) {
        std::cout << "[PASS] Method mismatch" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Method mismatch" << std::endl;
        failed++;
    }
    
    if (test_multiple_params()) {
        std::cout << "[PASS] Multiple parameter route" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Multiple parameter route" << std::endl;
        failed++;
    }
    

#define RUN_ROUTER_TEST(fn, label) \
    if (fn()) { \
        std::cout << "[PASS] " << label << std::endl; \
        passed++; \
    } else { \
        std::cout << "[FAIL] " << label << std::endl; \
        failed++; \
    }

    RUN_ROUTER_TEST(test_trailing_slash_redirect_get, "Trailing-slash redirect (GET /blog/ -> 301 /blog)");
    RUN_ROUTER_TEST(test_trailing_slash_redirect_preserves_query, "Trailing-slash redirect preserves query string");
    RUN_ROUTER_TEST(test_trailing_slash_redirect_param_route, "Trailing-slash redirect on parameterised route");
    RUN_ROUTER_TEST(test_trailing_slash_redirect_post_uses_308, "Trailing-slash redirect uses 308 for POST");
    RUN_ROUTER_TEST(test_trailing_slash_redirect_method_mismatch, "Trailing-slash redirect is method-aware");
    RUN_ROUTER_TEST(test_redirect_to_slashed_route, "Redirect to route registered with trailing slash");
    RUN_ROUTER_TEST(test_trailing_slash_root_not_redirected, "Root path / is not redirected");
    RUN_ROUTER_TEST(test_trailing_slash_redirect_disabled, "Redirect disabled via setRedirectTrailingSlash(false)");
    RUN_ROUTER_TEST(test_trailing_slash_no_match_404, "Slashed path with no matching route still 404s");

    RUN_ROUTER_TEST(test_catchall_concrete_route_not_redirected, "Bug#6: concrete route served, not redirected (catch-all present)");
    RUN_ROUTER_TEST(test_catchall_slash_redirects_to_concrete, "Bug#6: GET /blog/ redirects once to /blog (catch-all present)");
    RUN_ROUTER_TEST(test_catchall_no_redirect_loop, "Bug#6: following redirects always terminates (no ping-pong)");
    RUN_ROUTER_TEST(test_catchall_unknown_path_not_redirected, "Bug#6: unknown path hits catch-all, no redirect");
    RUN_ROUTER_TEST(test_catchall_unknown_slashed_path_not_redirected, "Bug#6: unknown slashed path hits catch-all, no redirect");
    RUN_ROUTER_TEST(test_catchall_redirect_preserves_query_end_to_end, "Bug#6: redirect chain preserves query string");
    RUN_ROUTER_TEST(test_catchall_matches_dot_path, "Bug#4: /* catch-all matches dot-paths like /.env");
#undef RUN_ROUTER_TEST

    std::cout << std::endl;
    std::cout << "Router tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}