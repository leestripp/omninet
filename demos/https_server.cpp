// OmniNet HTTPS Server Demo
#include "OmniNet.h"
#include "net/TLSSocket.h"
#include "crypto/Crypto.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet HTTPS Server Demo" << std::endl;
    std::cout << "=========================" << std::endl;
    
    // Initialize OpenSSL
    OmniNet::init();
    
    // Generate self-signed certificate if it doesn't exist
    std::string certFile = "server.crt";
    std::string keyFile = "server.key";
    
    // For demo, we'll create a basic config
    // In production, use proper certificates
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8443;
    config.tlsEnabled = true;
    config.tlsCertFile = certFile;
    config.tlsKeyFile = keyFile;
    
    // Try to generate self-signed certificate
    std::cout << "Generating self-signed certificate..." << std::endl;
    
    auto certKey = TLS::generateSelfSigned("localhost", "OmniNet Demo");
    if (!certKey.first.empty() && !certKey.second.empty()) {
        // Write certificate and key to files
        std::ofstream certOut(certFile);
        certOut << certKey.first;
        certOut.close();
        
        std::ofstream keyOut(keyFile);
        keyOut << certKey.second;
        keyOut.close();
        
        std::cout << "Certificate generated successfully!" << std::endl;
    }
    
    // Create server
    Server server(config);
    
    // Setup routes
    server.router().get("/", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("text/html");
        resp.setBody(R"(
<!DOCTYPE html>
<html>
<head>
    <title>OmniNet HTTPS Demo</title>
</head>
<body>
    <h1>OmniNet HTTPS Server</h1>
    <p>This page was served over HTTPS!</p>
    <p>Version: )" OMNINET_VERSION R"(</p>
</body>
</html>
)");
    });
    
    server.router().get("/api/secure", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"secure": true, "message": "This connection is encrypted!"})");
    });
    
    server.router().get("/api/cipher", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        // Note: In real implementation, you'd get the cipher from the TLS socket
        resp.setBody(R"({"cipher": "TLS_AES_256_GCM_SHA384"})");
    });
    
    // Start server in background
    std::cout << "Starting HTTPS server on " << config.address << ":" << config.port << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "HTTPS Server started!" << std::endl;
    std::cout << "Try these endpoints (use https://):" << std::endl;
    std::cout << "  https://localhost:" << config.port << "/" << std::endl;
    std::cout << "  https://localhost:" << config.port << "/api/secure" << std::endl;
    std::cout << "  https://localhost:" << config.port << "/api/cipher" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: You'll need to accept the self-signed certificate" << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}