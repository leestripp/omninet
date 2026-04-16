// OmniNet HTTP Server Demo
#include "OmniNet.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet HTTP Server Demo" << std::endl;
    std::cout << "========================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Create server configuration
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8080;
    config.workerCount = 4;
    config.requestTimeout = std::chrono::seconds(30);
    config.corsEnabled = true;
    config.corsAllowedOrigins = "*";
    
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
    <title>OmniNet Demo</title>
</head>
<body>
    <h1>OmniNet HTTP Server</h1>
    <p>Welcome to OmniNet!</p>
    <p>Version: 1.0.0</p>
</body>
</html>
)") ;
    });
    
    server.router().get("/api/hello", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    server.router().get("/api/time", [](const Request& req, Response& resp) {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody("{\"timestamp\": " + std::to_string(timestamp) + "}");
    });
    
    server.router().get("/api/health", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"status": "healthy"})");
    });
    
    // Handle POST request
    server.router().post("/api/echo", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"echo": ")" + std::string(req.bodyAsString()) + R"("})");
    });
    
    // Start server in background
    std::cout << "Starting server on " << config.address << ":" << config.port << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Server started!" << std::endl;
    std::cout << "Try these endpoints:" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/api/hello" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/api/time" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/api/health" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}
