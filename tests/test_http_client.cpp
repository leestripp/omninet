// OmniNet - HttpClient tests (v0.8.0 Phases 1-5)
// Verifies sync GET/POST, TLS (https), async, redirects, timeouts.

#include "http/HttpClient.h"
#include "net/Socket.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

using namespace OmniNet;
using namespace OmniNet::HTTP;

struct LocalServer {
    int port = 0;
    std::thread thread;
    std::atomic<bool> done{false};
    std::string last_method;
    std::string last_path;
    std::string last_body;
    int response_status = 200;
    std::string response_body = "hello";
    std::map<std::string, std::string> response_headers;
    std::string redirect_path;
    std::string redirect_target;

    void start(int p) {
        port = p;
        thread = std::thread([this] { run(); });
        for (int i = 0; i < 200; ++i) {
            if (try_connect()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void stop() {
        done.store(true);
        if (thread.joinable()) thread.join();
    }

    bool try_connect() {
        Socket s;
        try {
            s.connect("127.0.0.1", static_cast<uint16_t>(port));
            return s.isOpen();
        } catch (...) {
            return false;
        }
    }

    void serve_one(std::unique_ptr<Socket> client) {
        if (!client) return;
        try {
            std::string raw;
            uint8_t buf[4096];
            std::span<uint8_t> buf_span(buf, sizeof(buf));
            while (true) {
                size_t n = client->recv(buf_span);
                if (n == 0) break;
                raw.append(reinterpret_cast<const char*>(buf), n);
                if (raw.find("\r\n\r\n") != std::string::npos) {
                    auto cl = raw.find("Content-Length:");
                    if (cl == std::string::npos) break;
                    size_t hdr_end = raw.find("\r\n\r\n");
                    if (hdr_end == std::string::npos) break;
                    size_t body_start = hdr_end + 4;
                    size_t cl_val = 0;
                    for (size_t i = cl + 15; i < raw.size() &&
                         raw[i] >= '0' && raw[i] <= '9'; ++i) {
                        cl_val = cl_val * 10 + (raw[i] - '0');
                    }
                    if (raw.size() - body_start >= cl_val) break;
                }
            }

            auto eol = raw.find("\r\n");
            std::string line = (eol == std::string::npos)
                ? raw : raw.substr(0, eol);
            auto sp1 = line.find(' ');
            auto sp2 = (sp1 == std::string::npos) ? std::string::npos
                : line.find(' ', sp1 + 1);
            if (sp1 != std::string::npos) {
                last_method = line.substr(0, sp1);
                last_path = (sp2 == std::string::npos) ? "/" :
                    line.substr(sp1 + 1, sp2 - sp1 - 1);
            }
            auto hdr_end = raw.find("\r\n\r\n");
            if (hdr_end != std::string::npos) {
                last_body = raw.substr(hdr_end + 4);
            }

            int status = response_status;
            std::string body = response_body;
            std::map<std::string, std::string> headers = response_headers;
            if (!redirect_path.empty() && last_path == redirect_path) {
                status = 302;
                body = "";
                headers["Location"] = redirect_target;
            }

            std::string resp = "HTTP/1.1 " +
                std::to_string(status) + " OK\r\n";
            resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            resp += "Content-Type: text/plain\r\n";
            for (const auto& [k, v] : headers) {
                resp += k + ": " + v + "\r\n";
            }
            resp += "Connection: close\r\n\r\n";
            resp += body;
            client->sendAll(std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(resp.data()),
                resp.size()));
            client->close();
        } catch (...) {
            // ignore
        }
    }

    void run() {
        ServerSocket server;
        try {
            server.bindAndListen(static_cast<uint16_t>(port),
                                 "127.0.0.1", 5);
        } catch (...) {
            return;
        }
        while (!done.load()) {
            try {
                auto client = server.acceptWithTimeout(
                    std::chrono::milliseconds(50));
                serve_one(std::move(client));
            } catch (...) {
                // ignore
            }
        }
    }
};

int passed = 0;
int failed = 0;

#define EXPECT(cond) do { \
    if (cond) { ++passed; std::cout << "  PASS: " #cond "\n"; } \
    else      { ++failed; std::cerr << "  FAIL: " #cond \
                          << " at " << __FILE__ << ":" << __LINE__ << "\n"; } \
} while (0)

void test_url_parse() {
    std::cout << "test_url_parse\n";
    ClientRequest req;
    req.url("http://example.com/foo?bar=1");
    EXPECT(req.url() == "http://example.com/foo?bar=1");
}

void test_invalid_url() {
    std::cout << "test_invalid_url\n";
    ClientRequest req;
    req.url("not a url");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(!result.has_value());
    if (!result.has_value()) {
        EXPECT(result.error() == ClientError::InvalidUrl);
    }
}

void test_get() {
    std::cout << "test_get\n";
    LocalServer srv;
    srv.response_body = "hello world";
    srv.start(18080);
    ClientRequest req;
    req.method("GET").url("http://127.0.0.1:18080/test");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 200);
        EXPECT(result->bodyAsString() == "hello world");
        EXPECT(srv.last_method == "GET");
        EXPECT(srv.last_path == "/test");
    }
    srv.stop();
}

void test_post() {
    std::cout << "test_post\n";
    LocalServer srv;
    srv.response_body = "ack";
    srv.start(18081);
    ClientRequest req;
    req.method("POST")
       .url("http://127.0.0.1:18081/api")
       .header("Content-Type", "text/plain")
       .body(std::string("payload"));
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 200);
        EXPECT(srv.last_method == "POST");
        EXPECT(srv.last_body == "payload");
    }
    srv.stop();
}

void test_404() {
    std::cout << "test_404\n";
    LocalServer srv;
    srv.response_status = 404;
    srv.response_body = "not found";
    srv.start(18082);
    ClientRequest req;
    req.url("http://127.0.0.1:18082/missing");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 404);
        EXPECT(result->bodyAsString() == "not found");
    }
    srv.stop();
}

void test_headers() {
    std::cout << "test_headers\n";
    LocalServer srv;
    srv.response_headers["X-Custom"] = "value1";
    srv.start(18083);
    ClientRequest req;
    req.url("http://127.0.0.1:18083/x");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->header("X-Custom") == "value1");
        EXPECT(result->header("Content-Type") == "text/plain");
    }
    srv.stop();
}

// ----- Phase 2: async -----

void test_async() {
    std::cout << "test_async\n";
    LocalServer srv;
    srv.response_body = "async hello";
    srv.start(18084);
    ClientRequest req;
    req.url("http://127.0.0.1:18084/async");
    auto fut = Client::get_instance().send(std::move(req));
    EXPECT(fut.valid());
    auto result = fut.get();
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->bodyAsString() == "async hello");
    }
    srv.stop();
}

// ----- Phase 4: redirects -----

void test_redirect_follow() {
    std::cout << "test_redirect_follow\n";
    LocalServer srv;
    srv.response_body = "final";
    srv.redirect_path = "/start";
    srv.redirect_target = "http://127.0.0.1:18085/final";
    srv.start(18085);
    ClientRequest req;
    req.url("http://127.0.0.1:18085/start");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 200);
        EXPECT(result->bodyAsString() == "final");
        EXPECT(srv.last_path == "/final");
    }
    srv.stop();
}

void test_redirect_disabled() {
    std::cout << "test_redirect_disabled\n";
    LocalServer srv;
    srv.response_body = "should not be seen";
    srv.redirect_path = "/start";
    srv.redirect_target = "http://127.0.0.1:18086/final";
    srv.start(18086);
    ClientConfig cfg;
    cfg.follow_redirects = false;
    ClientRequest req;
    req.url("http://127.0.0.1:18086/start");
    auto result = Client::get_instance().send_blocking(req, cfg);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 302);
    }
    srv.stop();
}

// ----- Phase 5: timeout -----

void test_request_timeout() {
    std::cout << "test_request_timeout\n";
    LocalServer srv;
    srv.response_body = "";
    srv.start(18087);
    ClientConfig cfg;
    cfg.request_timeout = std::chrono::seconds(1);
    ClientRequest req;
    req.url("http://127.0.0.1:18087/x");
    auto start = std::chrono::steady_clock::now();
    Client::get_instance().send_blocking(req, cfg);
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT(elapsed < 10);
    srv.stop();
}

int main() {
    test_url_parse();
    test_invalid_url();
    test_get();
    test_post();
    test_404();
    test_headers();
    test_async();
    test_redirect_follow();
    test_redirect_disabled();
    test_request_timeout();

    std::cout << "\nResults: " << passed << " passed, "
              << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}