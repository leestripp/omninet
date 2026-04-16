// OmniNet HTTP Request Tests
#include "http/HttpRequest.h"
#include <iostream>
#include <vector>

using namespace OmniNet::HTTP;

bool test_parse_simple_get() {
    const char* raw = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";
    
    Request req;
    bool result = req.parse(std::string(raw));
    
    if (!result) return false;
    if (req.method() != Method::GET) return false;
    if (req.path() != "/") return false;
    if (req.version() != "HTTP/1.1") return false;
    
    return true;
}

bool test_parse_get_with_query() {
    const char* raw = 
        "GET /search?q=test&page=1 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";
    
    Request req;
    bool result = req.parse(std::string(raw));
    
    if (!result) return false;
    if (req.path() != "/search") return false;
    
    // Check query string was parsed
    auto& query = req.query();
    if (query.params.empty()) return false;
    
    return true;
}

bool test_parse_post_with_body() {
    const char* raw = 
        "POST /submit HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "name=John&age=30";
    
    Request req;
    bool result = req.parse(std::string(raw));
    
    if (!result) return false;
    if (req.method() != Method::POST) return false;
    if (req.path() != "/submit") return false;
    if (!req.hasBody()) return false;
    
    std::string body = req.bodyAsString();
    if (body != "name=John&age=30") return false;
    
    return true;
}

bool test_parse_headers() {
    const char* raw = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "X-Custom-Header: value\r\n"
        "Accept: */*\r\n"
        "\r\n";
    
    Request req;
    bool result = req.parse(std::string(raw));
    
    if (!result) return false;
    
    auto host = req.header("Host");
    if (!host || *host != "localhost") return false;
    
    auto custom = req.header("X-Custom-Header");
    if (!custom || *custom != "value") return false;
    
    auto accept = req.header("Accept");
    if (!accept || *accept != "*/*") return false;
    
    return true;
}

bool test_parse_keep_alive() {
    const char* raw = 
        "GET / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    Request req;
    bool result = req.parse(std::string(raw));
    
    if (!result) return false;
    if (!req.keepAlive()) return false;
    
    return true;
}

int main() {
    std::cout << "Running HTTP Request tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_parse_simple_get()) {
        std::cout << "[PASS] Parse simple GET request" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Parse simple GET request" << std::endl;
        failed++;
    }
    
    if (test_parse_get_with_query()) {
        std::cout << "[PASS] Parse GET with query string" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Parse GET with query string" << std::endl;
        failed++;
    }
    
    if (test_parse_post_with_body()) {
        std::cout << "[PASS] Parse POST with body" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Parse POST with body" << std::endl;
        failed++;
    }
    
    if (test_parse_headers()) {
        std::cout << "[PASS] Parse headers (case-insensitive)" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Parse headers (case-insensitive)" << std::endl;
        failed++;
    }
    
    if (test_parse_keep_alive()) {
        std::cout << "[PASS] Parse keep-alive connection" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Parse keep-alive connection" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "HTTP Request tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}