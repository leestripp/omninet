// OmniNet HTTP Server Demo
#include "OmniNet.h"
#include "http/StaticHandler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet HTTP Server Demo" << std::endl;
    std::cout << "========================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Determine static files directory
    std::string staticDir = "static";
    if (!std::filesystem::exists(staticDir)) {
        std::cout << "Warning: static/ directory not found, using root" << std::endl;
        staticDir = ".";
    }
    
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
    
    // Create static file handler
    StaticHandler staticHandler(staticDir);
    staticHandler.setDefaultFile("index.html");
    
    // Mount static file handler for root and static paths
    server.router().get("/", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    // API endpoints
    server.router().get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    server.router().get("/api/time", [](const Request& req, Response& resp) {
        (void)req;
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody("{\"timestamp\": " + std::to_string(timestamp) + "}");
    });
    
    server.router().get("/api/health", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"status": "healthy"})");
    });
    
    // POST endpoint
    server.router().post("/api/echo", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"echo": ")" + std::string(req.bodyAsString()) + R"("})");
    });
    
    // Start server in background
    std::cout << "Starting server on " << config.address << ":" << config.port << std::endl;
    std::cout << "Serving static files from: " << std::filesystem::absolute(staticDir) << std::endl;
    
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
    std::cout << "  http://localhost:" << config.port << "/api/hello" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/api/health" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/api/time" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}