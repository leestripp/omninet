# OmniNet Logger API

## Overview

The `OmniNet::Logger` class provides thread-safe logging with:
- Multiple log levels (DEBUG, INFO, WARN, ERROR, CRITICAL)
- Console and file output
- Async mode for better performance
- Log rotation based on file size
- Configurable via LoggingConfig

## Quick Start

```cpp
#include "OmniNet.h"

using namespace OmniNet;

// Get singleton instance
Logger& logger = Logger::instance();

// Set log level
logger.setLevel(LogLevel::DEBUG);

// Log messages
logger.debug("Debug message");
logger.info("Info message");
logger.warn("Warning message");
logger.error("Error message");
logger.critical("Critical message");

// Format with arguments
logger.infoF("User %s logged in from %s", username.c_str(), ip.c_str());
```

## Log Levels

```cpp
enum class LogLevel {
    DEBUG = 0,    // Detailed information for debugging
    INFO = 1,     // General information
    WARN = 2,     // Warning conditions
    ERROR = 3,    // Error conditions
    CRITICAL = 4  // Critical errors, system failure imminent
};
```

## Configuration

### Basic Configuration

```cpp
Logger::instance().setLevel(LogLevel::INFO);
Logger::instance().setAsync(true);  // Default: true
```

### File Output

```cpp
// Set log file
Logger::instance().setLogFile("/path/to/logfile.log");

// Or use config to set file location
Logger::instance().setLogFile(
    (Config::instance().dataDir() / "logs" / "omninet.log").string()
);
```

### Log Rotation

```cpp
// Set max file size (in bytes)
Logger::instance().setMaxFileSize(10 * 1024 * 1024);  // 10MB

// Set max number of rotated files
Logger::instance().setMaxRotatedFiles(5);

// Rotation creates files like:
// omninet.log      <- current
// omninet.log.1    <- oldest
// omninet.log.2
// ...
// omninet.log.5    <- will be deleted when exceeded
```

### Init from Config

```cpp
#include "config/Config.h"

// Use LoggingConfig struct
LoggingConfig config;
config.level = "DEBUG";
config.toConsole = true;
config.toFile = true;
config.file = (Config::instance().dataDir() / "logs" / "omninet.log").string();
config.maxSizeMb = 10;
config.maxFiles = 5;

Logger::instance().initFromConfig(config);
```

## Logging Methods

### Simple Logging

```cpp
logger.debug("Debug message");
logger.info("Info message");
logger.warn("Warning message");
logger.error("Error message");
logger.critical("Critical message");
```

### String View (avoids string copies)

```cpp
std::string_view msg = ...;
logger.debug(msg);  // Uses string_view overload
```

### Formatted Logging

```cpp
logger.debugF("User %s performed action %d", username.c_str(), actionId);
logger.infoF("Connection from %s:%d established", ip.c_str(), port);
logger.errorF("Failed to process request: %s", error.c_str());
```

### With std::string

```cpp
std::string msg = "Complex message: " + data;
logger.info(msg);  // String overload
```

## Output Format

Default format: `[LEVEL] [timestamp] [thread] message`

Example:
```
[INFO] [2024-01-01 12:00:00.123] [139978714599744] Server started on port 8080
[ERROR] [2024-01-01 12:00:01.456] [139978714599744] Connection failed: timeout
[WARN] [2024-01-01 12:00:02.789] [139978714599744] Rate limit exceeded for IP 192.168.1.1
```

## Async Mode

Logger supports async mode for better performance:

```cpp
// Enable async (default)
logger.setAsync(true);

// Disable async (synchronous)
logger.setAsync(false);

// Manual flush (for async mode before exit)
logger.flush();
```

When async is enabled:
- Messages are queued and written in background thread
- Call `flush()` before exiting to ensure all messages are written

## Custom Handler

```cpp
// Custom handler for additional processing
logger.setHandler([](LogLevel level, const std::string& message) {
    if (level >= LogLevel::ERROR) {
        // Send to error tracking service
        sendToErrorService(message);
    }
    
    // Also write to syslog
    syslog(LOG_ERR, "%s", message.c_str());
});
```

## LogReader

Read and query log files:

```cpp
#include "utils/LogReader.h"

LogReader reader("/path/to/omninet.log");

// Read last N lines
auto entries = reader.readLast(100);

// Filter by level
reader.setMinLevel(LogLevel::ERROR);
auto errors = reader.readLast(50);

// Filter by pattern
reader.setPattern("connection failed");
auto matching = reader.readLast(50);

// Iterate entries
for (const auto& entry : entries) {
    std::cout << "[" << entry.level << "] " << entry.message << std::endl;
}
```

### LogEntry Structure

```cpp
struct LogEntry {
    LogLevel level;           // Parsed log level
    std::string timestamp;    // Original timestamp
    std::string threadId;     // Thread ID
    std::string message;      // Log message
    std::string rawLine;       // Original line
};
```

### Tail Mode

```cpp
// Start tailing new entries
auto stop = reader.tail([](const LogEntry& entry) {
    std::cout << entry.message << std::endl;
}, std::chrono::milliseconds(500));

// ... later, stop tailing
stop();  // Calls stopTail()
```

### Get Rotated Files

```cpp
auto files = reader.rotatedFiles();
// Returns: [omninet.log.1, omninet.log.2, ...]
```

## ScopedLogger

RAII-style logging for measuring execution time:

```cpp
#include "utils/Logger.h"

void someFunction() {
    OMNET_SCOPED_LOGGER("someFunction");
    // Logs: "someFunction - enter" on construction
    // Logs: "someFunction - exit (45ms)" on destruction
}

// Or manual
{
    ScopedLogger logger(Logger::instance(), LogLevel::INFO, "myOperation");
    
    // Do work
    doSomething();
    
    logger.log("Step 1 completed");
    doMore();
    
} // Logs duration on destruction
```

## Convenience Macros

```cpp
// Optional macros (can use Logger::instance() directly)
OMNINET_LOG_DEBUG("message")
OMNINET_LOG_INFO("message")
OMNINET_LOG_WARN("message")
OMNINET_LOG_ERROR("message")
OMNINET_LOG_CRITICAL("message")

// Scoped logging
OMNET_SCOPED_LOGGER("operationName")
```

## Best Practices

### 1. Use Appropriate Levels

```cpp
logger.debug("Entering function");          // DEBUG
logger.info("Server started");               // INFO
logger.warn("High memory usage");           // WARN
logger.error("Connection failed");          // ERROR
logger.critical("Database unavailable");   // CRITICAL
```

### 2. Use Format Strings for Complex Messages

```cpp
// Good: single formatted string
logger.infoF("User %s logged in from %s at %s", 
    username.c_str(), ip.c_str(), time.c_str());

// Avoid: string concatenation
logger.info("User " + username + " logged in");  // Less efficient
```

### 3. Flush Before Exit

```cpp
int main() {
    OmniNet::init();
    
    // ... app logic ...
    
    Logger::instance().flush();  // Ensure all logs written
    OmniNet::cleanup();
}
```

### 4. Configure Early

```cpp
int main() {
    OmniNet::init();
    
    // Configure logging first
    auto& config = Config::instance();
    LoggingConfig logConfig;
    logConfig.level = config.getString("logging.level", "INFO");
    logConfig.file = (config.dataDir() / "logs" / "omninet.log").string();
    Logger::instance().initFromConfig(logConfig);
    
    Logger::instance().info("Application started");
    
    // ... rest of app ...
}
```

## Example: Integration with HTTP Server

```cpp
#include "OmniNet.h"
#include "http/StaticHandler.h"

using namespace OmniNet;

int main() {
    OmniNet::init();
    
    // Configure logging
    auto& config = Config::instance();
    auto logPath = (config.dataDir() / "logs" / "omninet.log").string();
    std::filesystem::create_directories(config.dataDir() / "logs");
    
    Logger::instance().setLogFile(logPath);
    Logger::instance().setLevel(LogLevel::DEBUG);
    Logger::instance().setMaxFileSize(10 * 1024 * 1024);
    Logger::instance().setMaxRotatedFiles(5);
    
    Logger::instance().info("Server starting...");
    
    // Create server
    HTTP::Server::Config serverConfig;
    serverConfig.port = 8080;
    HTTP::Server server(serverConfig);
    
    server.get("/api/hello", [](const HTTP::Request& req, HTTP::Response& resp) {
        Logger::instance().debug("Handling /api/hello request");
        resp.setContentType("application/json");
        resp.setBody(R"({"hello": "world"})");
    });
    
    server.adminHandler().mount(server);
    
    server.start();
    
    Logger::instance().info("Server stopped");
    OmniNet::cleanup();
}
```

## Data Directory Structure

```
~/.omninet/
├── config.json        # Configuration
├── logs/
│   ├── omninet.log    # Current log
│   ├── omninet.log.1  # Rotated (oldest)
│   ├── omninet.log.2
│   └── ...
├── dashboard.db       # Dashboard database
└── certs/             # TLS certificates
```

## Thread Safety

Logger is fully thread-safe:
- Internal mutex protects queue operations
- Safe to call from multiple threads simultaneously
- Each thread logs with its own thread ID