// OmniNet HTTPS Server Demo
#include "OmniNet.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet HTTPS Server Demo" << std::endl;
    std::cout << "=========================" << std::endl;
    
    // Initialize OpenSSL
    OmniNet::init();
    
    // Create server config
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8443;
    config.tlsEnabled = true;
    config.tlsCertFile = "server.crt";
    config.tlsKeyFile = "server.key";
    
    Server server(config);
    
    server.router().get("/", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("text/html");
        resp.setBody(R"(<!DOCTYPE html>
<html>
<head><title>OmniNet HTTPS</title></head>
<body>
    <h1>HTTPS Server</h1>
    <p>This page was served over HTTPS!</p>
</body>
</html>)");
    });
    
    server.router().get("/api/secure", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"secure": true, "message": "This connection is encrypted!"})");
    });
    
    std::cout << "Starting HTTPS server on " << config.address << ":" << config.port << std::endl;
    std::cout << "Note: Make sure to generate server.crt and server.key first" << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "HTTPS Server started!" << std::endl;
    std::cout << "Try: https://localhost:" << config.port << std::endl;
    std::cout << "Note: Accept the self-signed certificate in your browser" << std::endl;
    
    serverThread.join();
    OmniNet::cleanup();
    
    return 0;
}