// OmniNet Static File Server Demo
#include "OmniNet.h"
#include "http/StaticHandler.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace OmniNet;
using namespace OmniNet::HTTP;

int main(int argc, char* argv[]) {
    std::cout << "OmniNet Static File Server Demo" << std::endl;
    std::cout << "===============================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Get root directory (default to current directory or argument)
    std::string rootDir = ".";
    if (argc > 1) {
        rootDir = argv[1];
    }
    
    // Verify directory exists
    if (!std::filesystem::exists(rootDir) || !std::filesystem::is_directory(rootDir)) {
        std::cerr << "Error: Directory '" << rootDir << "' does not exist" << std::endl;
        return 1;
    }
    
    std::cout << "Serving files from: " << std::filesystem::absolute(rootDir) << std::endl;
    
    // Create static handler
    StaticHandler staticHandler(rootDir);
    staticHandler.setDefaultFile("index.html");
    staticHandler.setIndexFiles({"index.html", "index.htm", "default.html"});
    staticHandler.allowDirectoryListing(true);
    
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8080;
    
    Server server(config);
    
    // Mount static files at root - use Router
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        std::string path = std::string(req.path());
        if (path == "/") {
            staticHandler.handle(req, resp);
        } else {
            staticHandler.handle(req, resp);
        }
    });
    
    // API endpoint for info
    server.router().get("/api/info", [&rootDir](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        
        size_t fileCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(rootDir)) {
            if (entry.is_regular_file()) fileCount++;
        }
        
        std::string json = "{";
        json += "\"root\": \"" + rootDir + "\",";
        json += "\"files\": " + std::to_string(fileCount);
        json += "}";
        
        resp.setBody(json);
    });
    
    std::cout << "Starting static file server on 127.0.0.1:8080" << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Static File Server started!" << std::endl;
    std::cout << "Try: http://localhost:8080/" << std::endl;
    
    serverThread.join();
    OmniNet::cleanup();
    
    return 0;
}