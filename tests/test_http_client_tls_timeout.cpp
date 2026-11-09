#include "crypto/Crypto.h"
#include "http/HttpClient.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#endif

using namespace OmniNet;
using namespace OmniNet::HTTP;

#ifndef _WIN32
namespace {

struct Fd {
    int value = -1;
    ~Fd() { if (value >= 0) ::close(value); }
    Fd() = default;
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

struct PemFiles {
    std::filesystem::path cert;
    std::filesystem::path key;

    PemFiles() {
        const auto [certPem, keyPem] = Crypto::generateSelfSignedCert("localhost", 1);
        if (certPem.empty() || keyPem.empty()) throw std::runtime_error("certificate generation failed");
        const auto base = std::filesystem::temp_directory_path() /
            ("omninet-tls-timeout-" + std::to_string(::getpid()));
        cert = base;
        key = base;
        cert += ".crt";
        key += ".key";
        write(cert, certPem);
        write(key, keyPem);
    }

    ~PemFiles() {
        std::error_code ec;
        std::filesystem::remove(cert, ec);
        std::filesystem::remove(key, ec);
    }

    static void write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) throw std::runtime_error("failed to write temporary PEM file");
    }
};

class PartialTlsRecordServer {
public:
    PartialTlsRecordServer(const std::filesystem::path& cert,
                           const std::filesystem::path& key)
        : cert_(cert), key_(key) {}

    ~PartialTlsRecordServer() { stop(); }

    void start() {
        listener_.value = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listener_.value < 0) throw std::runtime_error("socket failed");
        int reuse = 1;
        ::setsockopt(listener_.value, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (::bind(listener_.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(listener_.value, 1) != 0) {
            throw std::runtime_error("bind/listen failed");
        }
        socklen_t size = sizeof(address);
        if (::getsockname(listener_.value, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
            throw std::runtime_error("getsockname failed");
        }
        port_ = ntohs(address.sin_port);
        worker_ = std::thread([this] { run(); });
    }

    uint16_t port() const { return port_; }

    void stop() {
        finish_.store(true);
        if (worker_.joinable()) worker_.join();
    }

private:
    void run() {
        Fd client;
        client.value = ::accept(listener_.value, nullptr, nullptr);
        if (client.value < 0) return;
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
            SSL_CTX_new(TLS_server_method()), SSL_CTX_free);
        std::unique_ptr<SSL, decltype(&SSL_free)> ssl(nullptr, SSL_free);
        if (!context ||
            SSL_CTX_use_certificate_file(context.get(), cert_.c_str(), SSL_FILETYPE_PEM) != 1 ||
            SSL_CTX_use_PrivateKey_file(context.get(), key_.c_str(), SSL_FILETYPE_PEM) != 1) {
            return;
        }
        ssl.reset(SSL_new(context.get()));
        if (!ssl || SSL_set_fd(ssl.get(), client.value) != 1 || SSL_accept(ssl.get()) != 1) return;

        char request[4096];
        if (SSL_read(ssl.get(), request, sizeof(request)) <= 0) return;
        const std::string response =
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok";

        // Encrypt a complete response into a memory BIO, then put only its
        // first few bytes on the wire. A blocking SSL_read() waits for the
        // remainder even though the client already passed waitForRead().
        BIO* memory = BIO_new(BIO_s_mem());
        if (!memory) return;
        SSL_set_bio(ssl.get(), nullptr, memory);
        if (SSL_write(ssl.get(), response.data(), static_cast<int>(response.size())) <= 0) return;
        char* encrypted = nullptr;
        const long length = BIO_get_mem_data(memory, &encrypted);
        if (length <= 0 || !encrypted) return;
        const std::string record(encrypted, static_cast<size_t>(length));
        BIO* socketBio = BIO_new_socket(client.value, BIO_NOCLOSE);
        if (!socketBio) return;
        SSL_set_bio(ssl.get(), nullptr, socketBio);

        const size_t partialSize = std::min<size_t>(8, record.size());
        if (::send(client.value, record.data(), partialSize, 0) !=
            static_cast<ssize_t>(partialSize)) {
            return;
        }
        for (int i = 0; i < 500 && !finish_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::filesystem::path cert_;
    std::filesystem::path key_;
    Fd listener_;
    std::thread worker_;
    std::atomic<bool> finish_{false};
    uint16_t port_ = 0;
};

}  // namespace
#endif

int main() {
    std::signal(SIGPIPE, SIG_IGN);
#ifndef _WIN32
    PemFiles pem;
    PartialTlsRecordServer server(pem.cert, pem.key);
    server.start();

    ClientConfig config;
    config.verify_tls = false;
    config.request_timeout = std::chrono::seconds(1);
    ClientRequest request;
    request.url("https://127.0.0.1:" + std::to_string(server.port()) + "/partial");

    const auto start = std::chrono::steady_clock::now();
    auto result = Client::get_instance().send_blocking(request, config);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    server.stop();

    std::cout << "partial TLS record returned after " << elapsed << " ms\n";
    if (result.has_value()) {
        std::cerr << "FAIL: incomplete response was accepted\n";
        return 1;
    }
    if (elapsed >= 2500) {
        std::cerr << "FAIL: blocking SSL_read escaped the one-second request timeout\n";
        return 1;
    }
    std::cout << "PASS: partial TLS record cannot block past request_timeout\n";
#endif
    return 0;
}
