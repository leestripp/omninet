#include "crypto/Crypto.h"
#include "http/HttpServer.h"
#include "net/TLSSocket.h"

#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace {

using OmniNet::Crypto;
using OmniNet::Socket;
using OmniNet::TLSSocket;
using OmniNet::HTTP::Request;
using OmniNet::HTTP::Response;
using OmniNet::HTTP::Server;
using OmniNet::HTTP::StatusCode;

class TempDirectory {
public:
    TempDirectory()
        : path_(std::filesystem::temp_directory_path() /
                ("omninet-tls-server-" + std::to_string(::getpid()))) {
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() { std::filesystem::remove_all(path_); }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!output) throw std::runtime_error("Could not write test PEM file");
}

std::span<const uint8_t> bytes(std::string_view text) {
    return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

} // namespace

int main() {
    std::signal(SIGPIPE, SIG_IGN);

    TempDirectory temp;
    const auto certPath = temp.path() / "server.crt";
    const auto keyPath = temp.path() / "server.key";
    auto [cert, key] = Crypto::generateSelfSignedCert("127.0.0.1", 1);
    writeFile(certPath, cert);
    writeFile(keyPath, key);

    Server::Config config;
    config.address = "127.0.0.1";
    config.port = 0;
    config.workerCount = 1;
    config.tlsEnabled = true;
    config.tlsCertFile = certPath.string();
    config.tlsKeyFile = keyPath.string();

    Server server(config);
    server.router().get("/healthz", [](const Request&, Response& response) {
        response.setStatus(StatusCode::OK);
        response.setBody("ok");
    });

    std::promise<uint16_t> readyPromise;
    auto ready = readyPromise.get_future();
    server.setOnReady([&](uint16_t port) { readyPromise.set_value(port); });
    server.startAsync();
    if (ready.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        std::cerr << "FAIL: TLS server did not start\n";
        return 1;
    }
    const auto port = ready.get();

    // Plain HTTP on an HTTPS port makes SSL_accept() fail. This is a client
    // protocol error and must never escape the server's accept loop.
    {
        Socket rejected;
        rejected.connect("127.0.0.1", port);
        rejected.sendAll(bytes("GET /healthz HTTP/1.0\r\n\r\n"));
        rejected.close();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    if (!server.isRunning()) {
        std::cerr << "FAIL: rejected TLS handshake stopped the HTTP server\n";
        return 1;
    }

    TLSSocket client;
    client.setVerifyMode(OmniNet::TLSVerify::NONE);
    client.connect("127.0.0.1", port);
    client.sendAll(bytes("GET /healthz HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"));

    std::string response;
    std::array<uint8_t, 4096> buffer{};
    for (int attempt = 0; attempt < 20 && response.find("ok") == std::string::npos; ++attempt) {
        const auto count = client.recv(buffer);
        if (count == 0) break;
        response.append(reinterpret_cast<const char*>(buffer.data()), count);
    }
    client.close();
    server.stop();

    if (response.find("200 OK") == std::string::npos ||
        response.find("ok") == std::string::npos) {
        std::cerr << "FAIL: valid TLS request after rejected handshake failed: " << response << '\n';
        return 1;
    }

    std::cout << "PASS: rejected TLS handshake is contained; later HTTPS request succeeds\n";
    return 0;
}
