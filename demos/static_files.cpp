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
    
    // Create server configuration
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8080;
    config.workerCount = 4;
    
    // Create server
    Server server(config);
    
    // Create static file handler
    StaticHandler staticHandler(rootDir);
    staticHandler.setDefaultFile("index.html");
    staticHandler.setIndexFiles({"index.html", "index.htm", "default.html"});
    staticHandler.allowDirectoryListing(true);
    
    // Mount static files at root
    server.router().get("/*", [&staticHandler](const Request& req, Response& resp) {
        // Get the path, removing leading slash
        std::string path = std::string(req.path());
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }
        
        // Handle empty path
        if (path.empty()) {
            path = "index.html";
        }
        
        // Serve the file
        staticHandler.handle(req.path(), resp);
    });
    
    // API endpoint for directory listing info
    server.router().get("/api/info", [&rootDir](const Request& req, Response& resp) {
        (void)req;
        resp.setStatus(StatusCode::OK);
        resp.setContentType("application/json");
        
        // Count files in root directory
        size_t fileCount = 0;
        size_t dirCount = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator(rootDir)) {
            if (entry.is_regular_file()) fileCount++;
            else if (entry.is_directory()) dirCount++;
        }
        
        std::string json = "{";
        json += "\"root\": \"" + rootDir + "\",";
        json += "\"files\": " + std::to_string(fileCount) + ",";
        json += "\"directories\": " + std::to_string(dirCount);
        json += "}";
        
        resp.setBody(json);
    });
    
    // Start server in background
    std::cout << "Starting static file server on " << config.address << ":" << config.port << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Static File Server started!" << std::endl;
    std::cout << "Try these endpoints:" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/" << std::endl;
    std::cout << "  http://localhost:" << config.port << "/api/info" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << argv[0] << " [directory]" << std::endl;
    std::cout << "  Default directory: current directory" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}