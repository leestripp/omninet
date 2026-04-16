// OmniNet Admin Dashboard Demo
#include "OmniNet.h"
#include "dashboard/DashboardHandler.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace OmniNet;
using namespace OmniNet::HTTP;
using namespace OmniNet::Dashboard;

int main() {
    std::cout << "OmniNet Admin Dashboard Demo" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Create dashboard configuration
    Handler::Config config;
    config.urlPath = "/dashboard";
    config.apiPath = "/api";
    config.dbPath = "dashboard.db";
    config.sessionTtlSeconds = 3600;  // 1 hour
    // Allow access from localhost only for security
    config.allowedIpPrefixes = {"127.", "::1"};
    
    // Create dashboard handler
    Handler dashboard(config);
    
    // Create HTTP server
    Server::Config serverConfig;
    serverConfig.address = "127.0.0.1";
    serverConfig.port = 8080;
    serverConfig.workerCount = 4;
    
    Server server(serverConfig);
    
    // Mount dashboard
    dashboard.mount(server);
    
    // Add some custom routes
    server.router().get("/", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("text/html");
        resp.setBody(R"(
<!DOCTYPE html>
<html>
<head>
    <title>OmniNet Server</title>
</head>
<body>
    <h1>OmniNet Server</h1>
    <p>Server is running!</p>
    <p>Visit <a href="/dashboard">Admin Dashboard</a> for management.</p>
</body>
</html>
)");
    });
    
    server.router().get("/api/status", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({
    "status": "running",
    "version": ")" OMNINET_VERSION R"(",
    "uptime": "0"
})");
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
    std::cout << "  http://localhost:" << serverConfig.port << "/api/status" << std::endl;
    std::cout << std::endl;
    std::cout << "First user created will become admin!" << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}