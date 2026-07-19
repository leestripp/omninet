// OmniNet HTTP Server Demo
#include "OmniNet.h"
#include "http/StaticHandler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstdlib>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet HTTP Server Demo" << std::endl;
    std::cout << "========================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Load config from the XDG config directory.
    auto& config = OmniNet::Config::instance();
    
    // Enable debug logging (from config)
    std::string logLevel = config.getString("logging.level", "INFO");
    OmniNet::Logger::instance().setLevel(
        logLevel == "DEBUG" ? OmniNet::LogLevel::DEBUG :
        logLevel == "WARN" ? OmniNet::LogLevel::WARN :
        OmniNet::LogLevel::INFO
    );
    
    // Set log file to the XDG state directory.
    auto stateDir = config.stateDir();
    std::filesystem::create_directories(stateDir);
    auto logPath = (stateDir / "omninet.log").string();
    OmniNet::Logger::instance().setLogFile(logPath);
    std::cerr << "[Demo] Logging to: " << logPath << std::endl;
    
    // Determine static files directory
    std::string staticDir = "static";
    if (!std::filesystem::exists(staticDir)) {
        std::cout << "Warning: static/ directory not found, using root" << std::endl;
        staticDir = ".";
    }
    
    // Create server configuration from config system.
    // OMNINET_PORT env var wins over the config file (useful for
    // container deployments where the host port is fixed, e.g. 8000).
    Server::Config serverConfig;
    uint16_t serverPort = 8080;
    if (const char* envPort = std::getenv("OMNINET_PORT"); envPort && *envPort) {
        try {
            int p = std::stoi(envPort);
            if (p > 0 && p < 65536) serverPort = static_cast<uint16_t>(p);
        } catch (...) { /* fall through to config value */ }
    }
    serverConfig.address = config.getString("server.address", "127.0.0.1");
    serverConfig.port    = static_cast<uint16_t>(config.getInt("server.port", serverPort));
    serverConfig.workerCount = static_cast<size_t>(config.getInt("server.workers", 4));
    serverConfig.requestTimeout = std::chrono::seconds(config.getInt("server.timeout", 30));
    serverConfig.corsEnabled = config.getBool("cors.enabled", true);
    serverConfig.corsAllowedOrigins = config.getString("cors.origins", "*");
    std::cerr << "[Demo] Listening on " << serverConfig.address << ":"
              << serverConfig.port << std::endl;
    
    // Create server
    Server server(serverConfig);
    
    // Create static file handler
    StaticHandler staticHandler(staticDir);
    staticHandler.setDefaultFile("index.html");
    
    // IMPORTANT: Register ADMIN routes FIRST so they are matched before catch-all
    // This ensures /omninet/* routes work correctly
    std::cerr << "[Demo] Mounting admin routes: /omninet/health, /omninet/stats" << std::endl;
    server.adminHandler().mount(server);
    
    // IMPORTANT: Register API routes FIRST so they are matched before static catch-all
    // The router checks routes in order of registration
    
    // API endpoints
    std::cerr << "[Demo] Registering route: GET /api/hello" << std::endl;
    server.router().get("/api/hello", [](const Request& req, Response& resp) {
        std::cerr << "[Demo] Handler called for /api/hello" << std::endl;
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello, World!"})");
    });
    
    std::cerr << "[Demo] Registering route: GET /api/time" << std::endl;
    server.router().get("/api/time", [](const Request& req, Response& resp) {
        (void)req;
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::system_clock::to_time_t(now);
        
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody("{\"timestamp\": " + std::to_string(timestamp) + "}");
    });
    
    std::cerr << "[Demo] Registering route: GET /api/health" << std::endl;
    server.router().get("/api/health", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"status": "healthy"})");
    });
    
    // POST endpoint
    std::cerr << "[Demo] Registering route: POST /api/echo" << std::endl;
    server.router().post("/api/echo", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"echo": ")" + std::string(req.bodyAsString()) + R"("})");
    });
    
    // Now register static routes AFTER API routes
    // The router checks routes in order - first match wins!
    std::cerr << "[Demo] Registering static route: GET /" << std::endl;
    server.router().get("/", [&staticHandler](const Request& req, Response& resp) {
        std::cerr << "[Demo] Static handler for /" << std::endl;
        staticHandler.handle(req, resp);
    });
    
    // Use a more restrictive pattern for static files (only match paths without more segments)
    // Actually, let's use a conditional approach - check if path starts with /api
    // For now, just don't register /* at all, let unmatched routes fall through
    // or register it last so it catches things not caught by API routes
    
    std::cerr << "[Demo] Registering static route: GET /* (catch-all)" << std::endl;
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        std::cerr << "[Demo] Static handler for /*" << std::endl;
        // Only serve static files if not an API or admin path
        std::string path(req.path());
        if (path.rfind("/api/", 0) == 0 || path.rfind("/omninet/", 0) == 0) {
            std::cerr << "[Demo] Skipping static handler for reserved path: " << path << std::endl;
            resp = Response::notFound();
            return;
        }
        staticHandler.handle(req, resp);
    });
    
    // Start server in background
    std::cout << "Starting server on " << serverConfig.address << ":" << serverConfig.port << std::endl;
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
    std::cout << "  http://localhost:" << serverConfig.port << "/api/hello" << std::endl;
    std::cout << "  http://localhost:" << serverConfig.port << "/api/health" << std::endl;
    std::cout << "  http://localhost:" << serverConfig.port << "/api/time" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}