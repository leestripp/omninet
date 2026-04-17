// OmniNet WebSocket Echo Server Demo
#include "OmniNet.h"
#include "websocket/WebSocket.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace OmniNet;
using namespace OmniNet::WebSocket;

int main() {
    std::cout << "OmniNet WebSocket Echo Server Demo" << std::endl;
    std::cout << "===================================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Load config from ~/.omninet/config.json
    auto& config = OmniNet::Config::instance();
    
    // Create WebSocket server with config settings
    OmniNet::HTTP::Server::Config serverConfig;
    serverConfig.address = config.getString("server.address", "127.0.0.1");
    serverConfig.port = static_cast<uint16_t>(config.getInt("server.port", 8080));
    serverConfig.workerCount = static_cast<size_t>(config.getInt("server.workers", 4));
    serverConfig.requestTimeout = std::chrono::seconds(config.getInt("server.timeout", 30));
    serverConfig.corsEnabled = config.getBool("cors.enabled", true);
    serverConfig.corsAllowedOrigins = config.getString("cors.origins", "*");
    
    Server wsServer(serverConfig);
    
    // Register WebSocket endpoint - Echo
    wsServer.on("/echo", [](Connection& ws) {
        std::cout << "Client connected to echo endpoint" << std::endl;
        
        // Set up text message handler
        ws.onText([&ws](std::string message) {
            std::cout << "Received: " << message << std::endl;
            ws.sendText("Echo: " + message);
        });
        
        // Set up close handler
        ws.onClose([](CloseCode code, std::string reason) {
            std::cout << "Client disconnected: " << static_cast<int>(code) << " - " << reason << std::endl;
        });
        
        std::cout << "Echo handler registered" << std::endl;
    });
    
    std::cout << "Starting WebSocket server on ws://127.0.0.1:8080/echo" << std::endl;
    wsServer.start();
    
    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;
    
    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    wsServer.stop();
    OmniNet::cleanup();
    
    return 0;
}
