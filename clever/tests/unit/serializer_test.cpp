#include <clever/ipc/serializer.h>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

using clever::ipc::Serializer;
using clever::ipc::Deserializer;

// ------------------------------------------------------------------
// 1. Round-trip u8, u16, u32, u64
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripU8) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(127);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 127);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU16) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(1000);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0);
    EXPECT_EQ(d.read_u16(), 1000);
    EXPECT_EQ(d.read_u16(), 65535);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU32) {
    Serializer s;
    s.write_u32(0);
    s.write_u32(123456789);
    s.write_u32(0xFFFFFFFF);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 123456789u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU64) {
    Serializer s;
    s.write_u64(0);
    s.write_u64(0xDEADBEEFCAFEBABEULL);
    s.write_u64(std::numeric_limits<uint64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_EQ(d.read_u64(), 0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 2. Round-trip i32, i64
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripI32) {
    Serializer s;
    s.write_i32(0);
    s.write_i32(-1);
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(std::numeric_limits<int32_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripI64) {
    Serializer s;
    s.write_i64(0);
    s.write_i64(-1);
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(std::numeric_limits<int64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 0);
    EXPECT_EQ(d.read_i64(), -1);
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 3. Round-trip f64 (including NaN, infinity)
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripF64Normal) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(3.14159265358979);
    s.write_f64(-1e300);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
    EXPECT_DOUBLE_EQ(d.read_f64(), -1e300);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripF64SpecialValues) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(d.read_f64()));
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 4. Round-trip bool
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripBool) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 5. Round-trip string (empty, short, long)
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripStringEmpty) {
    Serializer s;
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripStringShort) {
    Serializer s;
    s.write_string("hello");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripStringLong) {
    std::string long_str(10000, 'x');
    Serializer s;
    s.write_string(long_str);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripStringUTF8) {
    Serializer s;
    s.write_string("Hello \xC3\xA9\xC3\xA0\xC3\xBC \xE2\x9C\x93");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello \xC3\xA9\xC3\xA0\xC3\xBC \xE2\x9C\x93");
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 6. Round-trip bytes (empty, with data)
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripBytesEmpty) {
    Serializer s;
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripBytesWithData) {
    std::vector<uint8_t> bytes = {0x00, 0x01, 0xFF, 0xDE, 0xAD};
    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, bytes);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 7. Multiple values in sequence
// ------------------------------------------------------------------

TEST(SerializerTest, MultipleValuesInSequence) {
    Serializer s;
    s.write_u8(42);
    s.write_u32(12345);
    s.write_string("test");
    s.write_bool(true);
    s.write_f64(2.718281828);
    s.write_i64(-99999);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42);
    EXPECT_EQ(d.read_u32(), 12345u);
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_TRUE(d.read_bool());
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828);
    EXPECT_EQ(d.read_i64(), -99999);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// 8. Deserializer throws on underflow (reading past end)
// ------------------------------------------------------------------

TEST(SerializerTest, DeserializerThrowsOnUnderflowU8) {
    Deserializer d(nullptr, 0);
    EXPECT_THROW(d.read_u8(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowU16) {
    Serializer s;
    s.write_u8(1);
    Deserializer d(s.data());
    EXPECT_THROW(d.read_u16(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowU32) {
    Serializer s;
    s.write_u8(1);
    Deserializer d(s.data());
    EXPECT_THROW(d.read_u32(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowU64) {
    Serializer s;
    s.write_u32(1);
    Deserializer d(s.data());
    EXPECT_THROW(d.read_u64(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowString) {
    // Write a string length that exceeds available data
    Serializer s;
    s.write_u32(1000); // claims 1000 bytes but buffer ends here
    Deserializer d(s.data());
    EXPECT_THROW(d.read_string(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsAfterConsuming) {
    Serializer s;
    s.write_u32(42);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_THROW(d.read_u8(), std::runtime_error);
}

// ------------------------------------------------------------------
// take_data moves the buffer
// ------------------------------------------------------------------

TEST(SerializerTest, TakeDataMovesBuffer) {
    Serializer s;
    s.write_u32(42);
    auto data = s.take_data();
    EXPECT_EQ(data.size(), 4u);
    // After take_data, the serializer's buffer should be empty (moved from)
    EXPECT_TRUE(s.data().empty());
}

// ------------------------------------------------------------------
// Remaining / has_remaining
// ------------------------------------------------------------------

TEST(SerializerTest, RemainingAndHasRemaining) {
    Serializer s;
    s.write_u32(1);
    s.write_u32(2);

    Deserializer d(s.data());
    EXPECT_EQ(d.remaining(), 8u);
    EXPECT_TRUE(d.has_remaining());

    d.read_u32();
    EXPECT_EQ(d.remaining(), 4u);
    EXPECT_TRUE(d.has_remaining());

    d.read_u32();
    EXPECT_EQ(d.remaining(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// Cycle 430 — f64 boundary values, negative zero, underflow gaps,
//             and embedded-NUL string round-trip
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripF64BoundaryValues) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::denorm_min());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripF64NegativeZero) {
    Serializer s;
    s.write_f64(-0.0);

    Deserializer d(s.data());
    double result = d.read_f64();
    // -0.0 and 0.0 compare equal per IEEE 754; verify sign bit preserved
    EXPECT_EQ(result, -0.0);
    EXPECT_TRUE(std::signbit(result));
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowI32) {
    Serializer s;
    s.write_u16(1);  // only 2 bytes; read_i32 needs 4
    Deserializer d(s.data());
    EXPECT_THROW(d.read_i32(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowI64) {
    Serializer s;
    s.write_u32(1);  // only 4 bytes; read_i64 needs 8
    Deserializer d(s.data());
    EXPECT_THROW(d.read_i64(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowBool) {
    Deserializer d(nullptr, 0);
    EXPECT_THROW(d.read_bool(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowF64) {
    Serializer s;
    s.write_u32(1);  // only 4 bytes; read_f64 needs 8
    Deserializer d(s.data());
    EXPECT_THROW(d.read_f64(), std::runtime_error);
}

TEST(SerializerTest, DeserializerThrowsOnUnderflowBytes) {
    Serializer s;
    s.write_u32(1000);  // claims 1000-byte payload but buffer ends here
    Deserializer d(s.data());
    EXPECT_THROW(d.read_bytes(), std::runtime_error);
}

TEST(SerializerTest, RoundTripStringWithEmbeddedNul) {
    // String containing a NUL byte must round-trip as binary-safe data
    std::string nul_str("hello\0world", 11);
    Serializer s;
    s.write_string(nul_str);

    Deserializer d(s.data());
    std::string result = d.read_string();
    ASSERT_EQ(result.size(), 11u);
    EXPECT_EQ(result, nul_str);
    EXPECT_FALSE(d.has_remaining());
}

// ---------------------------------------------------------------------------
// Cycle 486 — additional Serializer / Deserializer regression tests
// ---------------------------------------------------------------------------

// i64 boundary values: INT64_MIN, -1, 0, INT64_MAX
TEST(SerializerTest, RoundTripI64BoundaryValues) {
    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(-1LL);
    s.write_i64(0LL);
    s.write_i64(std::numeric_limits<int64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), -1LL);
    EXPECT_EQ(d.read_i64(), 0LL);
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

// u8 edge values: 0, 1, 127, 128, 255
TEST(SerializerTest, RoundTripU8EdgeValues) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 1u);
    EXPECT_EQ(d.read_u8(), 127u);
    EXPECT_EQ(d.read_u8(), 128u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

// large bytes buffer: 1024 entries, verify content
TEST(SerializerTest, RoundTripLargeBytes) {
    std::vector<uint8_t> big(1024);
    std::iota(big.begin(), big.end(), static_cast<uint8_t>(0));

    Serializer s;
    s.write_bytes(big.data(), big.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, big);
    EXPECT_FALSE(d.has_remaining());
}

// data() size grows correctly with each write
TEST(SerializerTest, SerializerDataSizeGrowsWithWrites) {
    Serializer s;
    EXPECT_EQ(s.data().size(), 0u);

    s.write_u8(1);       // +1
    EXPECT_EQ(s.data().size(), 1u);

    s.write_u16(2);      // +2
    EXPECT_EQ(s.data().size(), 3u);

    s.write_u32(3);      // +4
    EXPECT_EQ(s.data().size(), 7u);

    s.write_u64(4);      // +8
    EXPECT_EQ(s.data().size(), 15u);
}

// very long string (1000 chars) round-trips correctly
TEST(SerializerTest, RoundTripStringVeryLong) {
    std::string long_str(1000, 'x');
    for (size_t i = 0; i < long_str.size(); ++i) {
        long_str[i] = static_cast<char>('a' + (i % 26));
    }

    Serializer s;
    s.write_string(long_str);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

// multiple bools: F,T,F,T,T pattern preserved
TEST(SerializerTest, RoundTripMultipleBoolsPattern) {
    std::vector<bool> pattern = {false, true, false, true, true};

    Serializer s;
    for (bool b : pattern) s.write_bool(b);

    Deserializer d(s.data());
    for (bool expected : pattern) {
        EXPECT_EQ(d.read_bool(), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

// u16 underflow: only 1 byte present (verifies check when 1 byte remains)
TEST(SerializerTest, DeserializerThrowsOnUnderflowU16SingleByte) {
    Serializer s;
    s.write_u8(0xFF);  // only 1 byte; read_u16 needs 2
    Deserializer d(s.data());
    EXPECT_THROW(d.read_u16(), std::runtime_error);
}

// Deserializer constructed from raw pointer and size
TEST(SerializerTest, DeserializerFromRawPointerAndSize) {
    Serializer s;
    s.write_u32(0xDEADBEEFu);
    s.write_u32(0xCAFEBABEu);
    auto buf = s.take_data();

    Deserializer d(buf.data(), buf.size());
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_u32(), 0xCAFEBABEu);
    EXPECT_FALSE(d.has_remaining());
}

// ---------------------------------------------------------------------------
// Cycle 496 — Serializer additional regression tests
// ---------------------------------------------------------------------------

// u16 boundary values: 0 and UINT16_MAX
TEST(SerializerTest, RoundTripU16BoundaryValues) {
    Serializer s;
    s.write_u16(0u);
    s.write_u16(std::numeric_limits<uint16_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), std::numeric_limits<uint16_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

// u64 max value round-trip
TEST(SerializerTest, RoundTripU64MaxValue) {
    Serializer s;
    s.write_u64(std::numeric_limits<uint64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

// A regular negative f64 value (not -0.0 or -inf)
TEST(SerializerTest, RoundTripNegativeF64Regular) {
    Serializer s;
    s.write_f64(-3.141592653589793);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -3.141592653589793);
    EXPECT_FALSE(d.has_remaining());
}

// Deserializer constructed from empty vector throws on first read
TEST(SerializerTest, DeserializerEmptyVectorThrowsOnRead) {
    std::vector<uint8_t> empty;
    Deserializer d(empty);
    EXPECT_THROW(d.read_u8(), std::runtime_error);
}

// Serializer data() first byte matches the u8 that was written
TEST(SerializerTest, SerializerDataFirstByteMatchesU8) {
    Serializer s;
    s.write_u8(0xABu);
    ASSERT_GE(s.data().size(), 1u);
    EXPECT_EQ(s.data()[0], static_cast<uint8_t>(0xABu));
}

// All zero numeric values round-trip correctly
TEST(SerializerTest, RoundTripAllZeroNumericValues) {
    Serializer s;
    s.write_u8(0);
    s.write_u16(0);
    s.write_u32(0);
    s.write_u64(0);
    s.write_i32(0);
    s.write_i64(0);
    s.write_f64(0.0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i64(), 0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_FALSE(d.has_remaining());
}

// Serializer data size equals sum of individual type sizes
TEST(SerializerTest, SerializerSizeMatchesTypeSizes) {
    Serializer s;
    s.write_u8(1);   // 1 byte
    s.write_u32(2);  // 4 bytes
    // Total = 5 bytes
    EXPECT_EQ(s.data().size(), 5u);
}

// String with special/escape characters round-trips intact
TEST(SerializerTest, RoundTripStringWithSpecialChars) {
    std::string special = "hello\nworld\t!\r\nend";
    Serializer s;
    s.write_string(special);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), special);
    EXPECT_FALSE(d.has_remaining());
}


// ============================================================================
// Cycle 504: Serializer additional regression tests
// ============================================================================

// fresh Serializer has empty buffer
TEST(SerializerTest, SerializerInitiallyEmpty) {
    Serializer s;
    EXPECT_TRUE(s.data().empty());
    EXPECT_EQ(s.data().size(), 0u);
}

// take_data() moves the buffer out, leaving serializer empty
TEST(SerializerTest, TakeDataEmptiesSerializer) {
    Serializer s;
    s.write_u32(42u);
    EXPECT_FALSE(s.data().empty());

    auto taken = s.take_data();
    EXPECT_FALSE(taken.empty());
    EXPECT_TRUE(s.data().empty()); // serializer is now empty
}

// empty string round-trips correctly
TEST(SerializerTest, RoundTripEmptyString) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

// remaining() decrements by the correct number of bytes on each read
TEST(SerializerTest, RemainingDecrementsOnRead) {
    Serializer s;
    s.write_u8(1);
    s.write_u8(2);
    s.write_u8(3);

    Deserializer d(s.data());
    EXPECT_EQ(d.remaining(), 3u);
    d.read_u8();
    EXPECT_EQ(d.remaining(), 2u);
    d.read_u8();
    EXPECT_EQ(d.remaining(), 1u);
    d.read_u8();
    EXPECT_EQ(d.remaining(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// interleaved types round-trip in correct order
TEST(SerializerTest, RoundTripInterleavedTypes) {
    Serializer s;
    s.write_u8(99);
    s.write_string("hello");
    s.write_i64(-12345678901LL);
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 99u);
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_EQ(d.read_i64(), -12345678901LL);
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// positive and negative infinity round-trip
TEST(SerializerTest, RoundTripF64Infinity) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_FALSE(d.has_remaining());
}

// NaN round-trips (result is still NaN)
TEST(SerializerTest, RoundTripF64NaN) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::quiet_NaN());

    Deserializer d(s.data());
    double result = d.read_f64();
    EXPECT_TRUE(std::isnan(result));
    EXPECT_FALSE(d.has_remaining());
}

// two consecutive write_bytes calls — total buffer size is sum of both
TEST(SerializerTest, TwoWriteBytesCallsRoundTrip) {
    // write_bytes includes a length prefix; verify both round-trip via read_bytes
    Serializer s;
    const uint8_t a[] = {0x01, 0x02, 0x03};
    const uint8_t b[] = {0x04, 0x05};
    s.write_bytes(a, sizeof(a));
    s.write_bytes(b, sizeof(b));

    Deserializer d(s.data());
    auto r1 = d.read_bytes();
    ASSERT_EQ(r1.size(), 3u);
    EXPECT_EQ(r1[0], 0x01);
    EXPECT_EQ(r1[2], 0x03);

    auto r2 = d.read_bytes();
    ASSERT_EQ(r2.size(), 2u);
    EXPECT_EQ(r2[0], 0x04);
    EXPECT_EQ(r2[1], 0x05);

    EXPECT_FALSE(d.has_remaining());
}
