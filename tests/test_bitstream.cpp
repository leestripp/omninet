// OmniNet - BitStream tests (game networking)
#include "game/BitStream.h"

#include <cmath>
#include <cstdio>

using namespace OmniNet::Game;

static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static void test_bits_roundtrip() {
    BitWriter w;
    w.writeBits(0xABC, 12);
    w.writeBool(true);
    w.writeBool(false);
    w.writeBits(7, 3);
    BitReader r(w.bytes());
    ASSERT_EQ(r.readBits(12), 0xABCu);
    ASSERT_TRUE(r.readBool());
    ASSERT_TRUE(!r.readBool());
    ASSERT_EQ(r.readBits(3), 7u);
    ASSERT_TRUE(!r.hasError());
    ASSERT_EQ(w.bitsWritten(), 17u);
}

static void test_integers() {
    BitWriter w;
    w.writeU8(0x12);
    w.writeU16(0x3456);
    w.writeU32(0x789ABCDE);
    w.writeU64(0x0123456789ABCDEFull);
    BitReader r(w.bytes());
    ASSERT_EQ(r.readU8(), 0x12);
    ASSERT_EQ(r.readU16(), 0x3456);
    ASSERT_EQ(r.readU32(), 0x789ABCDEu);
    ASSERT_EQ(r.readU64(), 0x0123456789ABCDEFull);
    ASSERT_TRUE(!r.hasError());
}

static void test_floats() {
    BitWriter w;
    w.writeFloat(3.14159f);
    w.writeDouble(-2.718281828459045);
    BitReader r(w.bytes());
    ASSERT_TRUE(std::fabs(r.readFloat() - 3.14159f) < 1e-6f);
    ASSERT_TRUE(std::fabs(r.readDouble() + 2.718281828459045) < 1e-12);
    ASSERT_TRUE(!r.hasError());
}

static void test_quantized_float() {
    BitWriter w;
    w.writeQuantizedFloat(50.0f, -100.0f, 100.0f, 12);
    w.writeQuantizedFloat(-100.0f, -100.0f, 100.0f, 12);
    w.writeQuantizedFloat(999.0f, -100.0f, 100.0f, 12);  // clamps to max
    BitReader r(w.bytes());
    float mid = r.readQuantizedFloat(-100.0f, 100.0f, 12);
    ASSERT_TRUE(std::fabs(mid - 50.0f) < 0.1f);
    ASSERT_TRUE(std::fabs(r.readQuantizedFloat(-100.0f, 100.0f, 12) + 100.0f) < 0.1f);
    ASSERT_TRUE(std::fabs(r.readQuantizedFloat(-100.0f, 100.0f, 12) - 100.0f) < 0.1f);
    ASSERT_TRUE(!r.hasError());
}

static void test_varint() {
    uint64_t values[] = {0, 1, 127, 128, 300, 65535, 1ull << 32, ~0ull};
    BitWriter w;
    for (uint64_t v : values) w.writeVarInt(v);
    BitReader r(w.bytes());
    for (uint64_t v : values) ASSERT_EQ(r.readVarInt(), v);
    ASSERT_TRUE(!r.hasError());

    // varint size sanity: small values take 1 byte
    BitWriter small;
    small.writeVarInt(42);
    ASSERT_EQ(small.bytesWritten(), 1u);
}

static void test_string_and_bytes() {
    BitWriter w;
    w.writeString("hello world");
    uint8_t blob[] = {1, 2, 3, 250, 0, 99};
    w.writeBytes(blob);
    BitReader r(w.bytes());
    ASSERT_TRUE(r.readString() == "hello world");
    uint8_t out[6];
    ASSERT_TRUE(r.readBytes(out));
    for (int i = 0; i < 6; ++i) ASSERT_EQ(out[i], blob[i]);
    ASSERT_TRUE(!r.hasError());
}

static void test_mixed_alignment() {
    // Unaligned writes followed by aligned data
    BitWriter w;
    w.writeBits(5, 3);
    w.writeString("abc");
    w.writeBool(true);
    w.alignToByte();
    w.writeU32(0xDEADBEEF);
    BitReader r(w.bytes());
    ASSERT_EQ(r.readBits(3), 5u);
    ASSERT_TRUE(r.readString() == "abc");
    ASSERT_TRUE(r.readBool());
    r.alignToByte();
    ASSERT_EQ(r.readU32(), 0xDEADBEEFu);
    ASSERT_TRUE(!r.hasError());
}

static void test_overread_sets_error() {
    BitWriter w;
    w.writeU8(42);
    BitReader r(w.bytes());
    ASSERT_EQ(r.readU8(), 42);
    ASSERT_TRUE(!r.hasError());
    r.readU32();  // too few bits left
    ASSERT_TRUE(r.hasError());
    ASSERT_EQ(r.readU8(), 0);  // stuck at 0 after error
}

static void test_empty_string() {
    BitWriter w;
    w.writeString("");
    BitReader r(w.bytes());
    ASSERT_TRUE(r.readString().empty());
    ASSERT_TRUE(!r.hasError());
}

static void test_clear_and_reuse() {
    BitWriter w(64);
    w.writeU32(123);
    w.clear();
    ASSERT_EQ(w.bitsWritten(), 0u);
    w.writeU8(7);
    BitReader r(w.bytes());
    ASSERT_EQ(r.readU8(), 7);
}

int main() {
    test_bits_roundtrip();
    test_integers();
    test_floats();
    test_quantized_float();
    test_varint();
    test_string_and_bytes();
    test_mixed_alignment();
    test_overread_sets_error();
    test_empty_string();
    test_clear_and_reuse();

    if (g_failures == 0) {
        std::printf("All BitStream tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
