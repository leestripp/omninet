// OmniNet HTTP Response Tests
#include "http/HttpResponse.h"
#include <iostream>
#include <vector>
#include <cstring>

using namespace OmniNet::HTTP;

bool test_response_status() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    
    if (resp.status() != 200) return false;
    
    return true;
}

bool test_response_headers() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("X-Custom", "value");
    
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
    
    auto data = resp.build();
    
    // Check it starts with "HTTP/1.1 200"
    std::string response(reinterpret_cast<const char*>(data.data()), data.size());
    
    if (response.find("HTTP/1.1 200") == std::string::npos) return false;
    if (response.find("Content-Type: text/plain") == std::string::npos) return false;
    if (response.find("Test") == std::string::npos) return false;
    
    return true;
}

bool test_response_json() {
    Response resp = Response::json(R"({"key": "value"})");
    
    if (resp.status() != 200) return false;
    
    auto ct = resp.header("Content-Type");
    if (!ct || ct->find("application/json") == std::string::npos) return false;
    
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
    
    if (test_response_json()) {
        std::cout << "[PASS] Response JSON helper" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Response JSON helper" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "HTTP Response tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}