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
    
    auto match = router.match(Method::GET, "/notfound");
    if (!match) return false;
    
    Request req;
    Response resp;
    match->handler(req, resp);
    
    if (resp.status() != 404) return false;
    
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
    Response resp;
    match->handler(req, resp);
    
    if (captured.size() != 3) return false;
    if (captured[0] != "v1") return false;
    if (captured[1] != "users") return false;
    if (captured[2] != "456") return false;
    
    return true;
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
    
    std::cout << std::endl;
    std::cout << "Router tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}