// OmniNet - HttpClient tests (v0.8.x Phases 1-7)

#include "http/HttpClient.h"
#include "net/Socket.h"

#include <atomic>
#include <chrono>
#include <csignal>
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
    bool use_chunked = false;
    int chunk_count = 3;
    int chunk_size = 4;
    int header_delay_ms = 0;
    int keep_open_ms = 0;

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
        try { s.connect("127.0.0.1", static_cast<uint16_t>(port));
              return s.isOpen(); }
        catch (...) { return false; }
    }

    void serve_one(std::unique_ptr<Socket> client) {
        if (!client) return;
        try {
            std::string raw;
            uint8_t buf[4096]{};
            std::span<uint8_t> buf_span(buf, sizeof(buf));
            while (true) {
                size_t n = client->recv(buf_span);
                if (n == 0) break;
                raw.append(reinterpret_cast<const char*>(buf), n);
                if (raw.find("\r\n\r\n") != std::string::npos) {
                    if (raw.find("Transfer-Encoding: chunked") != std::string::npos) {
                        size_t hdr_end = raw.find("\r\n\r\n");
                        size_t body_start = hdr_end + 4;
                        while (raw.find("0\r\n\r\n", body_start) == std::string::npos) {
                            size_t n2 = client->recv(buf_span);
                            if (n2 == 0) break;
                            raw.append(reinterpret_cast<const char*>(buf), n2);
                        }
                        break;
                    }
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

            if (raw.find("Transfer-Encoding: chunked") != std::string::npos) {
                size_t hdr_end = raw.find("\r\n\r\n");
                if (hdr_end != std::string::npos) {
                    size_t p = hdr_end + 4;
                    while (p < raw.size()) {
                        auto ceol = raw.find("\r\n", p);
                        if (ceol == std::string::npos) break;
                        std::string size_line = raw.substr(p, ceol - p);
                        auto semi = size_line.find(';');
                        if (semi != std::string::npos) size_line.resize(semi);
                        size_t csz = 0;
                        for (char c : size_line) {
                            int d = (c >= '0' && c <= '9') ? c - '0'
                                   : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                                   : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                            if (d < 0) break;
                            csz = csz * 16 + static_cast<size_t>(d);
                        }
                        if (csz == 0) break;
                        p = ceol + 2;
                        last_body.append(raw.data() + p,
                                         raw.data() + p + csz);
                        p += csz;
                        if (p < raw.size() && raw[p] == '\r') ++p;
                        if (p < raw.size() && raw[p] == '\n') ++p;
                    }
                }
            } else {
                auto hdr_end = raw.find("\r\n\r\n");
                if (hdr_end != std::string::npos) {
                    last_body = raw.substr(hdr_end + 4);
                }
            }

            if (header_delay_ms > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(header_delay_ms));
            }

            int status = response_status;
            std::string body = response_body;
            std::map<std::string, std::string> headers = response_headers;
            if (!redirect_path.empty() && last_path == redirect_path) {
                status = 302; body = "";
                headers["Location"] = redirect_target;
            }

            std::string resp = "HTTP/1.1 " +
                std::to_string(status) + " OK\r\n";
            if (use_chunked) {
                resp += "Transfer-Encoding: chunked\r\n";
            } else {
                resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            }
            resp += "Content-Type: text/plain\r\n";
            for (const auto& [k, v] : headers) {
                resp += k + ": " + v + "\r\n";
            }
            resp += keep_open_ms > 0 ? "Connection: keep-alive\r\n\r\n"
                                     : "Connection: close\r\n\r\n";
            client->sendAll(std::span<const uint8_t>(
                reinterpret_cast<const uint8_t*>(resp.data()),
                resp.size()));
            if (use_chunked) {
                for (int i = 0; i < chunk_count; ++i) {
                    std::string chunk(chunk_size, 'a' + i);
                    std::string cheader = std::to_string(chunk_size) + "\r\n";
                    client->sendAll(std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(cheader.data()),
                        cheader.size()));
                    client->sendAll(std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(chunk.data()),
                        chunk.size()));
                    client->sendAll(std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>("\r\n"), 2));
                }
                client->sendAll(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>("0\r\n\r\n"), 5));
            } else {
                client->sendAll(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(body.data()),
                    body.size()));
            }
            if (keep_open_ms > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(keep_open_ms));
            }
            client->close();
        } catch (...) {}
    }

    void run() {
        ServerSocket server;
        try {
            server.bindAndListen(static_cast<uint16_t>(port),
                                 "127.0.0.1", 5);
        } catch (...) { return; }
        while (!done.load()) {
            try {
                auto client = server.acceptWithTimeout(
                    std::chrono::milliseconds(50));
                serve_one(std::move(client));
            } catch (...) {}
        }
    }
};

int passed = 0, failed = 0;
#define EXPECT(c) do { \
    if (c) { ++passed; std::cout << "  PASS: " #c "\n"; } \
    else { ++failed; std::cerr << "  FAIL: " #c \
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
    if (!result.has_value()) EXPECT(result.error() == ClientError::InvalidUrl);
}
void test_get() {
    std::cout << "test_get\n"; std::cout.flush();
    LocalServer srv;
    srv.response_body = "hello world"; srv.start(18100);
    std::cout << "test_get after start\n"; std::cout.flush();
    ClientRequest req;
    req.method("GET").url("http://127.0.0.1:18100/test");
    std::cout << "test_get before send\n"; std::cout.flush();
    auto result = Client::get_instance().send_blocking(req);
    std::cout << "test_get after send has=" << result.has_value() << "\n"; std::cout.flush();
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 200);
        EXPECT(result->bodyAsString() == "hello world");
        EXPECT(srv.last_method == "GET");
    }
    srv.stop();
}
void test_post() {
    std::cout << "test_post\n";
    LocalServer srv;
    srv.response_body = "ack"; srv.start(18081);
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
    srv.response_body = "not found"; srv.start(18082);
    ClientRequest req;
    req.url("http://127.0.0.1:18082/missing");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) EXPECT(result->statusCode() == 404);
    srv.stop();
}
void test_headers() {
    std::cout << "test_headers\n";
    LocalServer srv;
    srv.response_headers["X-Custom"] = "value1"; srv.start(18083);
    ClientRequest req;
    req.url("http://127.0.0.1:18083/x");
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) EXPECT(result->header("X-Custom") == "value1");
    srv.stop();
}
void test_async() {
    std::cout << "test_async\n";
    LocalServer srv;
    srv.response_body = "async hello"; srv.start(18084);
    ClientRequest req;
    req.url("http://127.0.0.1:18084/async");
    auto fut = Client::get_instance().send(std::move(req));
    EXPECT(fut.valid());
    auto result = fut.get();
    EXPECT(result.has_value());
    if (result.has_value()) EXPECT(result->bodyAsString() == "async hello");
    srv.stop();
}
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
    }
    srv.stop();
}
void test_redirect_disabled() {
    std::cout << "test_redirect_disabled\n";
    LocalServer srv;
    srv.redirect_path = "/start";
    srv.redirect_target = "http://127.0.0.1:18086/final";
    srv.start(18086);
    ClientConfig cfg;
    cfg.follow_redirects = false;
    ClientRequest req;
    req.url("http://127.0.0.1:18086/start");
    auto result = Client::get_instance().send_blocking(req, cfg);
    EXPECT(result.has_value());
    if (result.has_value()) EXPECT(result->statusCode() == 302);
    srv.stop();
}
void test_request_timeout() {
    std::cout << "test_request_timeout\n";
    LocalServer srv; srv.start(18087);
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

void test_streaming_response_chunked() {
    std::cout << "test_streaming_response_chunked\n";
    LocalServer srv;
    srv.use_chunked = true;
    srv.chunk_count = 5;
    srv.chunk_size = 8;
    srv.start(18090);
    ClientRequest req;
    req.url("http://127.0.0.1:18090/stream");
    auto result = Client::get_instance().send_streaming(std::move(req));
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT((*result)->statusCode() == 200);
        std::vector<uint8_t> all = (*result)->read_all();
        EXPECT(all.size() == 5 * 8u);
        std::string expected =
            std::string(8, 'a') +
            std::string(8, 'b') +
            std::string(8, 'c') +
            std::string(8, 'd') +
            std::string(8, 'e');
        std::string got(reinterpret_cast<const char*>(all.data()),
                        all.size());
        EXPECT(got == expected);
        EXPECT((*result)->eof());
    }
    srv.stop();
}

void test_streaming_response_contentlength() {
    std::cout << "test_streaming_response_contentlength\n";
    LocalServer srv;
    srv.response_body = std::string(64, 'Z');
    srv.start(18091);
    ClientRequest req;
    req.url("http://127.0.0.1:18091/cl");
    auto result = Client::get_instance().send_streaming(std::move(req));
    EXPECT(result.has_value());
    if (result.has_value()) {
        std::vector<uint8_t> all = (*result)->read_all();
        EXPECT(all.size() == 64u);
        EXPECT((*result)->eof());
    }
    srv.stop();
}

void test_streaming_request_chunked() {
    std::cout << "test_streaming_request_chunked\n";
    LocalServer srv;
    srv.response_body = "ack";
    srv.start(18092);
    ClientRequest req;
    req.method("POST").url("http://127.0.0.1:18092/upload");
    int call_count = 0;
    req.set_body_writer([&call_count]() -> std::vector<uint8_t> {
        ++call_count;
        if (call_count == 1) return std::vector<uint8_t>(3, 'A');
        if (call_count == 2) return std::vector<uint8_t>(4, 'B');
        if (call_count == 3) return std::vector<uint8_t>(5, 'C');
        return {};
    });
    auto result = Client::get_instance().send_blocking(req);
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->statusCode() == 200);
        EXPECT(srv.last_method == "POST");
        EXPECT(srv.last_body == "AAABBBBCCCCC");
    }
    srv.stop();
}

void test_streaming_read_some_partial() {
    std::cout << "test_streaming_read_some_partial\n";
    LocalServer srv;
    srv.use_chunked = true;
    srv.chunk_count = 10;
    srv.chunk_size = 100;
    srv.start(18093);
    ClientRequest req;
    req.url("http://127.0.0.1:18093/big");
    auto result = Client::get_instance().send_streaming(std::move(req));
    EXPECT(result.has_value());
    if (result.has_value()) {
        auto first = (*result)->read_some();
        EXPECT(first.has_value());
        if (first.has_value()) {
            EXPECT(!first->empty());
        }
    }
    srv.stop();
}

void test_streaming_header_timeout() {
    std::cout << "test_streaming_header_timeout\n";
    // Regression: send_streaming() must honor cfg.request_timeout for the
    // time-to-first-byte of the response headers, not a hard-coded 2s wait.
    {
        LocalServer srv;
        srv.header_delay_ms = 3000;
        srv.start(18094);
        ClientConfig cfg;
        cfg.request_timeout = std::chrono::seconds(10);
        ClientRequest req;
        req.url("http://127.0.0.1:18094/slow");
        auto result = Client::get_instance().send_streaming(std::move(req), cfg);
        EXPECT(result.has_value());
        if (result.has_value()) {
            EXPECT((*result)->statusCode() == 200);
        }
        srv.stop();
    }
    {
        LocalServer srv;
        srv.header_delay_ms = 3000;
        srv.start(18095);
        ClientConfig cfg;
        cfg.request_timeout = std::chrono::seconds(1);
        ClientRequest req;
        req.url("http://127.0.0.1:18095/slow");
        auto start = std::chrono::steady_clock::now();
        auto result = Client::get_instance().send_streaming(std::move(req), cfg);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        EXPECT(!result.has_value());
        if (!result.has_value()) {
            EXPECT(result.error() == ClientError::Timeout);
        }
        // Tolerant lower bound: millisecond truncation can shave ~1ms.
        EXPECT(elapsed >= 900 && elapsed < 2500);
        srv.stop();
    }
}

void test_blocking_content_length_keep_alive() {
    std::cout << "test_blocking_content_length_keep_alive\n";
    LocalServer srv;
    srv.response_body = "complete response";
    srv.keep_open_ms = 1000;
    srv.start(18096);
    ClientConfig cfg;
    cfg.request_timeout = std::chrono::seconds(5);
    ClientRequest req;
    req.url("http://127.0.0.1:18096/keep-alive");
    const auto start = std::chrono::steady_clock::now();
    auto result = Client::get_instance().send_blocking(req, cfg);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT(result.has_value());
    if (result.has_value()) {
        EXPECT(result->bodyAsString() == "complete response");
    }
    EXPECT(elapsed < 500);
    srv.stop();
}

int main() {
    signal(SIGPIPE, SIG_IGN);
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
    test_streaming_response_chunked();
    test_streaming_response_contentlength();
    test_streaming_request_chunked();
    test_streaming_read_some_partial();
    test_streaming_header_timeout();
    test_blocking_content_length_keep_alive();

    std::cout << "\nResults: " << passed << " passed, "
              << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
