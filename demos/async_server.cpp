// OmniNet Async Server Integration Demo
// Showcases v0.4.0+ features: AsyncServer, AsyncDatabase, AsyncLogger,
// AsyncFile, std::expected<T, E>, gzip compression, and more.
#include "OmniNet.h"
#include "async/Task.h"
#include "async/Executor.h"
#include "async/AsyncHandler.h"
#include "async/AsyncLogger.h"
#include "io/IoContext.h"
#include "io/EpollContext.h"
#include "io/AsyncFile.h"
#include "server/AsyncServer.h"
#include "thread/ThreadPool.h"
#include "db/AsyncDatabase.h"
#include "db/DatabasePool.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "utils/Expected.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace OmniNet;
using namespace OmniNet::Async;
using namespace OmniNet::IO;
using namespace OmniNet::Server;
using namespace OmniNet::DB;
using namespace std::chrono_literals;

// ============================================================================
// Shared resources (using globals for simplicity in the demo)
// ============================================================================

struct AppContext {
    std::unique_ptr<Thread::ThreadPool> pool;
    std::unique_ptr<ThreadPoolExecutor> exec;
    std::unique_ptr<EpollIoContext> ioContext;
    std::unique_ptr<AsyncLogger> logger;
    std::unique_ptr<DatabasePool> dbPool;
    std::atomic<uint64_t> requestCount{0};
    std::atomic<uint64_t> connectionsTotal{0};
    bool setupDone = false;

    void setup() {
        if (setupDone) return;

        // Thread pool
        Thread::ThreadPool::Config poolCfg;
        poolCfg.workerCount = 4;
        pool = std::make_unique<Thread::ThreadPool>(poolCfg);
        exec = std::make_unique<ThreadPoolExecutor>(*pool);
        ioContext = std::make_unique<EpollIoContext>(*exec);

        // Async logger
        AsyncLogger::Config logCfg;
        logCfg.maxQueueSize = 50000;
        logger = std::make_unique<AsyncLogger>(*pool, logCfg);
        logger->setLevel(LogLevel::INFO);
        logger->setLogFile("/tmp/async_server_demo.log");

        // Database pool (file-backed so connections share data; in-memory
        // SQLite databases are per-connection and wouldn't work with a pool)
        const char* dbPath = std::getenv("ASYNC_DEMO_DB");
        std::string path = dbPath ? dbPath : "/tmp/async_server_demo.db";
        // Start fresh each run
        std::remove(path.c_str());
        dbPool = std::make_unique<DatabasePool>(*exec, path);

        // Seed database (acquire a connection from the pool so the seeded
        // data lives in the same database that handlers will query)
        auto seedDb = dbPool->acquire();
        if (seedDb) {
            // execute() returns a coroutine Task; we must wait for it to
            // complete (blocking_get) so the CREATE/INSERT actually run
            OmniNet::Async::blocking_get(seedDb->execute(
                "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT)"));
            OmniNet::Async::blocking_get(seedDb->execute(
                "INSERT INTO users (id, name, email) VALUES (1, 'Alice', 'alice@example.com')"));
            OmniNet::Async::blocking_get(seedDb->execute(
                "INSERT INTO users (id, name, email) VALUES (2, 'Bob', 'bob@example.com')"));
            OmniNet::Async::blocking_get(seedDb->execute(
                "INSERT INTO users (id, name, email) VALUES (3, 'Charlie', 'charlie@example.com')"));
            dbPool->release(seedDb);
        }

        setupDone = true;
    }

    void teardown() {
        if (logger) logger->shutdown();
    }
};

// Global app context (for demo simplicity)
static AppContext* g_app = nullptr;

// ============================================================================
// Route handlers
// ============================================================================

// GET / - Welcome message
Async::Task<HTTP::Response> handle_root(const HTTP::Request& req, Async::Executor& exec) {
    (void)req; (void)exec;
    HTTP::Response resp;
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(R"({
        "name": "OmniNet AsyncServer Demo",
        "version": "0.5.0",
        "features": ["AsyncServer", "AsyncDatabase", "AsyncLogger", "AsyncFile", "std::expected"]
    })");
    co_return resp;
}

// GET /api/users - List all users from database
Async::Task<HTTP::Response> handle_users_list(const HTTP::Request& req, Async::Executor& exec) {
    (void)req;
    (void)exec;
    if (!g_app) {
        HTTP::Response resp = HTTP::Response::internalError("No app context");
        co_return resp;
    }

    g_app->requestCount.fetch_add(1);
    g_app->logger->info("Handling /api/users request");

    auto pool = g_app->dbPool.get();
    auto dbOpt = co_await pool->acquire_async();
    if (!dbOpt) {
        g_app->logger->error("Failed to acquire database connection");
        HTTP::Response resp = HTTP::Response::internalError("DB unavailable");
        co_return resp;
    }

    auto queryResult = co_await dbOpt->query("SELECT id, name, email FROM users");

    std::string body = "[";
    bool first = true;
    for (const auto& row : queryResult) {
        if (!first) body += ",";
        first = false;
        body += R"({"id": )" + (row.size() > 0 ? row[0] : "0") +
               R"(, "name": ")" + (row.size() > 1 ? row[1] : "") +
               R"(", "email": ")" + (row.size() > 2 ? row[2] : "") + R"("})";
    }
    body += "]";

    pool->release(dbOpt);

    HTTP::Response resp;
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(body);
    co_return resp;
}

// GET /api/users/:id - Get single user (using std::expected)
Async::Task<HTTP::Response> handle_user_get(const HTTP::Request& req, Async::Executor& exec) {
    (void)exec;
    if (!g_app) {
        HTTP::Response resp = HTTP::Response::internalError("No app context");
        co_return resp;
    }

    // Extract ID from path
    std::string path = std::string(req.path());
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == path.size() - 1) {
        HTTP::Response resp = HTTP::Response::badRequest("Invalid user ID");
        co_return resp;
    }
    std::string idStr = path.substr(lastSlash + 1);

    g_app->logger->info("Looking up user ID: " + idStr);

    auto pool = g_app->dbPool.get();
    auto dbOpt = co_await pool->acquire_async();
    if (!dbOpt) {
        HTTP::Response resp = HTTP::Response::internalError("DB unavailable");
        co_return resp;
    }

    // Use query_one (returns std::optional which is awaitable via Task)
    std::string sql = "SELECT name, email FROM users WHERE id = " + idStr;
    auto queryResult = co_await dbOpt->query_one(sql);

    pool->release(dbOpt);

    if (!queryResult.has_value()) {
        HTTP::Response resp = HTTP::Response::notFound("User not found");
        co_return resp;
    }

    const auto& row = *queryResult;
    HTTP::Response resp;
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody(R"({"id": )" + idStr +
                 R"(, "name": ")" + (row.size() > 0 ? row[0] : "") +
                 R"(", "email": ")" + (row.size() > 1 ? row[1] : "") + R"("})");
    co_return resp;
}

// GET /api/stats - Server statistics
Async::Task<HTTP::Response> handle_stats(const HTTP::Request& req, Async::Executor& exec) {
    (void)req;
    (void)exec;
    if (!g_app) {
        HTTP::Response resp = HTTP::Response::internalError();
        co_return resp;
    }

    const auto& logStats = g_app->logger->stats();
    const auto& poolStats = g_app->pool->stats();

    HTTP::Response resp;
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody("{"
        "\"requests_total\": " + std::to_string(g_app->requestCount.load()) + ","
        "\"logs_queued\": " + std::to_string(logStats.messagesQueued.load()) + ","
        "\"logs_written\": " + std::to_string(logStats.messagesWritten.load()) + ","
        "\"logs_dropped\": " + std::to_string(logStats.messagesDropped.load()) + ","
        "\"pool_active_tasks\": " + std::to_string(poolStats.activeTasks) + ","
        "\"pool_executed\": " + std::to_string(poolStats.totalExecuted) +
    "}");
    co_return resp;
}

// GET /api/logtest - Test logger with 1000 messages
Async::Task<HTTP::Response> handle_logtest(const HTTP::Request& req, Async::Executor& exec) {
    (void)req;
    (void)exec;
    if (!g_app) {
        HTTP::Response resp = HTTP::Response::internalError();
        co_return resp;
    }

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) {
        g_app->logger->info("Log test message " + std::to_string(i));
    }
    g_app->logger->flush();
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();

    HTTP::Response resp;
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setContentType("application/json");
    resp.setBody("{"
        "\"messages\": 1000,"
        "\"elapsed_us\": " + std::to_string(us) + ","
        "\"throughput_per_sec\": " + std::to_string(1000000 / (us + 1)) +
    "}");
    co_return resp;
}

// GET /static/* - Serve static files using AsyncFile
Async::Task<HTTP::Response> handle_static(const HTTP::Request& req, Async::Executor& exec) {
    // Extract path after /static/
    std::string path = std::string(req.path());
    size_t pos = path.find("/static/");
    if (pos == std::string::npos) {
        HTTP::Response resp = HTTP::Response::badRequest();
        co_return resp;
    }
    std::string filePath = path.substr(pos + 8);  // Skip "/static/"

    // Add static/ prefix
    filePath = "static/" + filePath;

    AsyncFile file(exec);
    auto openTask = file.open(filePath, "r");
    openTask.resume();

    if (!file.isOpen()) {
        HTTP::Response resp = HTTP::Response::notFound(
            "File not found: " + filePath);
        co_return resp;
    }

    auto readTask = file.read_all();
    readTask.resume();
    std::vector<uint8_t> data = std::move(readTask).result();

    file.close().resume();

    HTTP::Response resp;
    resp.setStatus(HTTP::StatusCode::OK);
    resp.setBody(std::move(data));
    co_return resp;
}

// ============================================================================
// Helper: read entire file (synchronous, for startup)
// ============================================================================
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
}

// ============================================================================
// Main
// ============================================================================

int main() {
    // Create static directory and sample files
    // v0.6.0 fix (bug #10): use std::filesystem::create_directories
    // instead of system("mkdir -p static") — avoids both the ignored
    // return value and the shell dependency.
    std::error_code ec;
    std::filesystem::create_directories("static", ec);
    if (ec) {
        std::fprintf(stderr, "warning: could not create static/ directory: %s\n",
                     ec.message().c_str());
    }
    std::cout << "Showcasing v0.4.0+ features\n" << std::endl;

    OmniNet::init();

    // Create static directory and sample files
    system("mkdir -p static");
    if (readFile("static/hello.txt").empty()) {
        std::ofstream f("static/hello.txt");
        f << "Hello from AsyncFile!\nThis is a static file served via async I/O.\n";
    }
    if (readFile("static/index.html").empty()) {
        std::ofstream f("static/index.html");
        f << "<!DOCTYPE html><html><head><title>OmniNet Async Demo</title></head>"
             "<body><h1>OmniNet AsyncServer</h1><p>Visit <a href=\"/api/users\">/api/users</a></p></body></html>";
    }

    // Create shared app context
    AppContext app;
    app.setup();
    g_app = &app;

    // Configure server
    AsyncServer::Config serverConfig;
    serverConfig.address = "127.0.0.1";
    serverConfig.port = 19080;
    serverConfig.maxConnections = 1024;
    serverConfig.useNagle = true;
    serverConfig.reuseAddress = true;

    // Create server with our IoContext
    AsyncServer server(*app.ioContext, serverConfig);

    // Register routes
    app.logger->info("Registering routes...");

    server.get("/", Async::AsyncHandler{
        [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
            co_return co_await handle_root(req, exec);
        }
    });

    server.get("/api/users", Async::AsyncHandler{
        [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
            co_return co_await handle_users_list(req, exec);
        }
    });

    server.get("/api/users/:id", Async::AsyncHandler{
        [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
            co_return co_await handle_user_get(req, exec);
        }
    });

    server.get("/api/stats", Async::AsyncHandler{
        [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
            co_return co_await handle_stats(req, exec);
        }
    });

    server.get("/api/logtest", Async::AsyncHandler{
        [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
            co_return co_await handle_logtest(req, exec);
        }
    });

    server.get("/static/*", Async::AsyncHandler{
        [](const HTTP::Request& req, Async::Executor& exec) -> Async::Task<HTTP::Response> {
            co_return co_await handle_static(req, exec);
        }
    });

    // Print endpoints
    std::cout << "Starting server on " << serverConfig.address
              << ":" << serverConfig.port << std::endl;
    std::cout << "\nEndpoints:" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/             (info)" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/api/users    (DB query)" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/api/users/1  (single user)" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/api/stats    (stats)" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/api/logtest  (1000 logs)" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/static/hello.txt" << std::endl;
    std::cout << "  GET  http://localhost:" << serverConfig.port << "/static/index.html" << std::endl;
    std::cout << "\nPress Ctrl+C to stop." << std::endl;

    app.logger->info("Server started on " + serverConfig.address + ":" +
                     std::to_string(serverConfig.port));

    // Run server in background
    server.run_async();

    // Wait for interrupt
    std::cout << "\nServer is running in background. Press Enter to stop..." << std::endl;
    std::cin.get();

    app.logger->info("Server stopping");
    server.stop();

    std::cout << "Server stopped." << std::endl;
    app.teardown();
    g_app = nullptr;
    OmniNet::cleanup();

    return 0;
}
