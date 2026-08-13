// OmniNet - DtlsSocket tests: encrypted UDP echo over loopback
#include "crypto/Crypto.h"
#include "net/DtlsSocket.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

using namespace OmniNet;

static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

namespace {

std::string writeTemp(const std::vector<uint8_t>& pem, const char* name) {
    std::string path = std::string("/tmp/omninet_dtls_test_") + name;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(pem.data()), pem.size());
    return path;
}

} // namespace

static void run_echo_case(bool requireCookie) {
    auto [certPem, keyPem] = Crypto::generateSelfSignedCert("dtls-test", 1);
    ASSERT_TRUE(!certPem.empty() && !keyPem.empty());
    std::string certPath = writeTemp(certPem, "cert.pem");
    std::string keyPath = writeTemp(keyPem, "key.pem");

    DtlsSocket server;
    ASSERT_TRUE(server.bind("127.0.0.1", 0).has_value());
    uint16_t port = server.localPort();
    ASSERT_TRUE(port != 0);

    std::string echoBack;
    bool serverOk = false;
    std::thread serverThread([&] {
        if (!server.accept(certPath, keyPath, 8000, requireCookie).has_value()) return;
        serverOk = true;
        auto got = server.recv(4096, 5000);
        if (!got) return;
        echoBack.assign(reinterpret_cast<const char*>(got->data()), got->size());
        (void)server.send(*got);  // echo
    });

    DtlsSocket client;
    auto res = client.connect("127.0.0.1", port, 8000);
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(client.isEstablished());
    ASSERT_TRUE(!client.cipherName().empty());

    const char* msg = "secret position update";
    auto sent = client.send({reinterpret_cast<const uint8_t*>(msg), std::strlen(msg)});
    ASSERT_TRUE(sent.has_value());
    ASSERT_TRUE(*sent == std::strlen(msg));

    auto reply = client.recv(4096, 5000);
    ASSERT_TRUE(reply.has_value());
    ASSERT_TRUE(reply->size() == std::strlen(msg));
    ASSERT_TRUE(std::memcmp(reply->data(), msg, reply->size()) == 0);

    serverThread.join();
    ASSERT_TRUE(serverOk);
    ASSERT_TRUE(echoBack == msg);

    client.close();
    ASSERT_TRUE(!client.isEstablished());
}

static void test_handshake_and_echo() { run_echo_case(false); }
static void test_cookie_exchange_echo() { run_echo_case(true); }

static void test_connect_timeout_no_server() {
    DtlsSocket client;
    auto res = client.connect("127.0.0.1", 59998, 800);
    ASSERT_TRUE(!res.has_value());
    ASSERT_TRUE(!client.isEstablished());
}

int main() {
    test_handshake_and_echo();
    test_cookie_exchange_echo();
    test_connect_timeout_no_server();

    if (g_failures == 0) {
        std::printf("All DtlsSocket tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
