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
    
    // Enable debug logging
    OmniNet::Logger::instance().setLevel(OmniNet::LogLevel::DEBUG);
    
    // Determine static files directory
    std::string staticDir = "static";
    if (!std::filesystem::exists(staticDir)) {
        std::cerr << "Warning: static/ directory not found, using root" << std::endl;
        staticDir = ".";
    }
    
    // Create server config
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8443;
    config.tlsEnabled = true;
    
    // Look for certs in ~/.omninet/certs/ (standard OmniNet location)
    std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : ".";
    std::string certsDir = homeDir + "/.omninet/certs";
    config.tlsCertFile = certsDir + "/omninet.crt";
    config.tlsKeyFile = certsDir + "/omninet.key";
    
    config.corsEnabled = true;
    config.corsAllowedOrigins = "*";
    
    Server server(config);
    
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
    
    std::cerr << "Starting HTTPS server on " << config.address << ":" << config.port << std::endl;
    
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
    std::cout << "  https://localhost:" << config.port << "/api/hello" << std::endl;
    std::cout << "  https://localhost:" << config.port << "/api/health" << std::endl;
    std::cout << "Note: Accept the self-signed certificate in your browser" << std::endl;
    
    serverThread.join();
    OmniNet::cleanup();
    
    return 0;
}
