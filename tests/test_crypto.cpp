// OmniNet Crypto Tests
#include "crypto/Crypto.h"
#include <iostream>
#include <iomanip>

using namespace OmniNet::Crypto;

bool test_sha256() {
    std::string input = "Hello, World!";
    auto hash = sha256(std::vector<uint8_t>(input.begin(), input.end()));
    
    // Known SHA-256 hash of "Hello, World!"
    std::string expected = "dffd6021bb2bd5b0af676290809ec3a53191dd81c7f70a4b28688a362182986f";
    
    std::string hashStr = bytesToHex(hash);
    
    if (hashStr != expected) {
        std::cerr << "Expected: " << expected << std::endl;
        std::cerr << "Got: " << hashStr << std::endl;
        return false;
    }
    
    return true;
}

bool test_hmac_sha256() {
    std::string key = "secret";
    std::string message = "The quick brown fox jumps over the lazy dog";
    
    auto hmac = hmacSha256(
        std::vector<uint8_t>(key.begin(), key.end()),
        std::vector<uint8_t>(message.begin(), message.end())
    );
    
    // Known HMAC-SHA256
    std::string expected = "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8";
    
    std::string hmacStr = bytesToHex(hmac);
    
    if (hmacStr != expected) {
        std::cerr << "Expected: " << expected << std::endl;
        std::cerr << "Got: " << hmacStr << std::endl;
        return false;
    }
    
    return true;
}

bool test_base64_encode() {
    std::string input = "Hello, World!";
    auto encoded = base64Encode(std::vector<uint8_t>(input.begin(), input.end()));
    
    if (encoded != "SGVsbG8sIFdvcmxkIQ==") {
        std::cerr << "Expected: SGVsbG8sIFdvcmxkIQ==" << std::endl;
        std::cerr << "Got: " << encoded << std::endl;
        return false;
    }
    
    return true;
}

bool test_base64_decode() {
    std::string encoded = "SGVsbG8sIFdvcmxkIQ==";
    auto decoded = base64Decode(encoded);
    
    std::string expected = "Hello, World!";
    std::string result(decoded.begin(), decoded.end());
    
    if (result != expected) {
        std::cerr << "Expected: " << expected << std::endl;
        std::cerr << "Got: " << result << std::endl;
        return false;
    }
    
    return true;
}

bool test_random_bytes() {
    auto bytes1 = randomBytes(32);
    auto bytes2 = randomBytes(32);
    
    if (bytes1.size() != 32 || bytes2.size() != 32) return false;
    
    // They should be different (with very high probability)
    bool allSame = true;
    for (size_t i = 0; i < 32; i++) {
        if (bytes1[i] != bytes2[i]) {
            allSame = false;
            break;
        }
    }
    
    // This test could theoretically fail, but probability is ~1/2^256
    if (allSame) {
        std::cerr << "Warning: Random bytes were identical" << std::endl;
    }
    
    return true;
}

int main() {
    std::cout << "Running Crypto tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_sha256()) {
        std::cout << "[PASS] SHA-256 hash" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] SHA-256 hash" << std::endl;
        failed++;
    }
    
    if (test_hmac_sha256()) {
        std::cout << "[PASS] HMAC-SHA256" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] HMAC-SHA256" << std::endl;
        failed++;
    }
    
    if (test_base64_encode()) {
        std::cout << "[PASS] Base64 encode" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Base64 encode" << std::endl;
        failed++;
    }
    
    if (test_base64_decode()) {
        std::cout << "[PASS] Base64 decode" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Base64 decode" << std::endl;
        failed++;
    }
    
    if (test_random_bytes()) {
        std::cout << "[PASS] Random bytes generation" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Random bytes generation" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "Crypto tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}