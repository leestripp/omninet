// OmniNet Admin Dashboard Demo
#include "OmniNet.h"
#include "dashboard/DashboardHandler.h"
#include "http/StaticHandler.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet Admin Dashboard Demo" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Determine static files directory
    std::string staticDir = "static";
    if (!std::filesystem::exists(staticDir)) {
        std::cout << "Warning: static/ directory not found" << std::endl;
    }
    
    // Create dashboard configuration
    Dashboard::Handler::Config config;
    config.urlPath = "/dashboard";
    config.apiPath = "/api";
    config.dbPath = "dashboard.db";
    config.sessionTtlSeconds = 3600;
    config.allowedIpPrefixes = {"127.", "::1"};
    
    // Create dashboard handler
    Dashboard::Handler dashboard(config);
    
    // Create HTTP server
    Server::Config serverConfig;
    serverConfig.address = "127.0.0.1";
    serverConfig.port = 8080;
    serverConfig.workerCount = 4;
    
    Server server(serverConfig);
    
    // Mount dashboard
    dashboard.mount(server);
    
    // Create static file handler for index.html and CSS
    StaticHandler staticHandler(staticDir);
    staticHandler.setDefaultFile("index.html");
    
    // Serve root path with index.html
    server.router().get("/", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    // Serve static files (CSS, etc)
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    // API status endpoint
    server.router().get("/api/status", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"status": "running"})");
    });
    
    // Start server in background
    std::cout << "Starting server on " << serverConfig.address << ":" << serverConfig.port << std::endl;
    
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
    std::cout << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  http://localhost:" << serverConfig.port << "/" << std::endl;
    std::cout << "  http://localhost:" << serverConfig.port << "/dashboard - Admin Dashboard" << std::endl;
    std::cout << std::endl;
    std::cout << "First user created will become admin!" << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}