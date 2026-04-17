// OmniNet HTTPS Server Demo
#include "OmniNet.h"
#include "http/StaticHandler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main() {
    std::cout << "OmniNet HTTPS Server Demo" << std::endl;
    std::cout << "=========================" << std::endl;
    
    // Initialize OpenSSL
    OmniNet::init();
    
    // Load config from ~/.omninet/config.json
    auto& omninetConfig = OmniNet::Config::instance();
    
    // Enable logging (from config)
    std::string logLevel = omninetConfig.getString("logging.level", "INFO");
    OmniNet::Logger::instance().setLevel(
        logLevel == "DEBUG" ? OmniNet::LogLevel::DEBUG :
        logLevel == "WARN" ? OmniNet::LogLevel::WARN :
        OmniNet::LogLevel::INFO
    );
    
    // Set log file from config
    if (omninetConfig.logFile()) {
        OmniNet::Logger::instance().setLogFile(*omninetConfig.logFile());
    }
    
    // Determine static files directory
    std::string staticDir = "static";
    if (!std::filesystem::exists(staticDir)) {
        std::cerr << "Warning: static/ directory not found, using root" << std::endl;
        staticDir = ".";
    }
    
    // Create server config from OmniNet config system
    Server::Config serverConfig;
    serverConfig.address = omninetConfig.getString("server.address", "127.0.0.1");
    serverConfig.port = static_cast<uint16_t>(omninetConfig.getInt("server.httpsPort", 8443));
    serverConfig.workerCount = static_cast<size_t>(omninetConfig.getInt("server.workers", 4));
    serverConfig.requestTimeout = std::chrono::seconds(omninetConfig.getInt("server.timeout", 30));
    serverConfig.tlsEnabled = omninetConfig.getBool("tls.enabled", true);
    
    // Get TLS cert/key from OmniNet config
    if (serverConfig.tlsEnabled) {
        serverConfig.tlsCertFile = omninetConfig.defaultTlsCertPath().string();
        serverConfig.tlsKeyFile = omninetConfig.defaultTlsKeyPath().string();
        
        if (serverConfig.tlsCertFile.empty()) {
            std::cerr << "Warning: TLS certificate not found. Run 'omninet-init --enable-tls' first." << std::endl;
            serverConfig.tlsEnabled = false;
        }
    }
    
    serverConfig.corsEnabled = omninetConfig.getBool("cors.enabled", true);
    serverConfig.corsAllowedOrigins = omninetConfig.getString("cors.origins", "*");
    
    Server server(serverConfig);
    
    // Create static file handler
    StaticHandler staticHandler(staticDir);
    staticHandler.setDefaultFile("index.html");
    
    // API endpoints
    server.router().get("/api/hello", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"message": "Hello from HTTPS!"})");
    });
    
    server.router().get("/api/health", [](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        resp.setBody(R"({"status": "healthy", "protocol": "https"})");
    });
    
    // Static routes - serve index.html for root
    server.router().get("/", [&staticHandler](const Request& req, Response& resp) {
        staticHandler.handle(req, resp);
    });
    
    // Catch-all for other static files
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        std::string path(req.path());
        if (path.rfind("/api/", 0) == 0) {
            resp = Response::notFound();
            return;
        }
        staticHandler.handle(req, resp);
    });
    
    std::cerr << "Starting HTTPS server on " << serverConfig.address << ":" << serverConfig.port << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "HTTPS Server started!" << std::endl;
    std::cout << "Serving static files from: " << std::filesystem::absolute(staticDir) << std::endl;
    std::cout << "Try:" << std::endl;
    std::cout << "  https://localhost:" << serverConfig.port << "/api/hello" << std::endl;
    std::cout << "  https://localhost:" << serverConfig.port << "/api/health" << std::endl;
    std::cout << "Note: Accept the self-signed certificate in your browser" << std::endl;
    
    serverThread.join();
    OmniNet::cleanup();
    
    return 0;
}
