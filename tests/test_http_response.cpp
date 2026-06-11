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

#ifdef OMNINET_HAVE_ZLIB
// v0.6.0: Compression tests (Phase 10d test gaps)
bool test_response_compress_with_gzip_support() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("text/plain");
    std::string body(1024, 'a');  // compressible body
    resp.setBody(body);

    bool compressed = resp.compressBody("gzip, deflate");
    if (!compressed) return false;

    // Should now have Content-Encoding: gzip
    auto enc = resp.header("Content-Encoding");
    if (!enc || *enc != "gzip") return false;

    // The build() should have the gzip header
    auto data = resp.build();
    std::string response(reinterpret_cast<const char*>(data.data()), data.size());
    if (response.find("Content-Encoding: gzip") == std::string::npos) return false;
    return true;
}

bool test_response_no_compress_without_gzip() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setBody("Hello world");

    // "deflate" only - no gzip in Accept-Encoding
    bool compressed = resp.compressBody("deflate, br");
    if (compressed) return false;

    // No Content-Encoding header
    auto enc = resp.header("Content-Encoding");
    if (enc.has_value()) return false;
    return true;
}

bool test_response_no_compress_empty_body() {
    Response resp;
    resp.setStatus(StatusCode::OK);
    // Empty body

    bool compressed = resp.compressBody("gzip");
    if (compressed) return false;
    return true;
}

bool test_response_compress_idempotent() {
    // Calling compressBody twice should not re-compress or change state
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setBody(std::string(512, 'b'));

    if (!resp.compressBody("gzip")) return false;
    auto firstSize = resp.bodySize();

    // Second call should be a no-op
    if (resp.compressBody("gzip")) return false;
    if (resp.bodySize() != firstSize) return false;
    return true;
}

bool test_response_buildFull_uses_compressed_body() {
    // v0.6.0 fix: buildFull() must use m_compressedBody (not m_body)
    // when the response has been compressed.
    Response resp;
    resp.setStatus(StatusCode::OK);
    std::string body(1024, 'a');
    resp.setBody(body);

    if (!resp.compressBody("gzip")) return false;

    auto fullResp = resp.buildFull();

    // Find the header/body boundary
    std::string fullStr(reinterpret_cast<const char*>(fullResp.data()), fullResp.size());
    auto headerEnd = fullStr.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;
    size_t bodyStart = headerEnd + 4;
    size_t wireBodySize = fullResp.size() - bodyStart;

    // Wire body must match the compressed body size (NOT the original 1024)
    if (wireBodySize >= 1024) return false;

    // Wire body must start with gzip magic bytes 0x1f 0x8b
    if (fullResp[bodyStart] != 0x1f || fullResp[bodyStart + 1] != 0x8b) return false;

    // Content-Length header must match the wire body size
    auto cl = resp.header("Content-Length");
    if (!cl) return false;
    if (std::stoul(std::string(*cl)) != wireBodySize) return false;

    return true;
}

bool test_response_bodySize_after_compress() {
    // v0.6.0 fix: bodySize() should return compressed size when compressed
    Response resp;
    resp.setStatus(StatusCode::OK);
    std::string body(512, 'x');
    resp.setBody(body);

    if (resp.bodySize() != 512) return false;
    if (!resp.compressBody("gzip")) return false;

    size_t compressedSize = resp.bodySize();
    if (compressedSize >= 512) return false;  // compressed should be smaller
    if (compressedSize == 0) return false;

    return true;
}
#endif // OMNINET_HAVE_ZLIB

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

#ifdef OMNINET_HAVE_ZLIB
    if (test_response_compress_with_gzip_support()) {
        std::cout << "[PASS] Compress body with gzip support" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Compress body with gzip support" << std::endl;
        failed++;
    }

    if (test_response_no_compress_without_gzip()) {
        std::cout << "[PASS] No compress without gzip in Accept-Encoding" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] No compress without gzip in Accept-Encoding" << std::endl;
        failed++;
    }

    if (test_response_no_compress_empty_body()) {
        std::cout << "[PASS] No compress empty body" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] No compress empty body" << std::endl;
        failed++;
    }

    if (test_response_compress_idempotent()) {
        std::cout << "[PASS] Compress idempotent (no double-compress)" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Compress idempotent (no double-compress)" << std::endl;
        failed++;
    }

    if (test_response_buildFull_uses_compressed_body()) {
        std::cout << "[PASS] buildFull uses compressed body (v0.6.0 fix)" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] buildFull uses compressed body (v0.6.0 fix)" << std::endl;
        failed++;
    }

    if (test_response_bodySize_after_compress()) {
        std::cout << "[PASS] bodySize reflects compressed size" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] bodySize reflects compressed size" << std::endl;
        failed++;
    }
#endif // OMNINET_HAVE_ZLIB

    std::cout << std::endl;
    std::cout << "HTTP Response tests: " << passed << " passed, " << failed << " failed" << std::endl;

    return failed > 0 ? 1 : 0;
}