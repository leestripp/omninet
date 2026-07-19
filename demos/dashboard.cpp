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
    
    // Load config from the XDG config directory.
    auto& config = OmniNet::Config::instance();
    
    // Determine static files directory
    std::string staticDir = "static";
    if (!std::filesystem::exists(staticDir)) {
        std::cout << "Warning: static/ directory not found" << std::endl;
    }
    
    // Create dashboard configuration using config system
    Dashboard::Handler::Config dashboardConfig;
    dashboardConfig.urlPath = "/dashboard";
    dashboardConfig.apiPath = "/api/dashboard";
    dashboardConfig.dbPath = config.databasePath();  // Use config database path
    dashboardConfig.sessionTtlSeconds = config.get("dashboard.sessionTtl", 3600);
    dashboardConfig.allowedIpPrefixes = {"127.", "::1"};
    
    // Create dashboard handler
    Dashboard::Handler dashboard(dashboardConfig);
    
    // Create HTTP server using config settings
    Server::Config serverConfig;
    serverConfig.address = config.getString("server.address", "127.0.0.1");
    serverConfig.port = static_cast<uint16_t>(config.get("server.port", 8080));
    serverConfig.workerCount = 4;
    serverConfig.corsEnabled = config.get("cors.enabled", true);
    serverConfig.corsAllowedOrigins = config.getString("cors.origins", "*");
    
    Server server(serverConfig);
    
    // Mount dashboard FIRST so its specific routes (/dashboard, /api/dashboard/*)
    // are registered before the catch-all /* shadowing them
    dashboard.mount(server);

    // Create static file handler for index.html and CSS
    StaticHandler staticHandler(staticDir);
    staticHandler.setDefaultFile("index.html");

    // IMPORTANT: Register the API status endpoint BEFORE the catch-all /*
    // so it isn't shadowed by it
    server.router().get("/api/status", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"status": "running"})");
    });

    // Serve root path with index.html
    server.router().get("/", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });

    // Catch-all LAST, with a check to skip reserved paths so it doesn't
    // shadow /api/*, /dashboard, or /omninet/* (admin) routes
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        std::string path(req.path());
        if (path.rfind("/api/", 0) == 0 ||
            path.rfind("/dashboard", 0) == 0 ||
            path.rfind("/omninet/", 0) == 0) {
            resp = Response::notFound();
            return;
        }
        staticHandler.handle(req, resp);
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