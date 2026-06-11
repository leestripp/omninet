// OmniNet HTTP Response Tests
#include "http/HttpResponse.h"
#include <iostream>
#include <vector>
#include <cstring>

using namespace OmniNet;
using namespace OmniNet::HTTP;

bool test_response_status() {
    Response resp;
    resp.setStatus(StatusCode::OK);

    if (resp.statusInt() != 200) return false;

    return true;
}

bool test_response_headers() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("application/json");
    resp.setHeader("X-Custom", std::string("value"));

    auto ct = resp.header("Content-Type");
    if (!ct || *ct != "application/json") return false;

    auto custom = resp.header("X-Custom");
    if (!custom || *custom != "value") return false;

    return true;
}

bool test_response_body() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("text/plain");
    resp.setBody("Hello, World!");

    auto body = resp.body();
    std::string bodyStr(reinterpret_cast<const char*>(body.data()), body.size());

    if (bodyStr != "Hello, World!") return false;

    return true;
}

bool test_response_build() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("text/plain");
    resp.setBody("Test");

    // buildFull() includes the body, so "Test" should be in the response
    auto data = resp.buildFull();

    // Check it starts with "HTTP/1.1 200"
    std::string response(reinterpret_cast<const char*>(data.data()), data.size());

    if (response.find("HTTP/1.1 200") == std::string::npos) return false;
    if (response.find("Content-Type: text/plain") == std::string::npos) return false;
    if (response.find("Content-Length: 4") == std::string::npos) return false;  // v0.6.0: auto Content-Length
    if (response.find("Test") == std::string::npos) return false;

    return true;
}

// v0.6.0 Content-Length tests
bool test_response_auto_content_length_empty_body() {
    // Empty body responses must have Content-Length: 0 so HTTP/1.1
    // keep-alive clients don't hang waiting for data (v0.6.0 fix)
    Response resp = Response::unauthorized("Invalid credentials");
    auto data = resp.build();
    std::string response(reinterpret_cast<const char*>(data.data()), data.size());
    if (response.find("Content-Length: 0") == std::string::npos) return false;
    return true;
}

bool test_response_auto_content_length_with_body() {
    // Content-Length is auto-set to body size when not explicit
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setBody("Hello, world!");
    auto data = resp.build();
    std::string response(reinterpret_cast<const char*>(data.data()), data.size());
    if (response.find("Content-Length: 13") == std::string::npos) return false;
    return true;
}

bool test_response_explicit_content_length_preserved() {
    // Explicit Content-Length is not overridden
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setBody("Hello");
    resp.setContentLength(99);
    auto data = resp.build();
    std::string response(reinterpret_cast<const char*>(data.data()), data.size());
    if (response.find("Content-Length: 99") == std::string::npos) return false;
    return true;
}

int main() {
    std::cout << "Running HTTP Response tests..." << std::endl;

    int passed = 0;
    int failed = 0;

    if (test_response_status()) {
        std::cout << "[PASS] Response status" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Response status" << std::endl;
        failed++;
    }

    if (test_response_headers()) {
        std::cout << "[PASS] Response headers" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Response headers" << std::endl;
        failed++;
    }

    if (test_response_body()) {
        std::cout << "[PASS] Response body" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Response body" << std::endl;
        failed++;
    }

    if (test_response_build()) {
        std::cout << "[PASS] Response build" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Response build" << std::endl;
        failed++;
    }

    if (test_response_auto_content_length_empty_body()) {
        std::cout << "[PASS] Auto Content-Length: 0 for empty body" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Auto Content-Length: 0 for empty body" << std::endl;
        failed++;
    }

    if (test_response_auto_content_length_with_body()) {
        std::cout << "[PASS] Auto Content-Length from body size" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Auto Content-Length from body size" << std::endl;
        failed++;
    }

    if (test_response_explicit_content_length_preserved()) {
        std::cout << "[PASS] Explicit Content-Length preserved" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Explicit Content-Length preserved" << std::endl;
        failed++;
    }

    std::cout << std::endl;
    std::cout << "HTTP Response tests: " << passed << " passed, " << failed << " failed" << std::endl;

    return failed > 0 ? 1 : 0;
}