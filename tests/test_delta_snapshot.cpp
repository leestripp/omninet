// OmniNet - DeltaSnapshot compression tests
#include "game/DeltaSnapshot.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace OmniNet::Game;

static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static std::vector<uint8_t> toBytes(const std::string& s) { return {s.begin(), s.end()}; }

static void test_full_then_delta() {
    DeltaEncoder enc;
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    // First encode: no baseline -> full
    std::string s1 = "player{x:10,y:20,hp:100}";
    auto e1 = enc.encode(7, 1, toBytes(s1));
    ASSERT_TRUE(dec.decode(7, e1, out, snapId));
    ASSERT_EQ(snapId, 1u);
    ASSERT_TRUE(out == toBytes(s1));

    // Ack: encoder moves baseline forward
    enc.setBaseline(7, 1, toBytes(s1));

    // Small change -> delta, much smaller than full
    std::string s2 = "player{x:11,y:20,hp:100}";
    auto e2 = enc.encode(7, 2, toBytes(s2));
    ASSERT_TRUE(e2.size() < e1.size() / 2);
    ASSERT_TRUE(dec.decode(7, e2, out, snapId));
    ASSERT_EQ(snapId, 2u);
    ASSERT_TRUE(out == toBytes(s2));
}

static void test_identical_snapshot_tiny_delta() {
    DeltaEncoder enc;
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    std::string big(2000, 'q');
    auto e1 = enc.encode(1, 1, toBytes(big));
    ASSERT_TRUE(dec.decode(1, e1, out, snapId));
    enc.setBaseline(1, 1, toBytes(big));

    auto e2 = enc.encode(1, 2, toBytes(big));  // unchanged
    ASSERT_TRUE(e2.size() < 32);               // ~3 varints + type
    ASSERT_TRUE(dec.decode(1, e2, out, snapId));
    ASSERT_TRUE(out == toBytes(big));
}

static void test_completely_different_falls_back_to_full() {
    DeltaEncoder enc;
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    std::string a(500, 'A');
    std::string b(500, 'B');
    auto e1 = enc.encode(1, 1, toBytes(a));
    ASSERT_TRUE(dec.decode(1, e1, out, snapId));
    enc.setBaseline(1, 1, toBytes(a));

    auto e2 = enc.encode(1, 2, toBytes(b));
    // Fallback: full snapshot, decoder still reconstructs correctly
    ASSERT_TRUE(dec.decode(1, e2, out, snapId));
    ASSERT_TRUE(out == toBytes(b));
}

static void test_size_change_growth() {
    DeltaEncoder enc;
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    std::string small = "abc";
    auto e1 = enc.encode(9, 1, toBytes(small));
    ASSERT_TRUE(dec.decode(9, e1, out, snapId));
    enc.setBaseline(9, 1, toBytes(small));

    std::string grown = "abc" + std::string(100, 'x');
    auto e2 = enc.encode(9, 2, toBytes(grown));
    ASSERT_TRUE(dec.decode(9, e2, out, snapId));
    ASSERT_TRUE(out == toBytes(grown));
}

static void test_independent_objects() {
    DeltaEncoder enc;
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    auto e1 = enc.encode(1, 1, toBytes("obj-one"));
    auto e2 = enc.encode(2, 1, toBytes("obj-two"));
    ASSERT_TRUE(dec.decode(1, e1, out, snapId));
    ASSERT_TRUE(out == toBytes("obj-one"));
    ASSERT_TRUE(dec.decode(2, e2, out, snapId));
    ASSERT_TRUE(out == toBytes("obj-two"));

    enc.setBaseline(1, 1, toBytes("obj-one"));
    auto e3 = enc.encode(1, 2, toBytes("obj-ONE"));
    ASSERT_TRUE(dec.decode(1, e3, out, snapId));
    ASSERT_TRUE(out == toBytes("obj-ONE"));
    // Object 2 untouched
    ASSERT_TRUE(dec.baseline(2) != nullptr);
    ASSERT_TRUE(*dec.baseline(2) == toBytes("obj-two"));
}

static void test_malformed_rejected() {
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    std::vector<uint8_t> junk = {0xFF, 0xFF, 0xFF};
    ASSERT_TRUE(!dec.decode(1, junk, out, snapId));

    // Delta with no baseline
    DeltaEncoder enc;
    // Hand-craft a delta record: type=1, snapId=1, baseId=0, size=10, ops...
    std::vector<uint8_t> fakeDelta = {1, 1, 0, 10, 5, 5, 'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(!dec.decode(42, fakeDelta, out, snapId));
}

static void test_large_world_snapshot_savings() {
    // Simulate a 4 KB world snapshot with a small region changing per tick
    DeltaEncoder enc;
    DeltaDecoder dec;
    std::vector<uint8_t> out;
    uint32_t snapId = 0;

    std::string world(4096, '\0');
    for (size_t i = 0; i < world.size(); ++i)
        world[i] = static_cast<char>((i * 7 + 3) & 0xFF);

    auto full = enc.encode(0, 1, toBytes(world));
    ASSERT_TRUE(dec.decode(0, full, out, snapId));
    enc.setBaseline(0, 1, toBytes(world));

    // One entity moves: 16 bytes change
    std::string world2 = world;
    for (size_t i = 512; i < 528; ++i) world2[i] = static_cast<char>(i & 0xFF);

    auto delta = enc.encode(0, 2, toBytes(world2));
    ASSERT_TRUE(delta.size() < full.size() / 20);  // >95% savings
    ASSERT_TRUE(dec.decode(0, delta, out, snapId));
    ASSERT_TRUE(out == toBytes(world2));
}

int main() {
    test_full_then_delta();
    test_identical_snapshot_tiny_delta();
    test_completely_different_falls_back_to_full();
    test_size_change_growth();
    test_independent_objects();
    test_malformed_rejected();
    test_large_world_snapshot_savings();

    if (g_failures == 0) {
        std::printf("All DeltaSnapshot tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
