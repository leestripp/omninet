// OmniNet WebSocket Echo Server Demo
#include "OmniNet.h"
#include "websocket/WebSocket.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace OmniNet;
using namespace OmniNet::HTTP;
using namespace OmniNet::WebSocket;

int main() {
    std::cout << "OmniNet WebSocket Echo Server Demo" << std::endl;
    std::cout << "===================================" << std::endl;
    
    // Initialize OmniNet
    OmniNet::init();
    
    // Create HTTP server configuration
    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 8080;
    config.workerCount = 4;
    
    // Create HTTP server
    Server server(config);
    
    // Create WebSocket server
    WebSocket::Server wsServer;
    
    // Register WebSocket endpoint
    wsServer.on("/echo", [](Connection& ws) {
        std::cout << "Client connected to echo endpoint" << std::endl;
        
        // Set up message handler
        ws.onMessage([&ws](std::span<const uint8_t> data, Opcode opcode) {
            std::string message(reinterpret_cast<const char*>(data.data()), data.size());
            std::cout << "Received: " << message << std::endl;
            
            // Echo back the message
            if (opcode == Opcode::TEXT) {
                std::string response = "Echo: " + message;
                ws.sendText(response);
            } else if (opcode == Opcode::BINARY) {
                ws.sendBinary(data);
            }
        });
        
        // Set up close handler
        ws.onClose([](uint16_t code, std::string reason) {
            std::cout << "Client disconnected: " << code << " - " << reason << std::endl;
        });
        
        // Set up error handler
        ws.onError([](const std::string& error) {
            std::cerr << "WebSocket error: " << error << std::endl;
        });
        
        // Send welcome message
        ws.sendText("Welcome to OmniNet WebSocket Echo Server!");
    });
    
    // Also handle a chat endpoint
    wsServer.on("/chat", [](Connection& ws) {
        std::cout << "Client connected to chat endpoint" << std::endl;
        
        ws.sendText("Welcome to the chat room!");
        
        ws.onMessage([&ws](std::span<const uint8_t> data, Opcode opcode) {
            if (opcode == Opcode::TEXT) {
                std::string message(reinterpret_cast<const char*>(data.data()), data.size());
                std::cout << "Chat: " << message << std::endl;
                // In a real chat, we'd broadcast to all clients
                ws.sendText("You said: " + message);
            }
        });
    });
    
    // Mount WebSocket server on HTTP server
    wsServer.mount(server);
    
    // Setup HTTP routes for info
    server.router().get("/", [](const Request& req, Response& resp) {
        resp.setStatus(StatusCode::OK);
        resp.setContentType("text/html");
        resp.setBody(R"(
<!DOCTYPE html>
<html>
<head>
    <title>OmniNet WebSocket Demo</title>
</head>
<body>
    <h1>OmniNet WebSocket Echo Server</h1>
    <p>Connect to <code>ws://localhost:8080/echo</code> to test echo.</p>
    <p>Connect to <code>ws://localhost:8080/chat</code> for chat demo.</p>
    <hr>
    <h2>Test with JavaScript:</h2>
    <pre>
const ws = new WebSocket('ws://localhost:8080/echo');
ws.onopen = () => ws.send('Hello!');
ws.onmessage = (event) => console.log(event.data);
    </pre>
</body>
</html>
)");
    });
    
    // Start server in background
    std::cout << "Starting server on " << config.address << ":" << config.port << std::endl;
    
    std::thread serverThread([&server]() {
        try {
            server.start();
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
        }
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "WebSocket Server started!" << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  ws://localhost:" << config.port << "/echo - Echo server" << std::endl;
    std::cout << "  ws://localhost:" << config.port << "/chat - Chat demo" << std::endl;
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the server..." << std::endl;
    
    // Wait for interrupt
    serverThread.join();
    
    // Cleanup
    OmniNet::cleanup();
    
    return 0;
}