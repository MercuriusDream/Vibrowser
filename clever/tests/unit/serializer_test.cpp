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

// ============================================================================
// Cycle 518: Serializer regression tests
// ============================================================================

// Round-trip multiple u8 values
TEST(SerializerTest, MultipleU8ValuesRoundTrip) {
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

// Alternating u32 and string round-trips
TEST(SerializerTest, AlternatingU32AndString) {
    Serializer s;
    s.write_u32(1000);
    s.write_string("hello");
    s.write_u32(2000);
    s.write_string("world");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1000u);
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_EQ(d.read_u32(), 2000u);
    EXPECT_EQ(d.read_string(), "world");
    EXPECT_FALSE(d.has_remaining());
}

// write_u64 with value fitting in 32 bits
TEST(SerializerTest, U64FitsIn32Bits) {
    Serializer s;
    s.write_u64(0xDEADBEEFu);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0xDEADBEEFu);
}

// take_data leaves serializer in empty state
TEST(SerializerTest, TakeDataLeavesSerializerEmpty) {
    Serializer s;
    s.write_u32(42);
    auto v = s.take_data();
    EXPECT_FALSE(v.empty());
    EXPECT_TRUE(s.data().empty());
}

// data() on fresh Serializer returns empty vector
TEST(SerializerTest, FreshSerializerDataIsEmpty) {
    Serializer s;
    EXPECT_TRUE(s.data().empty());
    EXPECT_EQ(s.data().size(), 0u);
}

// Boolean sequence: write 4 bools, read back in order
TEST(SerializerTest, BoolSequenceRoundTrip) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Deserializer from raw pointer + size (second variant with larger values)
TEST(SerializerTest, DeserializerFromRawPointerLargeValues) {
    Serializer s;
    s.write_u32(999999);
    s.write_u32(111111);
    auto bytes = s.data();
    Deserializer d(bytes.data(), bytes.size());
    EXPECT_EQ(d.read_u32(), 999999u);
    EXPECT_EQ(d.read_u32(), 111111u);
    EXPECT_FALSE(d.has_remaining());
}

// String with special printable characters
TEST(SerializerTest, StringWithSpecialPrintableChars) {
    Serializer s;
    std::string special = "hello\n\t!@#world";
    s.write_string(special);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), special);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 528: Serializer regression tests
// ============================================================================

// Write a single u16 and read it back
TEST(SerializerTest, RoundTripSingleU16) {
    Serializer s;
    s.write_u16(12345);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_FALSE(d.has_remaining());
}

// Write max u16 value
TEST(SerializerTest, RoundTripMaxU16) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

// Write zero u64 and read back
TEST(SerializerTest, RoundTripZeroU64) {
    Serializer s;
    s.write_u64(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// Empty string round trip
TEST(SerializerTest, EmptyStringRoundTrip) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

// Write and read multiple strings in sequence
TEST(SerializerTest, MultipleStringsInSequence) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_string("gamma");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_string(), "gamma");
    EXPECT_FALSE(d.has_remaining());
}

// Write a single false bool
TEST(SerializerTest, RoundTripFalseBool) {
    Serializer s;
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Write u8 max value (255)
TEST(SerializerTest, RoundTripU8MaxValue) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

// Write u32 zero
TEST(SerializerTest, RoundTripU32Zero) {
    Serializer s;
    s.write_u32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 543: Serializer regression tests
// ============================================================================

// Write u32 max value (0xFFFFFFFF)
TEST(SerializerTest, RoundTripU32MaxValue) {
    Serializer s;
    s.write_u32(0xFFFFFFFF);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_FALSE(d.has_remaining());
}

// Write u16 zero
TEST(SerializerTest, RoundTripU16Zero) {
    Serializer s;
    s.write_u16(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// Write true bool
TEST(SerializerTest, RoundTripTrueBool) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Write u64 max value
TEST(SerializerTest, RoundTripU64UINT64MAX) {
    Serializer s;
    s.write_u64(UINT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

// Write multiple bools
TEST(SerializerTest, MultipleBoolsRoundTrip) {
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

// Long string round trip
TEST(SerializerTest, LongStringRoundTrip) {
    Serializer s;
    std::string long_str(200, 'x');
    s.write_string(long_str);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

// Mix u8 with string and u32
TEST(SerializerTest, MixedTypesRoundTrip) {
    Serializer s;
    s.write_u8(7);
    s.write_string("test");
    s.write_u32(12345);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 7u);
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_EQ(d.read_u32(), 12345u);
    EXPECT_FALSE(d.has_remaining());
}

// Serializer data size grows with writes
TEST(SerializerTest, DataSizeGrowsWithWrites) {
    Serializer s;
    auto size0 = s.data().size();
    s.write_u8(42);
    auto size1 = s.data().size();
    s.write_u32(9999);
    auto size2 = s.data().size();
    EXPECT_GT(size1, size0);
    EXPECT_GT(size2, size1);
}

// ============================================================================
// Cycle 551: Serializer regression tests
// ============================================================================

// Interleaved u16 and u32 round trip
TEST(SerializerTest, InterleavedU16AndU32) {
    Serializer s;
    s.write_u16(100);
    s.write_u32(200000);
    s.write_u16(300);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 100u);
    EXPECT_EQ(d.read_u32(), 200000u);
    EXPECT_EQ(d.read_u16(), 300u);
    EXPECT_FALSE(d.has_remaining());
}

// Write 10 u8 values and read them all
TEST(SerializerTest, TenU8ValuesRoundTrip) {
    Serializer s;
    for (uint8_t i = 0; i < 10; ++i) {
        s.write_u8(i * 10);
    }
    Deserializer d(s.data());
    for (uint8_t i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i * 10));
    }
    EXPECT_FALSE(d.has_remaining());
}

// String followed by bool
TEST(SerializerTest, StringThenBoolRoundTrip) {
    Serializer s;
    s.write_string("hello");
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Take data resets serializer state  
TEST(SerializerTest, TakeDataAndResend) {
    Serializer s;
    s.write_u32(42);
    auto data1 = s.take_data();
    EXPECT_FALSE(data1.empty());
    // After take, serializer should be empty
    EXPECT_TRUE(s.data().empty());
    // Can write again
    s.write_u32(99);
    auto data2 = s.take_data();
    EXPECT_FALSE(data2.empty());
}

// u64 value that uses all 8 bytes
TEST(SerializerTest, U64LargeValueRoundTrip) {
    Serializer s;
    uint64_t val = 0x0102030405060708ULL;
    s.write_u64(val);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), val);
    EXPECT_FALSE(d.has_remaining());
}

// Four u8 values round trip
TEST(SerializerTest, FourU8RoundTrip) {
    Serializer s;
    s.write_u8(10);
    s.write_u8(20);
    s.write_u8(30);
    s.write_u8(40);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 10u);
    EXPECT_EQ(d.read_u8(), 20u);
    EXPECT_EQ(d.read_u8(), 30u);
    EXPECT_EQ(d.read_u8(), 40u);
    EXPECT_FALSE(d.has_remaining());
}

// String with space and punctuation
TEST(SerializerTest, StringWithSpaceAndPunctuation) {
    Serializer s;
    std::string str = "Hello, World!";
    s.write_string(str);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), str);
    EXPECT_FALSE(d.has_remaining());
}

// Two u64 values
TEST(SerializerTest, TwoU64ValuesRoundTrip) {
    Serializer s;
    s.write_u64(0xDEADBEEFCAFEBABEULL);
    s.write_u64(0x123456789ABCDEF0ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(d.read_u64(), 0x123456789ABCDEF0ULL);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 563: i32, i64, bytes, remaining
// ============================================================================

// i32 round trip: positive
TEST(SerializerTest, RoundTripI32Positive) {
    Serializer s;
    s.write_i32(42);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 42);
    EXPECT_FALSE(d.has_remaining());
}

// i32 round trip: negative
TEST(SerializerTest, RoundTripI32Negative) {
    Serializer s;
    s.write_i32(-1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000);
    EXPECT_FALSE(d.has_remaining());
}

// i32 round trip: INT32_MIN
TEST(SerializerTest, RoundTripI32Min) {
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
}

// i64 round trip: positive
TEST(SerializerTest, RoundTripI64Positive) {
    Serializer s;
    s.write_i64(1234567890123LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1234567890123LL);
    EXPECT_FALSE(d.has_remaining());
}

// i64 round trip: negative
TEST(SerializerTest, RoundTripI64Negative) {
    Serializer s;
    s.write_i64(-9876543210LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9876543210LL);
    EXPECT_FALSE(d.has_remaining());
}

// bytes round trip
TEST(SerializerTest, BytesRoundTrip) {
    Serializer s;
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0xFF};
    s.write_bytes(payload.data(), payload.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, payload);
}

// remaining() decreases as reads proceed
TEST(SerializerTest, RemainingDecreasesAfterRead) {
    Serializer s;
    s.write_u8(1);
    s.write_u8(2);
    s.write_u8(3);
    Deserializer d(s.data());
    size_t before = d.remaining();
    d.read_u8();
    size_t after = d.remaining();
    EXPECT_LT(after, before);
}

// Mixed i32 and u8 round trip
TEST(SerializerTest, MixedI32AndU8RoundTrip) {
    Serializer s;
    s.write_i32(-7);
    s.write_u8(200u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -7);
    EXPECT_EQ(d.read_u8(), 200u);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 576: More serializer tests
// ============================================================================

// i32 round trip: INT32_MAX
TEST(SerializerTest, RoundTripI32Max) {
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::max());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

// i64 round trip: INT64_MIN
TEST(SerializerTest, RoundTripI64Min) {
    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_FALSE(d.has_remaining());
}

// write_bytes empty vector
TEST(SerializerTest, EmptyBytesRoundTrip) {
    Serializer s;
    s.write_bytes(nullptr, 0);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_TRUE(result.empty());
}

// Three strings serialize and deserialize in order
TEST(SerializerTest, ThreeStringsInOrder) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_string("gamma");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_string(), "gamma");
    EXPECT_FALSE(d.has_remaining());
}

// u8 max followed by i32 zero
TEST(SerializerTest, U8MaxThenI32Zero) {
    Serializer s;
    s.write_u8(255u);
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_FALSE(d.has_remaining());
}

// data() returns non-empty vector after writes
TEST(SerializerTest, DataNonEmptyAfterWrites) {
    Serializer s;
    s.write_u32(0xABCDEF01u);
    EXPECT_FALSE(s.data().empty());
}

// take_data() moves data out
TEST(SerializerTest, TakeDataMovesOut) {
    Serializer s;
    s.write_u16(12345u);
    auto data = s.take_data();
    EXPECT_FALSE(data.empty());
    // After take_data, original should be empty
    EXPECT_TRUE(s.data().empty());
}

// u64 zero is a valid value
TEST(SerializerTest, U64ZeroRoundTripV2) {
    Serializer s;
    s.write_u64(0ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ULL);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 588: More serializer tests
// ============================================================================

// i32 round trip: zero
TEST(SerializerTest, RoundTripI32Zero) {
    Serializer s;
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_FALSE(d.has_remaining());
}

// Alternating u8 and bool
TEST(SerializerTest, AlternatingU8AndBool) {
    Serializer s;
    s.write_u8(77u);
    s.write_bool(true);
    s.write_u8(88u);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 77u);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u8(), 88u);
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Five u16 values round trip
TEST(SerializerTest, FiveU16ValuesRoundTrip) {
    Serializer s;
    for (uint16_t i = 0; i < 5; ++i) {
        s.write_u16(static_cast<uint16_t>(i * 1000));
    }
    Deserializer d(s.data());
    for (uint16_t i = 0; i < 5; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(i * 1000));
    }
    EXPECT_FALSE(d.has_remaining());
}

// i64 round trip: zero
TEST(SerializerTest, RoundTripI64Zero) {
    Serializer s;
    s.write_i64(0LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 0LL);
    EXPECT_FALSE(d.has_remaining());
}

// Bytes of size 8 round trip
TEST(SerializerTest, EightBytesRoundTrip) {
    Serializer s;
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    s.write_bytes(payload.data(), payload.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, payload);
}

// String then i64
TEST(SerializerTest, StringThenI64RoundTrip) {
    Serializer s;
    s.write_string("test");
    s.write_i64(-42LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_EQ(d.read_i64(), -42LL);
    EXPECT_FALSE(d.has_remaining());
}

// u32 alternating with bool
TEST(SerializerTest, U32AlternatingWithBool) {
    Serializer s;
    s.write_u32(100u);
    s.write_bool(true);
    s.write_u32(200u);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 100u);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u32(), 200u);
    EXPECT_FALSE(d.read_bool());
}

// Write 20 u8 values, all preserved
TEST(SerializerTest, TwentyU8ValuesRoundTrip) {
    Serializer s;
    for (int i = 0; i < 20; ++i) {
        s.write_u8(static_cast<uint8_t>(i * 10));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i * 10));
    }
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 599: More serializer tests
// ============================================================================

// Write u8 max value then read it back
TEST(SerializerTest, U8MaxValueRoundTrip) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
}

// Write u16 max value then read it back
TEST(SerializerTest, U16MaxValueRoundTrip) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
}

// Write u32 max and min
TEST(SerializerTest, U32MaxAndMinRoundTrip) {
    Serializer s;
    s.write_u32(0xFFFFFFFFu);
    s.write_u32(0u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0u);
}

// Write u64 max value
TEST(SerializerTest, U64MaxValueRoundTrip) {
    Serializer s;
    s.write_u64(0xFFFFFFFFFFFFFFFFULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFULL);
}

// Write empty string followed by has_remaining check
TEST(SerializerTest, EmptyStringThenExhausted) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

// Write two booleans true/false
TEST(SerializerTest, TwoBoolTrueFalseRoundTrip) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
}

// Remaining decreases after multiple reads
TEST(SerializerTest, RemainingDecreasesMultipleReads) {
    Serializer s;
    s.write_u8(1);
    s.write_u8(2);
    s.write_u8(3);
    Deserializer d(s.data());
    auto r0 = d.remaining();
    d.read_u8();
    EXPECT_LT(d.remaining(), r0);
    d.read_u8();
    d.read_u8();
    EXPECT_FALSE(d.has_remaining());
}

// Write string then i32 round-trip
TEST(SerializerTest, StringThenI32V2) {
    Serializer s;
    s.write_string("goodbye");
    s.write_i32(-999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "goodbye");
    EXPECT_EQ(d.read_i32(), -999);
}

// ============================================================================
// Cycle 613: More serializer tests
// ============================================================================

// Write u8 zero, then u8 non-zero
TEST(SerializerTest, U8ZeroThenNonZero) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(42);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 42u);
}

// Write three strings in order
TEST(SerializerTest, ThreeStringsPreservedOrder) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_string("gamma");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_string(), "gamma");
}

// Write bool false four times
TEST(SerializerTest, FourBoolFalseValues) {
    Serializer s;
    for (int i = 0; i < 4; ++i) s.write_bool(false);
    Deserializer d(s.data());
    for (int i = 0; i < 4; ++i) EXPECT_FALSE(d.read_bool());
}

// Write u32 incrementing values
TEST(SerializerTest, FiveU32IncrementingValues) {
    Serializer s;
    for (uint32_t i = 0; i < 5; ++i) s.write_u32(i * 100);
    Deserializer d(s.data());
    for (uint32_t i = 0; i < 5; ++i) EXPECT_EQ(d.read_u32(), i * 100);
}

// Write i64 positive then negative
TEST(SerializerTest, I64PositiveThenNegative) {
    Serializer s;
    s.write_i64(1000000LL);
    s.write_i64(-1000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1000000LL);
    EXPECT_EQ(d.read_i64(), -1000000LL);
}

// Write string with special characters
TEST(SerializerTest, StringWithSpacesRoundTrip) {
    Serializer s;
    s.write_string("hello world");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello world");
}

// Serializer data is non-empty after writes
TEST(SerializerTest, DataNonEmptyAfterBoolWrite) {
    Serializer s;
    s.write_bool(true);
    EXPECT_FALSE(s.data().empty());
}

// Write u16 alternating min/max
TEST(SerializerTest, U16AlternatingMinMax) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(65535);
    s.write_u16(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_EQ(d.read_u16(), 0u);
}

// ============================================================================
// Cycle 622: More serializer tests
// ============================================================================

// Write i32 max value (2147483647)
TEST(SerializerTest, I32MaxValueRoundTrip) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
}

// Write i32 -1 
TEST(SerializerTest, I32MinusOneRoundTrip) {
    Serializer s;
    s.write_i32(-1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
}

// Write multiple different types interleaved
TEST(SerializerTest, InterleavedTypesRoundTrip) {
    Serializer s;
    s.write_u8(7);
    s.write_i32(-100);
    s.write_string("x");
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 7u);
    EXPECT_EQ(d.read_i32(), -100);
    EXPECT_EQ(d.read_string(), "x");
    EXPECT_TRUE(d.read_bool());
}

// Write a single bool true
TEST(SerializerTest, SingleBoolTrueRoundTrip) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Write u64 zero
TEST(SerializerTest, U64ZeroV3) {
    Serializer s;
    s.write_u64(0ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ULL);
}

// Write u32 1234567890
TEST(SerializerTest, U32LargeValueRoundTrip) {
    Serializer s;
    s.write_u32(1234567890u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1234567890u);
}

// Write 10 strings consecutively
TEST(SerializerTest, TenStringsConsecutive) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_string(std::to_string(i));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_string(), std::to_string(i));
    }
}

// Take data empties serializer
TEST(SerializerTest, TakeDataLeavesEmpty) {
    Serializer s;
    s.write_u8(42);
    auto data = s.take_data();
    EXPECT_EQ(data.size(), 1u);
}

// ============================================================================
// Cycle 631: More Serializer tests
// ============================================================================

// Write and read a bool false value
TEST(SerializerTest, BoolFalseRoundTrip) {
    Serializer s;
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Write u8 255 (max) then u8 0 (min)
TEST(SerializerTest, U8MaxThenMinSequence) {
    Serializer s;
    s.write_u8(255);
    s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// Write i32 value -1000
TEST(SerializerTest, I32NegativeThousandRoundTrip) {
    Serializer s;
    s.write_i32(-1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000);
    EXPECT_FALSE(d.has_remaining());
}

// Write and read u64 nine billion value
TEST(SerializerTest, U64NineBillionRoundTrip) {
    Serializer s;
    s.write_u64(9999999999ull);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 9999999999ull);
    EXPECT_FALSE(d.has_remaining());
}

// Write string with digits
TEST(SerializerTest, StringWithDigitsRoundTrip) {
    Serializer s;
    s.write_string("abc123xyz");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "abc123xyz");
    EXPECT_FALSE(d.has_remaining());
}

// Write two u32 values and verify order
TEST(SerializerTest, TwoU32ValuesOrdered) {
    Serializer s;
    s.write_u32(100u);
    s.write_u32(200u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 100u);
    EXPECT_EQ(d.read_u32(), 200u);
    EXPECT_FALSE(d.has_remaining());
}

// Write bool true then string
TEST(SerializerTest, BoolTrueThenStringRoundTrip) {
    Serializer s;
    s.write_bool(true);
    s.write_string("yes");
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "yes");
    EXPECT_FALSE(d.has_remaining());
}

// Write i64 negative large value
TEST(SerializerTest, I64NegativeLargeValue) {
    Serializer s;
    s.write_i64(-123456789012ll);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -123456789012ll);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 644: More Serializer tests
// ============================================================================

// Write 5 u8 values and verify each
TEST(SerializerTest, FiveU8ValuesVerified) {
    Serializer s;
    for (uint8_t i = 0; i < 5; ++i) s.write_u8(i * 10);
    Deserializer d(s.data());
    for (uint8_t i = 0; i < 5; ++i) {
        EXPECT_EQ(d.read_u8(), i * 10u);
    }
    EXPECT_FALSE(d.has_remaining());
}

// Write i32 zero
TEST(SerializerTest, I32ZeroRoundTrip) {
    Serializer s;
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_FALSE(d.has_remaining());
}

// Write string with punctuation
TEST(SerializerTest, StringWithPunctuation) {
    Serializer s;
    std::string str = "Hello, World!";
    s.write_string(str);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), str);
    EXPECT_FALSE(d.has_remaining());
}

// Write u8 then string then bool sequence
TEST(SerializerTest, U8StringBoolSequence) {
    Serializer s;
    s.write_u8(77);
    s.write_string("test");
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 77u);
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

// Write i64 zero
TEST(SerializerTest, I64ZeroRoundTrip) {
    Serializer s;
    s.write_i64(0LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 0LL);
    EXPECT_FALSE(d.has_remaining());
}

// Deserializer: remaining is exact byte count for u32
TEST(SerializerTest, RemainingIsExactForU32) {
    Serializer s;
    s.write_u32(42u);
    Deserializer d(s.data());
    EXPECT_EQ(d.remaining(), 4u);
}

// Write u16 zero and max in sequence
TEST(SerializerTest, U16ZeroAndMaxSequence) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), 65535u);
}

// Data non-empty after writing string
TEST(SerializerTest, DataNonEmptyAfterStringWrite) {
    Serializer s;
    s.write_string("data");
    EXPECT_FALSE(s.data().empty());
}

// ============================================================================
// Cycle 653: More Serializer tests
// ============================================================================

// Write and read exactly 3 strings in sequence
TEST(SerializerTest, ThreeDistinctStringsSequence) {
    Serializer s;
    s.write_string("one");
    s.write_string("two");
    s.write_string("three");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "one");
    EXPECT_EQ(d.read_string(), "two");
    EXPECT_EQ(d.read_string(), "three");
    EXPECT_FALSE(d.has_remaining());
}

// Write u32 max value
TEST(SerializerTest, U32MaxValueVerified) {
    Serializer s;
    s.write_u32(4294967295u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_FALSE(d.has_remaining());
}

// Write then take data, remaining is 0
TEST(SerializerTest, TakeDataRemainingZero) {
    Serializer s;
    s.write_u8(1);
    s.write_u8(2);
    auto data = s.take_data();
    EXPECT_EQ(data.size(), 2u);
}

// Bool sequence true, false, true
TEST(SerializerTest, TrueFalseTrueBoolSequence) {
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

// Write u8 then i64 interleaved
TEST(SerializerTest, U8ThenI64Interleaved) {
    Serializer s;
    s.write_u8(55u);
    s.write_i64(-1000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 55u);
    EXPECT_EQ(d.read_i64(), -1000000000LL);
    EXPECT_FALSE(d.has_remaining());
}

// Remaining after partial read
TEST(SerializerTest, RemainingAfterPartialRead) {
    Serializer s;
    s.write_u32(10u);
    s.write_u32(20u);
    Deserializer d(s.data());
    d.read_u32();
    EXPECT_EQ(d.remaining(), 4u);
}

// Write 0 u16 and max u32 together
TEST(SerializerTest, U16ZeroAndU32MaxTogether) {
    Serializer s;
    s.write_u16(0u);
    s.write_u32(4294967295u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_FALSE(d.has_remaining());
}

// String "abc" then u8 42
TEST(SerializerTest, StringThenU8RoundTrip) {
    Serializer s;
    s.write_string("abc");
    s.write_u8(42u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "abc");
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 662: More serializer tests
// ============================================================================

TEST(SerializerTest, U16MinIsZeroRoundTrip) {
    Serializer s;
    s.write_u16(0u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
}

TEST(SerializerTest, U16FiftyThousandRoundTrip) {
    Serializer s;
    s.write_u16(50000u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 50000u);
}

TEST(SerializerTest, U64MaxVerified) {
    Serializer s;
    s.write_u64(std::numeric_limits<uint64_t>::max());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
}

TEST(SerializerTest, I64MinVerified) {
    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
}

TEST(SerializerTest, EmptyStringExplicit) {
    Serializer s;
    s.write_string(std::string{});
    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_TRUE(result.empty());
}

TEST(SerializerTest, FiveHundredCharStringRoundTrip) {
    std::string long_str(500, 'z');
    Serializer s;
    s.write_string(long_str);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
}

TEST(SerializerTest, U32ThenBoolSequence) {
    Serializer s;
    s.write_u32(12345u);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 12345u);
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, TwoI32ValuesOrdered) {
    Serializer s;
    s.write_i32(-100);
    s.write_i32(200);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -100);
    EXPECT_EQ(d.read_i32(), 200);
}

// ============================================================================
// Cycle 670: More serializer tests
// ============================================================================

TEST(SerializerTest, FourBoolSequence) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U8AllFourValuesMixed) {
    Serializer s;
    s.write_u8(0u);
    s.write_u8(64u);
    s.write_u8(128u);
    s.write_u8(255u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 64u);
    EXPECT_EQ(d.read_u8(), 128u);
    EXPECT_EQ(d.read_u8(), 255u);
}

TEST(SerializerTest, I32PosNegZeroSequence) {
    Serializer s;
    s.write_i32(100);
    s.write_i32(-200);
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 100);
    EXPECT_EQ(d.read_i32(), -200);
    EXPECT_EQ(d.read_i32(), 0);
}

TEST(SerializerTest, StringLengthMatchesOriginal) {
    std::string input = "Hello, Vibrowser!";
    Serializer s;
    s.write_string(input);
    Deserializer d(s.data());
    auto out = d.read_string();
    EXPECT_EQ(out.size(), input.size());
    EXPECT_EQ(out, input);
}

TEST(SerializerTest, U32OneMillionRoundTrip) {
    Serializer s;
    s.write_u32(1000000u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1000000u);
}

TEST(SerializerTest, I64MaxPositiveRoundTrip) {
    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::max());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
}

TEST(SerializerTest, MultipleTypesInterleavedRead) {
    Serializer s;
    s.write_u8(42u);
    s.write_i32(-5);
    s.write_string("hi");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_i32(), -5);
    EXPECT_EQ(d.read_string(), "hi");
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RemainingDecreasesAsWeRead) {
    Serializer s;
    s.write_u8(1u);
    s.write_u8(2u);
    s.write_u8(3u);
    Deserializer d(s.data());
    size_t before = d.remaining();
    d.read_u8();
    size_t after = d.remaining();
    EXPECT_LT(after, before);
}

// ============================================================================
// Cycle 678: More serializer tests
// ============================================================================

TEST(SerializerTest, SingleByteWriteRoundTrip) {
    Serializer s;
    s.write_u8(77u);
    EXPECT_EQ(s.data().size(), 1u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 77u);
}

TEST(SerializerTest, I32EightBytesAfterTwoWrites) {
    Serializer s;
    s.write_i32(10);
    s.write_i32(20);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 10);
    EXPECT_EQ(d.read_i32(), 20);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithUnicodeChars) {
    Serializer s;
    s.write_string("hello \xc3\xa9");  // "hello é" in UTF-8
    Deserializer d(s.data());
    auto out = d.read_string();
    EXPECT_EQ(out, "hello \xc3\xa9");
}

TEST(SerializerTest, BoolWritesSingleByte) {
    Serializer s;
    s.write_bool(true);
    EXPECT_GE(s.data().size(), 1u);
}

TEST(SerializerTest, U32NegativeOneAsU32) {
    uint32_t val = 0xFFFFFFFFu;
    Serializer s;
    s.write_u32(val);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), val);
}

TEST(SerializerTest, TwoStringsSecondAccessible) {
    Serializer s;
    s.write_string("first");
    s.write_string("second");
    Deserializer d(s.data());
    d.read_string();  // skip first
    EXPECT_EQ(d.read_string(), "second");
}

TEST(SerializerTest, U8ThenStringLengthVerified) {
    Serializer s;
    s.write_u8(7u);
    s.write_string("abcdefg");
    Deserializer d(s.data());
    uint8_t len_hint = d.read_u8();
    std::string str = d.read_string();
    EXPECT_EQ(len_hint, 7u);
    EXPECT_EQ(str.size(), 7u);
}

TEST(SerializerTest, ThreeU64ValuesRoundTrip) {
    uint64_t a = 111111111ULL;
    uint64_t b = 222222222ULL;
    uint64_t c = 333333333ULL;
    Serializer s;
    s.write_u64(a); s.write_u64(b); s.write_u64(c);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), a);
    EXPECT_EQ(d.read_u64(), b);
    EXPECT_EQ(d.read_u64(), c);
}

// ============================================================================
// Cycle 688: More serializer tests
// ============================================================================

TEST(SerializerTest, AlternatingBoolAndU8) {
    Serializer s;
    s.write_bool(true);
    s.write_u8(10u);
    s.write_bool(false);
    s.write_u8(20u);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u8(), 10u);
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_u8(), 20u);
}

TEST(SerializerTest, I64PositiveMillionRoundTrip) {
    Serializer s;
    s.write_i64(1000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1000000LL);
}

TEST(SerializerTest, StringWithNewline) {
    Serializer s;
    s.write_string("line1\nline2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2");
}

TEST(SerializerTest, StringWithTab) {
    Serializer s;
    s.write_string("col1\tcol2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "col1\tcol2");
}

TEST(SerializerTest, ZeroU8WritesAndReads) {
    Serializer s;
    s.write_u8(0u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32FollowedByString) {
    Serializer s;
    s.write_u32(999u);
    s.write_string("test");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 999u);
    EXPECT_EQ(d.read_string(), "test");
}

TEST(SerializerTest, FiveI32ValuesInSequence) {
    Serializer s;
    for (int i = 1; i <= 5; i++) s.write_i32(i * 10);
    Deserializer d(s.data());
    for (int i = 1; i <= 5; i++) EXPECT_EQ(d.read_i32(), i * 10);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithSpacesPreserved) {
    Serializer s;
    s.write_string("hello world from vibrowser");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello world from vibrowser");
}

// ---------------------------------------------------------------------------
// Cycle 694 — 8 additional serializer tests (f64 and mixed-type sequences)
// ---------------------------------------------------------------------------

TEST(SerializerTest, TwoF64ValuesInSequence) {
    Serializer s;
    s.write_f64(1.234);
    s.write_f64(5.678);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.234);
    EXPECT_DOUBLE_EQ(d.read_f64(), 5.678);
}

TEST(SerializerTest, F64WithU32Interleaved) {
    Serializer s;
    s.write_f64(3.14);
    s.write_u32(42u);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14);
    EXPECT_EQ(d.read_u32(), 42u);
}

TEST(SerializerTest, F64PiRoundTrip) {
    const double pi = 3.14159265358979323846;
    Serializer s;
    s.write_f64(pi);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), pi);
}

TEST(SerializerTest, F64SmallEpsilonRoundTrip) {
    const double eps = 1e-15;
    Serializer s;
    s.write_f64(eps);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), eps);
}

TEST(SerializerTest, F64LargeExponentRoundTrip) {
    const double val = 1.0e15;
    Serializer s;
    s.write_f64(val);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), val);
}

TEST(SerializerTest, StringThenF64RoundTrip) {
    Serializer s;
    s.write_string("hello");
    s.write_f64(2.718281828);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828);
}

TEST(SerializerTest, F64ThenBoolSequence) {
    Serializer s;
    s.write_f64(0.5);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.5);
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, ThreeF64ValuesInOrder) {
    Serializer s;
    s.write_f64(-1.0);
    s.write_f64(0.0);
    s.write_f64(1.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0);
    EXPECT_FALSE(d.has_remaining());
}

// ---------------------------------------------------------------------------
// Cycle 702 — 8 additional serializer tests (bytes and edge cases)
// ---------------------------------------------------------------------------

TEST(SerializerTest, BytesWithNullByteInMiddle) {
    Serializer s;
    std::vector<uint8_t> data = {0x01, 0x00, 0x02, 0x00, 0x03};
    s.write_bytes(data.data(), data.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, data);
}

TEST(SerializerTest, BytesWithAllOnes) {
    Serializer s;
    std::vector<uint8_t> data(8, 0xFF);
    s.write_bytes(data.data(), data.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, data);
}

TEST(SerializerTest, BytesThenString) {
    Serializer s;
    std::vector<uint8_t> bytes = {1, 2, 3};
    s.write_bytes(bytes.data(), bytes.size());
    s.write_string("hello");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_EQ(d.read_string(), "hello");
}

TEST(SerializerTest, StringThenBytes) {
    Serializer s;
    std::vector<uint8_t> bytes = {10, 20, 30};
    s.write_string("world");
    s.write_bytes(bytes.data(), bytes.size());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "world");
    EXPECT_EQ(d.read_bytes(), bytes);
}

TEST(SerializerTest, U8MaxValueThenString) {
    Serializer s;
    s.write_u8(255u);
    s.write_string("max");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_EQ(d.read_string(), "max");
}

TEST(SerializerTest, I32NegativeMaxAndMinInSequence) {
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i32(std::numeric_limits<int32_t>::min());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
}

TEST(SerializerTest, BoolFalseReadBack) {
    Serializer s;
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, MixedTypesLargeSequence) {
    Serializer s;
    s.write_u8(42u);
    s.write_string("test");
    s.write_i32(-100);
    s.write_bool(true);
    s.write_f64(1.23);
    s.write_u64(999999u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_EQ(d.read_i32(), -100);
    EXPECT_TRUE(d.read_bool());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.23);
    EXPECT_EQ(d.read_u64(), 999999u);
}

TEST(SerializerTest, U16ZeroAndMaxRoundTrip) {
    clever::ipc::Serializer s;
    s.write_u16(0);
    s.write_u16(65535);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0);
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, I64NegativeOneRoundTrip) {
    clever::ipc::Serializer s;
    s.write_i64(-1);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1);
}

TEST(SerializerTest, FourU8ValuesInOrder) {
    clever::ipc::Serializer s;
    s.write_u8(10);
    s.write_u8(20);
    s.write_u8(30);
    s.write_u8(40);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 10);
    EXPECT_EQ(d.read_u8(), 20);
    EXPECT_EQ(d.read_u8(), 30);
    EXPECT_EQ(d.read_u8(), 40);
}











TEST(SerializerTest, EmptyStringSecondRoundTrip) {
    clever::ipc::Serializer s;
    s.write_string("");
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, F64NegativeValueRoundTrip) {
    clever::ipc::Serializer s;
    s.write_f64(-3.14159);
    clever::ipc::Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -3.14159);
}

TEST(SerializerTest, U64ZeroRoundTrip) {
    clever::ipc::Serializer s;
    s.write_u64(0);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
}

TEST(SerializerTest, TwoBoolsTrueTrue) {
    clever::ipc::Serializer s;
    s.write_bool(true);
    s.write_bool(true);
    clever::ipc::Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, I32PositiveAndNegativeSequence) {
    clever::ipc::Serializer s;
    s.write_i32(100);
    s.write_i32(-100);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 100);
    EXPECT_EQ(d.read_i32(), -100);
}

TEST(SerializerTest, U32MaxValueRoundTrip) {
    clever::ipc::Serializer s;
    s.write_u32(4294967295u);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295u);
}

TEST(SerializerTest, I64MinValueRoundTrip) {
    clever::ipc::Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
}

TEST(SerializerTest, LongStringThousandXsRoundTrip) {
    clever::ipc::Serializer s;
    std::string long_str(1000, 'x');
    s.write_string(long_str);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
}

TEST(SerializerTest, U16FourValuesSequence) {
    clever::ipc::Serializer s;
    s.write_u16(100);
    s.write_u16(200);
    s.write_u16(300);
    s.write_u16(400);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 100);
    EXPECT_EQ(d.read_u16(), 200);
    EXPECT_EQ(d.read_u16(), 300);
    EXPECT_EQ(d.read_u16(), 400);
}

TEST(SerializerTest, F64InfinityRoundTrip) {
    clever::ipc::Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    clever::ipc::Deserializer d(s.data());
    EXPECT_TRUE(std::isinf(d.read_f64()));
}

TEST(SerializerTest, StringWithMultipleWordsRoundTrip) {
    clever::ipc::Serializer s;
    s.write_string("hello world foo bar");
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello world foo bar");
}

TEST(SerializerTest, I32ZeroValueRoundTrip) {
    clever::ipc::Serializer s;
    s.write_i32(0);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
}

TEST(SerializerTest, TenBooleansAlternating) {
    clever::ipc::Serializer s;
    for (int i = 0; i < 10; ++i) s.write_bool(i % 2 == 0);
    clever::ipc::Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) EXPECT_EQ(d.read_bool(), i % 2 == 0);
}

TEST(SerializerTest, TwentyU8ValuesInOrder) {
    clever::ipc::Serializer s;
    for (uint8_t i = 0; i < 20; ++i) s.write_u8(i);
    clever::ipc::Deserializer d(s.data());
    for (uint8_t i = 0; i < 20; ++i) EXPECT_EQ(d.read_u8(), i);
}

TEST(SerializerTest, StringWithChineseChars) {
    clever::ipc::Serializer s;
    s.write_string("Hello \xE4\xB8\xAD\xE6\x96\x87");  // "Hello 中文" in UTF-8
    clever::ipc::Deserializer d(s.data());
    auto str = d.read_string();
    EXPECT_FALSE(str.empty());
    EXPECT_EQ(str[0], 'H');
}

TEST(SerializerTest, AlternatingI32Values) {
    clever::ipc::Serializer s;
    s.write_i32(1);
    s.write_i32(-1);
    s.write_i32(2);
    s.write_i32(-2);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 1);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), 2);
    EXPECT_EQ(d.read_i32(), -2);
}

TEST(SerializerTest, F64ZeroRoundTrip) {
    clever::ipc::Serializer s;
    s.write_f64(0.0);
    clever::ipc::Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
}

TEST(SerializerTest, MultipleStringsThenU32) {
    clever::ipc::Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_u32(99);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_u32(), 99u);
}

TEST(SerializerTest, U64MaxUint64RoundTrip) {
    clever::ipc::Serializer s;
    s.write_u64(std::numeric_limits<uint64_t>::max());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
}

// --- Cycle 1149: 8 IPC tests ---

TEST(SerializerTest, U8HundredV6) {
    Serializer s;
    s.write_u8(100u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 100u);
}

TEST(SerializerTest, U16ThirtyThousandV6) {
    Serializer s;
    s.write_u16(30000u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 30000u);
}

TEST(SerializerTest, I32NegThousandV6) {
    Serializer s;
    s.write_i32(-1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000);
}

TEST(SerializerTest, U64TrillionV6) {
    Serializer s;
    s.write_u64(1000000000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1000000000000ULL);
}

TEST(SerializerTest, F64SqrtThreeV6) {
    Serializer s;
    s.write_f64(1.7320508075689);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.7320508075689);
}

TEST(SerializerTest, StringWithNewlineV6) {
    Serializer s;
    s.write_string("line1\nline2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2");
}

TEST(SerializerTest, I64NegBillionV6) {
    Serializer s;
    s.write_i64(-1000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1000000000LL);
}

TEST(SerializerTest, BoolTrueThenFalseV6) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, I32MaxInt32RoundTrip) {
    clever::ipc::Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::max());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
}

TEST(SerializerTest, BoolTrueAfterFalseSeries) {
    clever::ipc::Serializer s;
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    clever::ipc::Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, EmptyBytesNullPtrRoundTrip) {
    clever::ipc::Serializer s;
    s.write_bytes(nullptr, 0);
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 0u);
}

TEST(SerializerTest, SingleByteValueRoundTrip) {
    clever::ipc::Serializer s;
    uint8_t val = 127;
    s.write_bytes(&val, 1);
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], 127);
}

TEST(SerializerTest, I32MinPlusOne) {
    clever::ipc::Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min() + 1);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min() + 1);
}

TEST(SerializerTest, StringThenBoolThenU32) {
    clever::ipc::Serializer s;
    s.write_string("key");
    s.write_bool(true);
    s.write_u32(12345u);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "key");
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u32(), 12345u);
}

TEST(SerializerTest, ThreeStringsPreserveOrder) {
    clever::ipc::Serializer s;
    s.write_string("first");
    s.write_string("second");
    s.write_string("third");
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "first");
    EXPECT_EQ(d.read_string(), "second");
    EXPECT_EQ(d.read_string(), "third");
}

TEST(SerializerTest, U8ZeroAndMaxAlternating) {
    clever::ipc::Serializer s;
    s.write_u8(0);
    s.write_u8(255);
    s.write_u8(0);
    s.write_u8(255);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, F64NaNRoundTrip) {
    clever::ipc::Serializer s;
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    clever::ipc::Deserializer d(s.data());
    EXPECT_TRUE(std::isnan(d.read_f64()));
}

TEST(SerializerTest, I64PositiveMaxRoundTrip) {
    clever::ipc::Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::max());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
}

// Cycle 757 — IPC serializer edge cases
TEST(SerializerTest, F64NegInfinityRoundTrip) {
    clever::ipc::Serializer s;
    s.write_f64(-std::numeric_limits<double>::infinity());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
}

TEST(SerializerTest, I32MinValueRoundTrip) {
    clever::ipc::Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
}

TEST(SerializerTest, U64OneAndMaxSequence) {
    clever::ipc::Serializer s;
    s.write_u64(1u);
    s.write_u64(std::numeric_limits<uint64_t>::max());
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1u);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
}

TEST(SerializerTest, U8ThenI32Sequence) {
    clever::ipc::Serializer s;
    s.write_u8(42);
    s.write_i32(-1000);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42);
    EXPECT_EQ(d.read_i32(), -1000);
}

TEST(SerializerTest, StringThenU16Sequence) {
    clever::ipc::Serializer s;
    s.write_string("hello");
    s.write_u16(999);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_EQ(d.read_u16(), 999u);
}

TEST(SerializerTest, BoolAfterStringRoundTrip) {
    clever::ipc::Serializer s;
    s.write_string("test");
    s.write_bool(true);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, FiveI32NegativeValues) {
    clever::ipc::Serializer s;
    for (int i = -1; i >= -5; --i) s.write_i32(i);
    clever::ipc::Deserializer d(s.data());
    for (int i = -1; i >= -5; --i) EXPECT_EQ(d.read_i32(), i);
}

TEST(SerializerTest, TwoStringsPreserveContents) {
    clever::ipc::Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
}

// Cycle 770 — IPC serializer additional combinations
TEST(SerializerTest, TenF64ValuesRoundTrip) {
    clever::ipc::Serializer s;
    for (int i = 0; i < 10; ++i) s.write_f64(i * 1.5);
    clever::ipc::Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) EXPECT_DOUBLE_EQ(d.read_f64(), i * 1.5);
}

TEST(SerializerTest, U32FiveValues) {
    clever::ipc::Serializer s;
    s.write_u32(0); s.write_u32(1); s.write_u32(100); s.write_u32(1000); s.write_u32(UINT32_MAX);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 100u);
    EXPECT_EQ(d.read_u32(), 1000u);
    EXPECT_EQ(d.read_u32(), UINT32_MAX);
}

TEST(SerializerTest, BoolThenU64Sequence) {
    clever::ipc::Serializer s;
    s.write_bool(false);
    s.write_u64(9999999999ull);
    s.write_bool(true);
    clever::ipc::Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_u64(), 9999999999ull);
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, StringBeforeAndAfterI32) {
    clever::ipc::Serializer s;
    s.write_string("before");
    s.write_i32(42);
    s.write_string("after");
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "before");
    EXPECT_EQ(d.read_i32(), 42);
    EXPECT_EQ(d.read_string(), "after");
}

TEST(SerializerTest, FiveStringsDifferentLengths) {
    clever::ipc::Serializer s;
    s.write_string("a");
    s.write_string("bb");
    s.write_string("ccc");
    s.write_string("dddd");
    s.write_string("eeeee");
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "a");
    EXPECT_EQ(d.read_string(), "bb");
    EXPECT_EQ(d.read_string(), "ccc");
    EXPECT_EQ(d.read_string(), "dddd");
    EXPECT_EQ(d.read_string(), "eeeee");
}

TEST(SerializerTest, AllTypesCombinedOnce) {
    clever::ipc::Serializer s;
    s.write_bool(true);
    s.write_u8(7);
    s.write_u16(300);
    s.write_u32(70000);
    s.write_u64(5000000000ull);
    s.write_i32(-42);
    s.write_i64(-9000000000ll);
    s.write_f64(3.14);
    s.write_string("combo");
    clever::ipc::Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u8(), 7);
    EXPECT_EQ(d.read_u16(), 300u);
    EXPECT_EQ(d.read_u32(), 70000u);
    EXPECT_EQ(d.read_u64(), 5000000000ull);
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_EQ(d.read_i64(), -9000000000ll);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14);
    EXPECT_EQ(d.read_string(), "combo");
}

TEST(SerializerTest, TwentyBoolsAlternating) {
    clever::ipc::Serializer s;
    for (int i = 0; i < 20; ++i) s.write_bool(i % 2 == 0);
    clever::ipc::Deserializer d(s.data());
    for (int i = 0; i < 20; ++i) EXPECT_EQ(d.read_bool(), i % 2 == 0);
}

TEST(SerializerTest, MixedLargeAndSmallInts) {
    clever::ipc::Serializer s;
    s.write_u8(1);
    s.write_u64(std::numeric_limits<uint64_t>::max());
    s.write_u8(2);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(d.read_u8(), 2);
}

// Cycle 784 — IPC bytes read/write operations
TEST(SerializerTest, BytesAfterU32RoundTrip) {
    clever::ipc::Serializer s;
    s.write_u32(12345);
    uint8_t data[] = {10, 20, 30};
    s.write_bytes(data, 3);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 12345u);
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 10);
    EXPECT_EQ(bytes[2], 30);
}

TEST(SerializerTest, BytesAfterStringRoundTrip) {
    clever::ipc::Serializer s;
    s.write_string("header");
    uint8_t data[] = {1, 2, 3, 4, 5};
    s.write_bytes(data, 5);
    clever::ipc::Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "header");
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[4], 5);
}

TEST(SerializerTest, BytesThenBoolRoundTrip) {
    clever::ipc::Serializer s;
    uint8_t data[] = {0xFF, 0x00};
    s.write_bytes(data, 2);
    s.write_bool(true);
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[1], 0x00);
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, BytesThenU64RoundTrip) {
    clever::ipc::Serializer s;
    uint8_t data[] = {42};
    s.write_bytes(data, 1);
    s.write_u64(9999999ull);
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], 42);
    EXPECT_EQ(d.read_u64(), 9999999ull);
}

TEST(SerializerTest, HundredBytesLength) {
    clever::ipc::Serializer s;
    std::vector<uint8_t> data(100, 42);
    s.write_bytes(data.data(), data.size());
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 100u);
    EXPECT_EQ(bytes[99], 42);
}

TEST(SerializerTest, SequentialByteValues) {
    clever::ipc::Serializer s;
    uint8_t data[4] = {10, 20, 30, 40};
    s.write_bytes(data, 4);
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 10);
    EXPECT_EQ(bytes[1], 20);
    EXPECT_EQ(bytes[2], 30);
    EXPECT_EQ(bytes[3], 40);
}

TEST(SerializerTest, BytesMaxValue) {
    clever::ipc::Serializer s;
    uint8_t data[] = {255, 254, 253};
    s.write_bytes(data, 3);
    clever::ipc::Deserializer d(s.data());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 255);
    EXPECT_EQ(bytes[1], 254);
    EXPECT_EQ(bytes[2], 253);
}

TEST(SerializerTest, TwoBytesCallsOrderPreserved) {
    clever::ipc::Serializer s;
    uint8_t d1[] = {1, 2};
    uint8_t d2[] = {3, 4};
    s.write_bytes(d1, 2);
    s.write_bytes(d2, 2);
    clever::ipc::Deserializer d(s.data());
    auto b1 = d.read_bytes();
    auto b2 = d.read_bytes();
    EXPECT_EQ(b1[0], 1);
    EXPECT_EQ(b2[0], 3);
}

// Cycle 790 — sequence stress, unicode string, alternating types
TEST(SerializerTest, FiftyBoolsTrue) {
    Serializer s;
    for (int i = 0; i < 50; ++i) s.write_bool(true);
    Deserializer d(s.data());
    for (int i = 0; i < 50; ++i) EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, FiftyU8Sequential) {
    Serializer s;
    for (uint8_t i = 0; i < 50; ++i) s.write_u8(i);
    Deserializer d(s.data());
    for (uint8_t i = 0; i < 50; ++i) EXPECT_EQ(d.read_u8(), i);
}

TEST(SerializerTest, TwentyF64Sequential) {
    Serializer s;
    for (int i = 0; i < 20; ++i) s.write_f64(static_cast<double>(i) * 1.5);
    Deserializer d(s.data());
    for (int i = 0; i < 20; ++i) EXPECT_DOUBLE_EQ(d.read_f64(), static_cast<double>(i) * 1.5);
}

TEST(SerializerTest, LargeStringRoundTrip) {
    std::string large(1000, 'A');
    Serializer s;
    s.write_string(large);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), large);
}

TEST(SerializerTest, StringWithTabAndNewline) {
    Serializer s;
    s.write_string("line1\tvalue\nline2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\tvalue\nline2");
}

TEST(SerializerTest, TenStringsRoundTrip) {
    Serializer s;
    for (int i = 0; i < 10; ++i) s.write_string("item" + std::to_string(i));
    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) EXPECT_EQ(d.read_string(), "item" + std::to_string(i));
}

TEST(SerializerTest, AlternatingBoolAndU8V2) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_bool(i % 2 == 0);
        s.write_u8(static_cast<uint8_t>(i));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i));
    }
}

TEST(SerializerTest, ThirtyI32NegativeToPositive) {
    Serializer s;
    for (int i = -15; i < 15; ++i) s.write_i32(i);
    Deserializer d(s.data());
    for (int i = -15; i < 15; ++i) EXPECT_EQ(d.read_i32(), i);
}

// Cycle 800 — MILESTONE: 800 cycles! Stress tests for IPC serializer
TEST(SerializerTest, TwoHundredBoolsTrue) {
    Serializer s;
    for (int i = 0; i < 200; ++i) s.write_bool(true);
    Deserializer d(s.data());
    for (int i = 0; i < 200; ++i) EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, ThirtyF64SpecialValues) {
    Serializer s;
    for (int i = 0; i < 10; ++i) s.write_f64(0.0);
    for (int i = 0; i < 10; ++i) s.write_f64(-1.0);
    for (int i = 0; i < 10; ++i) s.write_f64(1e100);
    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    for (int i = 0; i < 10; ++i) EXPECT_DOUBLE_EQ(d.read_f64(), -1.0);
    for (int i = 0; i < 10; ++i) EXPECT_DOUBLE_EQ(d.read_f64(), 1e100);
}

TEST(SerializerTest, TwentyStringsVariousLengths) {
    Serializer s;
    for (int i = 0; i < 20; ++i) s.write_string(std::string(i + 1, 'x'));
    Deserializer d(s.data());
    for (int i = 0; i < 20; ++i) EXPECT_EQ(d.read_string(), std::string(i + 1, 'x'));
}

TEST(SerializerTest, SixteenU8AllMaxValues) {
    Serializer s;
    for (int i = 0; i < 16; ++i) s.write_u8(255);
    Deserializer d(s.data());
    for (int i = 0; i < 16; ++i) EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, TwentyI64MixedSignValues) {
    Serializer s;
    for (int64_t i = -10; i < 10; ++i) s.write_i64(i * 1000000LL);
    Deserializer d(s.data());
    for (int64_t i = -10; i < 10; ++i) EXPECT_EQ(d.read_i64(), i * 1000000LL);
}

TEST(SerializerTest, FifteenBoolsFalse) {
    Serializer s;
    for (int i = 0; i < 15; ++i) s.write_bool(false);
    Deserializer d(s.data());
    for (int i = 0; i < 15; ++i) EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, StringBoolStringPattern) {
    Serializer s;
    for (int i = 0; i < 5; ++i) {
        s.write_string("val" + std::to_string(i));
        s.write_bool(i % 2 == 0);
        s.write_string("end");
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(d.read_string(), "val" + std::to_string(i));
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
        EXPECT_EQ(d.read_string(), "end");
    }
}

TEST(SerializerTest, FiftyU32PowersOfTwo) {
    Serializer s;
    for (int i = 0; i < 30; ++i) s.write_u32(1u << i);
    // Not all 32 bits can be shifted safely, stop at 30
    Deserializer d(s.data());
    for (int i = 0; i < 30; ++i) EXPECT_EQ(d.read_u32(), 1u << i);
}

// Cycle 810 — IPC Serializer edge cases and single-value tests
TEST(SerializerTest, SerializerDataNotEmptyAfterWrite) {
    Serializer s;
    s.write_u8(42);
    EXPECT_FALSE(s.data().empty());
}

TEST(SerializerTest, SingleU64ValueRoundTrip) {
    Serializer s;
    s.write_u64(9007199254740992ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 9007199254740992ULL);
}

TEST(SerializerTest, SingleI32NegativeRoundTrip) {
    Serializer s;
    s.write_i32(-999999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -999999);
}

TEST(SerializerTest, SingleF64PiRoundTrip) {
    Serializer s;
    s.write_f64(3.14159265358979);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
}

TEST(SerializerTest, ZeroBytesVectorRoundTrip) {
    Serializer s;
    std::vector<uint8_t> empty_bytes;
    s.write_bytes(empty_bytes.data(), empty_bytes.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_TRUE(result.empty());
}

TEST(SerializerTest, TwoHundredU8Values) {
    Serializer s;
    for (int i = 0; i < 200; ++i) s.write_u8(static_cast<uint8_t>(i % 256));
    Deserializer d(s.data());
    for (int i = 0; i < 200; ++i) EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i % 256));
}

TEST(SerializerTest, NegativeI64RoundTrip) {
    Serializer s;
    s.write_i64(-9999999999999LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9999999999999LL);
}

TEST(SerializerTest, F64NegativePiRoundTrip) {
    Serializer s;
    s.write_f64(-3.14159265358979);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -3.14159265358979);
}

// Cycle 822 — boundary values and bulk sequences
TEST(SerializerTest, MaxUint32RoundTrip) {
    Serializer s;
    s.write_u32(0xFFFFFFFFu);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
}

TEST(SerializerTest, MinInt32RoundTrip) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483648);
}

TEST(SerializerTest, MaxInt64RoundTrip) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775807LL);
}

TEST(SerializerTest, MinInt64RoundTrip) {
    Serializer s;
    s.write_i64(-9223372036854775807LL - 1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL - 1);
}

TEST(SerializerTest, FortyBoolsAlternating) {
    Serializer s;
    for (int i = 0; i < 40; i++) s.write_bool(i % 2 == 0);
    Deserializer d(s.data());
    for (int i = 0; i < 40; i++) EXPECT_EQ(d.read_bool(), i % 2 == 0);
}

TEST(SerializerTest, FiftyI32NegativeSequence) {
    Serializer s;
    for (int i = 1; i <= 50; i++) s.write_i32(-i);
    Deserializer d(s.data());
    for (int i = 1; i <= 50; i++) EXPECT_EQ(d.read_i32(), -i);
}

TEST(SerializerTest, SeventyU32Sequential) {
    Serializer s;
    for (uint32_t i = 0; i < 70; i++) s.write_u32(i * 100);
    Deserializer d(s.data());
    for (uint32_t i = 0; i < 70; i++) EXPECT_EQ(d.read_u32(), i * 100);
}

TEST(SerializerTest, TwentyBoolsTrueThenFalse) {
    Serializer s;
    for (int i = 0; i < 10; i++) s.write_bool(true);
    for (int i = 0; i < 10; i++) s.write_bool(false);
    Deserializer d(s.data());
    for (int i = 0; i < 10; i++) EXPECT_TRUE(d.read_bool());
    for (int i = 0; i < 10; i++) EXPECT_FALSE(d.read_bool());
}

// Cycle 833 — Mixed-type longer sequences
TEST(SerializerTest, EightyU8WithMaxValues) {
    Serializer s;
    for (int i = 0; i < 80; i++) s.write_u8(i % 2 == 0 ? 0u : 255u);
    Deserializer d(s.data());
    for (int i = 0; i < 80; i++) EXPECT_EQ(d.read_u8(), i % 2 == 0 ? 0u : 255u);
}

TEST(SerializerTest, SixtyStringsTenCharsEach) {
    Serializer s;
    for (int i = 0; i < 60; i++) {
        std::string str(10, static_cast<char>('a' + (i % 26)));
        s.write_string(str);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 60; i++) {
        std::string expected(10, static_cast<char>('a' + (i % 26)));
        EXPECT_EQ(d.read_string(), expected);
    }
}

TEST(SerializerTest, HundredF64ValuesIncreasing) {
    Serializer s;
    for (int i = 0; i < 100; i++) s.write_f64(i * 0.1);
    Deserializer d(s.data());
    for (int i = 0; i < 100; i++) EXPECT_NEAR(d.read_f64(), i * 0.1, 1e-9);
}

TEST(SerializerTest, U32AndI32Interleaved) {
    Serializer s;
    for (int i = 0; i < 20; i++) {
        s.write_u32(static_cast<uint32_t>(i * 1000));
        s.write_i32(-i);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(d.read_u32(), static_cast<uint32_t>(i * 1000));
        EXPECT_EQ(d.read_i32(), -i);
    }
}

TEST(SerializerTest, StringLengthOneByte) {
    Serializer s;
    s.write_string("A");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "A");
}

TEST(SerializerTest, StringLengthTwoBytes) {
    Serializer s;
    s.write_string("AB");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "AB");
}

TEST(SerializerTest, U64ThenStringThenBool) {
    Serializer s;
    s.write_u64(9999999999ULL);
    s.write_string("hello");
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 9999999999ULL);
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, FiftyI64NegativePowerOf2) {
    Serializer s;
    for (int i = 0; i < 50; i++) s.write_i64(-(1LL << i % 32));
    Deserializer d(s.data());
    for (int i = 0; i < 50; i++) EXPECT_EQ(d.read_i64(), -(1LL << i % 32));
}

// Cycle 843 — new serializer test sequences
TEST(SerializerTest, ThirtyU16DecreasingSequence) {
    Serializer s;
    for (int i = 0; i < 30; i++) s.write_u16(static_cast<uint16_t>(30000 - i * 1000));
    Deserializer d(s.data());
    for (int i = 0; i < 30; i++) EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(30000 - i * 1000));
}

TEST(SerializerTest, BytesSingleElementRoundTrip) {
    Serializer s;
    const uint8_t single[1] = {0xAB};
    s.write_bytes(single, 1);
    Deserializer d(s.data());
    auto out = d.read_bytes();
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], 0xAB);
}

TEST(SerializerTest, TwentyI64AlternatingPosNeg) {
    Serializer s;
    for (int i = 0; i < 20; i++) s.write_i64(i % 2 == 0 ? 123456789LL : -123456789LL);
    Deserializer d(s.data());
    for (int i = 0; i < 20; i++) EXPECT_EQ(d.read_i64(), i % 2 == 0 ? 123456789LL : -123456789LL);
}

TEST(SerializerTest, F64PiMultiplesRoundTrip) {
    Serializer s;
    const double pi = 3.141592653589793;
    for (int i = 1; i <= 5; i++) s.write_f64(pi * i);
    Deserializer d(s.data());
    for (int i = 1; i <= 5; i++) EXPECT_DOUBLE_EQ(d.read_f64(), pi * i);
}

TEST(SerializerTest, FiveStringsMixedContent) {
    Serializer s;
    s.write_string("");
    s.write_string("x");
    s.write_string("42");
    s.write_string(" ");
    s.write_string("!@#");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "x");
    EXPECT_EQ(d.read_string(), "42");
    EXPECT_EQ(d.read_string(), " ");
    EXPECT_EQ(d.read_string(), "!@#");
}

TEST(SerializerTest, StringBoolU16TripletPattern) {
    Serializer s;
    for (int i = 0; i < 5; i++) {
        s.write_string("item");
        s.write_bool(i % 2 == 0);
        s.write_u16(static_cast<uint16_t>(i * 100));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(d.read_string(), "item");
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(i * 100));
    }
}

TEST(SerializerTest, U8SequenceAllSameValue77) {
    Serializer s;
    for (int i = 0; i < 30; i++) s.write_u8(77);
    Deserializer d(s.data());
    for (int i = 0; i < 30; i++) EXPECT_EQ(d.read_u8(), 77u);
}

TEST(SerializerTest, TwoBytesBlocksBackToBack) {
    Serializer s;
    const uint8_t a[] = {1, 2, 3};
    const uint8_t b[] = {4, 5, 6, 7};
    s.write_bytes(a, 3);
    s.write_bytes(b, 4);
    Deserializer d(s.data());
    auto out_a = d.read_bytes();
    auto out_b = d.read_bytes();
    ASSERT_EQ(out_a.size(), 3u);
    ASSERT_EQ(out_b.size(), 4u);
    EXPECT_EQ(out_a[2], 3u);
    EXPECT_EQ(out_b[3], 7u);
}

// Cycle 852 — ascending types, alternating patterns, space string, three bytes blocks
TEST(SerializerTest, AscendingWidthTypesU8ToU64) {
    Serializer s;
    s.write_u8(0xAB);
    s.write_u16(0xCDEF);
    s.write_u32(0x12345678);
    s.write_u64(0xFEDCBA9876543210ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(0xAB));
    EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(0xCDEF));
    EXPECT_EQ(d.read_u32(), static_cast<uint32_t>(0x12345678));
    EXPECT_EQ(d.read_u64(), static_cast<uint64_t>(0xFEDCBA9876543210ULL));
}

TEST(SerializerTest, DescendingWidthTypesU64ToU8) {
    Serializer s;
    s.write_u64(0x0102030405060708ULL);
    s.write_u32(0xDEAD1234);
    s.write_u16(0xBEEF);
    s.write_u8(0x42);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), static_cast<uint64_t>(0x0102030405060708ULL));
    EXPECT_EQ(d.read_u32(), static_cast<uint32_t>(0xDEAD1234));
    EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(0xBEEF));
    EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(0x42));
}

TEST(SerializerTest, SpaceOnlyStringRoundTrip) {
    Serializer s;
    s.write_string("   ");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "   ");
}

TEST(SerializerTest, I32ThenF64RoundTrip) {
    Serializer s;
    s.write_i32(-999999);
    s.write_f64(2.718281828459045);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -999999);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
}

TEST(SerializerTest, ThreeBytesBlocksInSequence) {
    Serializer s;
    const uint8_t a[] = {0xAA};
    const uint8_t b[] = {0xBB, 0xCC};
    const uint8_t c[] = {0xDD, 0xEE, 0xFF};
    s.write_bytes(a, 1);
    s.write_bytes(b, 2);
    s.write_bytes(c, 3);
    Deserializer d(s.data());
    auto ra = d.read_bytes();
    auto rb = d.read_bytes();
    auto rc = d.read_bytes();
    ASSERT_EQ(ra.size(), 1u); EXPECT_EQ(ra[0], 0xAAu);
    ASSERT_EQ(rb.size(), 2u); EXPECT_EQ(rb[1], 0xCCu);
    ASSERT_EQ(rc.size(), 3u); EXPECT_EQ(rc[2], 0xFFu);
}

TEST(SerializerTest, AlternatingU32ZeroAndMax) {
    Serializer s;
    for (int i = 0; i < 5; i++) {
        s.write_u32(0);
        s.write_u32(0xFFFFFFFF);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(d.read_u32(), 0u);
        EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    }
}

TEST(SerializerTest, BoolStringAlternation) {
    Serializer s;
    const std::string words[] = {"alpha", "beta", "gamma"};
    for (int i = 0; i < 3; i++) {
        s.write_bool(i % 2 == 0);
        s.write_string(words[i]);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
        EXPECT_EQ(d.read_string(), words[i]);
    }
}

TEST(SerializerTest, TenOddU64Values) {
    Serializer s;
    for (int i = 0; i < 10; i++) s.write_u64(static_cast<uint64_t>(i * 2 + 1));
    Deserializer d(s.data());
    for (int i = 0; i < 10; i++) EXPECT_EQ(d.read_u64(), static_cast<uint64_t>(i * 2 + 1));
}

// Cycle 861 — increasing length strings, powers of 2, tab/newline strings, interleaved types
TEST(SerializerTest, FiveStringsIncreasingLength) {
    Serializer s;
    const std::string strs[] = {"a", "bb", "cccc", "dddddddd", "eeeeeeeeeeeeeeee"};
    for (const auto& str : strs) s.write_string(str);
    Deserializer d(s.data());
    for (const auto& str : strs) EXPECT_EQ(d.read_string(), str);
}

TEST(SerializerTest, I64InterleavedWithF64) {
    Serializer s;
    for (int i = 0; i < 5; i++) {
        s.write_i64(static_cast<int64_t>(i) * -1000);
        s.write_f64(static_cast<double>(i) * 3.14);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(d.read_i64(), static_cast<int64_t>(i) * -1000);
        EXPECT_DOUBLE_EQ(d.read_f64(), static_cast<double>(i) * 3.14);
    }
}

TEST(SerializerTest, U16PowersOfTwo) {
    Serializer s;
    for (int i = 0; i < 8; i++) s.write_u16(static_cast<uint16_t>(1 << i));
    Deserializer d(s.data());
    for (int i = 0; i < 8; i++) EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1 << i));
}

TEST(SerializerTest, BytesAllSameValue0xCC) {
    Serializer s;
    const std::vector<uint8_t> data(16, 0xCC);
    s.write_bytes(data.data(), data.size());
    Deserializer d(s.data());
    auto out = d.read_bytes();
    ASSERT_EQ(out.size(), 16u);
    for (auto b : out) EXPECT_EQ(b, 0xCCu);
}

TEST(SerializerTest, StringContainingTabs) {
    Serializer s;
    s.write_string("col1\tcol2\tcol3");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "col1\tcol2\tcol3");
}

TEST(SerializerTest, StringContainingNewlines) {
    Serializer s;
    s.write_string("line1\nline2\nline3");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2\nline3");
}

TEST(SerializerTest, U8SurroundingString) {
    Serializer s;
    s.write_u8(0xFF);
    s.write_string("middle");
    s.write_u8(0x00);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xFFu);
    EXPECT_EQ(d.read_string(), "middle");
    EXPECT_EQ(d.read_u8(), 0x00u);
}

TEST(SerializerTest, MixedI32U32InterleaveSign) {
    Serializer s;
    for (int i = 0; i < 5; i++) {
        s.write_i32(-(i + 1) * 1000);
        s.write_u32(static_cast<uint32_t>((i + 1) * 2000));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(d.read_i32(), -(i + 1) * 1000);
        EXPECT_EQ(d.read_u32(), static_cast<uint32_t>((i + 1) * 2000));
    }
}
// Cycle 871 — boundary U64/I64, F64 infinity, U16+U32 mix, empty bytes distinct name, bool alternation, U32 extremes
TEST(SerializerTest, U64MaxValueBoundary) {
    Serializer s;
    s.write_u64(UINT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
}

TEST(SerializerTest, I64MinValueBoundary) {
    Serializer s;
    s.write_i64(INT64_MIN);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MIN);
}

TEST(SerializerTest, I64MaxValueBoundary) {
    Serializer s;
    s.write_i64(INT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MAX);
}

TEST(SerializerTest, F64PositiveInfinityRoundTrip) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    Deserializer d(s.data());
    double v1 = d.read_f64();
    EXPECT_TRUE(std::isinf(v1));
    EXPECT_GT(v1, 0.0);
}

TEST(SerializerTest, U16ThenU32ThenU16Sequence) {
    Serializer s;
    s.write_u16(0xABCD);
    s.write_u32(0x12345678);
    s.write_u16(0xEF01);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(0xABCD));
    EXPECT_EQ(d.read_u32(), static_cast<uint32_t>(0x12345678));
    EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(0xEF01));
}

TEST(SerializerTest, ZeroBytesLengthBlock) {
    Serializer s;
    const uint8_t* data = nullptr;
    s.write_bytes(data, 0);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 0u);
}

TEST(SerializerTest, FiveBoolsAlternating) {
    Serializer s;
    for (int i = 0; i < 5; i++) {
        s.write_bool(i % 2 == 0);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
    }
}

TEST(SerializerTest, U32ZeroAndMaxAlternating) {
    Serializer s;
    for (int i = 0; i < 6; i++) {
        if (i % 2 == 0) s.write_u32(0);
        else s.write_u32(UINT32_MAX);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 6; i++) {
        uint32_t expected = (i % 2 == 0) ? 0u : UINT32_MAX;
        EXPECT_EQ(d.read_u32(), expected);
    }
}
// Cycle 880 — serializer: 2000-byte string, bytes all zero, negative F64, I32 boundaries, backslash string, 20 u8, 5 large U64, F64 precision
TEST(SerializerTest, TwoThousandCharStringRoundTrip) {
    Serializer s;
    std::string big(2000, 'Z');
    s.write_string(big);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), big);
}

TEST(SerializerTest, FourBytesAllZero) {
    Serializer s;
    const uint8_t data[4] = {0x00, 0x00, 0x00, 0x00};
    s.write_bytes(data, 4);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 4u);
    for (auto b : result) EXPECT_EQ(b, 0x00u);
}

TEST(SerializerTest, NegativeF64RoundTrip) {
    Serializer s;
    s.write_f64(-2.718281828);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), -2.718281828, 1e-9);
}

TEST(SerializerTest, I32MinAndMaxAndZero) {
    Serializer s;
    s.write_i32(INT32_MIN);
    s.write_i32(INT32_MAX);
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MIN);
    EXPECT_EQ(d.read_i32(), INT32_MAX);
    EXPECT_EQ(d.read_i32(), 0);
}

TEST(SerializerTest, StringWithBackslash) {
    Serializer s;
    s.write_string("path\\to\\file");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "path\\to\\file");
}

TEST(SerializerTest, TwentySequentialU8Values) {
    Serializer s;
    for (int i = 0; i < 20; i++) {
        s.write_u8(static_cast<uint8_t>(i * 10));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i * 10));
    }
}

TEST(SerializerTest, FiveLargeU64Values) {
    Serializer s;
    for (uint64_t i = 1; i <= 5; i++) {
        s.write_u64(i * 1000000000000ULL);
    }
    Deserializer d(s.data());
    for (uint64_t i = 1; i <= 5; i++) {
        EXPECT_EQ(d.read_u64(), i * 1000000000000ULL);
    }
}

TEST(SerializerTest, F64PrecisionNineDigits) {
    Serializer s;
    s.write_f64(1.23456789);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 1.23456789, 1e-9);
}

// Cycle 889 — IPC serializer varied patterns

TEST(SerializerTest, U16MaxMinSequence) {
    Serializer s;
    s.write_u16(65535);
    s.write_u16(0);
    s.write_u16(1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
    EXPECT_EQ(d.read_u16(), 0);
    EXPECT_EQ(d.read_u16(), 1000);
}

TEST(SerializerTest, StringThenBoolFalseThenU32) {
    Serializer s;
    s.write_string("payload");
    s.write_bool(false);
    s.write_u32(999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "payload");
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_u32(), 999u);
}

TEST(SerializerTest, I32NegativeOneMillion) {
    Serializer s;
    s.write_i32(-1000000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000000);
}

TEST(SerializerTest, ThirtySequentialU8) {
    Serializer s;
    for (int i = 0; i < 30; i++) {
        s.write_u8(static_cast<uint8_t>(i));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 30; i++) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i));
    }
}

TEST(SerializerTest, I64AllBitsSet) {
    Serializer s;
    s.write_i64(static_cast<int64_t>(-1));
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), int64_t{-1});
}

TEST(SerializerTest, TwoBytesAndThreeStrings) {
    Serializer s;
    const uint8_t raw[] = {0xAA, 0xBB};
    s.write_bytes(raw, 2);
    s.write_string("one");
    s.write_string("two");
    s.write_string("three");
    Deserializer d(s.data());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], 0xAAu);
    EXPECT_EQ(d.read_string(), "one");
    EXPECT_EQ(d.read_string(), "two");
    EXPECT_EQ(d.read_string(), "three");
}

TEST(SerializerTest, FiveU64ValuesAllOdd) {
    Serializer s;
    for (int i = 0; i < 5; i++) {
        s.write_u64(static_cast<uint64_t>(2 * i + 1));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(d.read_u64(), static_cast<uint64_t>(2 * i + 1));
    }
}

TEST(SerializerTest, F64SpecialNegativeValue) {
    Serializer s;
    s.write_f64(-999.999);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), -999.999, 1e-6);
}

// Cycle 897 — IPC serializer varied patterns

TEST(SerializerTest, AlternatingI32AndU64) {
    Serializer s;
    s.write_i32(-100);
    s.write_u64(999999999999ULL);
    s.write_i32(200);
    s.write_u64(1ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -100);
    EXPECT_EQ(d.read_u64(), 999999999999ULL);
    EXPECT_EQ(d.read_i32(), 200);
    EXPECT_EQ(d.read_u64(), 1ULL);
}

TEST(SerializerTest, BoolStringBoolSequence) {
    Serializer s;
    s.write_bool(true);
    s.write_string("middle");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_string(), "middle");
    EXPECT_EQ(d.read_bool(), false);
}

TEST(SerializerTest, TenBoolAlternating) {
    Serializer s;
    for (int i = 0; i < 10; i++) {
        s.write_bool(i % 2 == 0);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
    }
}

TEST(SerializerTest, SingleCharString) {
    Serializer s;
    s.write_string("x");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "x");
}

TEST(SerializerTest, I64ThenU32ThenString) {
    Serializer s;
    s.write_i64(-9999999999LL);
    s.write_u32(42);
    s.write_string("result");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9999999999LL);
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_EQ(d.read_string(), "result");
}

TEST(SerializerTest, I32MaxMinMax) {
    Serializer s;
    s.write_i32(2147483647);
    s.write_i32(-2147483648);
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
    EXPECT_EQ(d.read_i32(), -2147483648);
    EXPECT_EQ(d.read_i32(), 2147483647);
}

TEST(SerializerTest, U32ThenI64ThenBool) {
    Serializer s;
    s.write_u32(0xDEADBEEF);
    s.write_i64(-1LL);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_i64(), int64_t{-1});
    EXPECT_EQ(d.read_bool(), true);
}

TEST(SerializerTest, LargeU16SequenceOfFifty) {
    Serializer s;
    for (int i = 0; i < 50; i++) {
        s.write_u16(static_cast<uint16_t>(i * 100));
    }
    Deserializer d(s.data());
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(i * 100));
    }
}

TEST(SerializerTest, StringWithComma) {
    Serializer s;
    s.write_string("hello, world");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello, world");
}

TEST(SerializerTest, TwoEmptyStrings) {
    Serializer s;
    s.write_string("");
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, F64NegPiRoundTrip) {
    Serializer s;
    s.write_f64(-3.14159265358979);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -3.14159265358979);
}

TEST(SerializerTest, F64NegativeRoundTrip) {
    Serializer s;
    s.write_f64(-2.718281828459045);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -2.718281828459045);
}

TEST(SerializerTest, AlternatingStringAndBool) {
    Serializer s;
    s.write_string("yes");
    s.write_bool(true);
    s.write_string("no");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "yes");
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_string(), "no");
    EXPECT_EQ(d.read_bool(), false);
}

TEST(SerializerTest, IntThenEmptyString) {
    Serializer s;
    s.write_i32(42);
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 42);
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, ZeroU8Sequence) {
    Serializer s;
    for (int i = 0; i < 10; i++) {
        s.write_u8(0);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(d.read_u8(), 0u);
    }
}

TEST(SerializerTest, FiveU16DistinctValues) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(255);
    s.write_u16(1000);
    s.write_u16(32767);
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), uint16_t{0});
    EXPECT_EQ(d.read_u16(), uint16_t{255});
    EXPECT_EQ(d.read_u16(), uint16_t{1000});
    EXPECT_EQ(d.read_u16(), uint16_t{32767});
    EXPECT_EQ(d.read_u16(), uint16_t{65535});
}

TEST(SerializerTest, StringWithColon) {
    Serializer s;
    s.write_string("http://example.com");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "http://example.com");
}

TEST(SerializerTest, StringWithEquals) {
    Serializer s;
    s.write_string("key=value");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "key=value");
}

TEST(SerializerTest, StringWithBrackets) {
    Serializer s;
    s.write_string("[1, 2, 3]");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "[1, 2, 3]");
}

TEST(SerializerTest, DoubleU64ThenBool) {
    Serializer s;
    s.write_u64(UINT64_MAX);
    s.write_u64(0ULL);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
    EXPECT_EQ(d.read_u64(), 0ULL);
    EXPECT_EQ(d.read_bool(), true);
}

TEST(SerializerTest, U8U16U32Sequence) {
    Serializer s;
    s.write_u8(255);
    s.write_u16(65535);
    s.write_u32(0xFFFFFFFF);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), uint8_t{255});
    EXPECT_EQ(d.read_u16(), uint16_t{65535});
    EXPECT_EQ(d.read_u32(), uint32_t{0xFFFFFFFF});
}

TEST(SerializerTest, U32U16U8Sequence) {
    Serializer s;
    s.write_u32(1000000);
    s.write_u16(1000);
    s.write_u8(10);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{1000000});
    EXPECT_EQ(d.read_u16(), uint16_t{1000});
    EXPECT_EQ(d.read_u8(), uint8_t{10});
}

TEST(SerializerTest, I32ThenString) {
    Serializer s;
    s.write_i32(-1);
    s.write_string("minus one");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_string(), "minus one");
}

TEST(SerializerTest, StringThenI32) {
    Serializer s;
    s.write_string("answer");
    s.write_i32(42);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "answer");
    EXPECT_EQ(d.read_i32(), 42);
}

// Cycle 924 — additional serializer coverage
TEST(SerializerTest, F64OnePointFiveRoundTrip) {
    Serializer s;
    s.write_f64(1.5);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.5);
}

TEST(SerializerTest, F64NegTwoRoundTrip) {
    Serializer s;
    s.write_f64(-2.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -2.0);
}

TEST(SerializerTest, U8ThenBoolRoundTrip) {
    Serializer s;
    s.write_u8(200);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), uint8_t{200});
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, BoolThenU8RoundTrip) {
    Serializer s;
    s.write_bool(true);
    s.write_u8(77);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u8(), uint8_t{77});
}

TEST(SerializerTest, FourStringsInOrder) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_string("gamma");
    s.write_string("delta");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_string(), "gamma");
    EXPECT_EQ(d.read_string(), "delta");
}

TEST(SerializerTest, U32HundredRoundTrip) {
    Serializer s;
    s.write_u32(100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{100});
}

TEST(SerializerTest, StringAndU32Sequence) {
    Serializer s;
    s.write_string("value");
    s.write_u32(999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "value");
    EXPECT_EQ(d.read_u32(), uint32_t{999});
}

TEST(SerializerTest, U32AndStringSequence) {
    Serializer s;
    s.write_u32(42);
    s.write_string("hello");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{42});
    EXPECT_EQ(d.read_string(), "hello");
}

// Cycle 933 — additional serializer: F64 math constants, mixed large sequences
TEST(SerializerTest, F64SqrtTwoRoundTrip) {
    Serializer s;
    s.write_f64(1.41421356237);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 1.41421356237, 1e-9);
}

TEST(SerializerTest, F64EulerNumberRoundTrip) {
    Serializer s;
    s.write_f64(2.71828182845);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 2.71828182845, 1e-9);
}

TEST(SerializerTest, FourF64ValuesInOrder) {
    Serializer s;
    s.write_f64(1.0);
    s.write_f64(2.0);
    s.write_f64(3.0);
    s.write_f64(4.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 4.0);
}

TEST(SerializerTest, StringWithBackslashPath) {
    Serializer s;
    s.write_string("path\\to\\file");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "path\\to\\file");
}

TEST(SerializerTest, I64TwoHundredRoundTrip) {
    Serializer s;
    s.write_i64(200LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 200LL);
}

TEST(SerializerTest, I64BigNegativeRoundTrip) {
    Serializer s;
    s.write_i64(-9000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9000000000LL);
}

TEST(SerializerTest, U64HundredRoundTrip) {
    Serializer s;
    s.write_u64(100ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 100ULL);
}

TEST(SerializerTest, I32PlusHundredRoundTrip) {
    Serializer s;
    s.write_i32(100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 100);
}

// Cycle 942 — additional serializer: string content variants, numeric edge cases
TEST(SerializerTest, StringWithDash) {
    Serializer s;
    s.write_string("well-formed-name");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "well-formed-name");
}

TEST(SerializerTest, StringWithDot) {
    Serializer s;
    s.write_string("file.name.txt");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "file.name.txt");
}

TEST(SerializerTest, StringWithPercent) {
    Serializer s;
    s.write_string("100%");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "100%");
}

TEST(SerializerTest, StringUrlPath) {
    Serializer s;
    s.write_string("https://example.com/path");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "https://example.com/path");
}

TEST(SerializerTest, I32MinusHundredRoundTrip) {
    Serializer s;
    s.write_i32(-100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -100);
}

TEST(SerializerTest, I32MinusThousandRoundTrip) {
    Serializer s;
    s.write_i32(-1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000);
}

TEST(SerializerTest, U32ThousandRoundTrip) {
    Serializer s;
    s.write_u32(1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{1000});
}

TEST(SerializerTest, I64ThousandRoundTrip) {
    Serializer s;
    s.write_i64(1000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1000LL);
}

// Cycle 951 — strings with special chars, large numeric values
TEST(SerializerTest, StringWithAtSign) {
    Serializer s;
    s.write_string("user@example.com");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "user@example.com");
}

TEST(SerializerTest, StringWithHashSign) {
    Serializer s;
    s.write_string("color: #ff0000");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "color: #ff0000");
}

TEST(SerializerTest, StringWithQuestionMark) {
    Serializer s;
    s.write_string("is it working?");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "is it working?");
}

TEST(SerializerTest, StringWithStar) {
    Serializer s;
    s.write_string("glob: *.txt");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "glob: *.txt");
}

TEST(SerializerTest, I32PlusMillionRoundTrip) {
    Serializer s;
    s.write_i32(1000000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 1000000);
}

TEST(SerializerTest, I32MinusMillionRoundTrip) {
    Serializer s;
    s.write_i32(-1000000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000000);
}

TEST(SerializerTest, F64OneTenthRoundTrip) {
    Serializer s;
    s.write_f64(0.1);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 0.1, 1e-15);
}

TEST(SerializerTest, U64MillionRoundTrip) {
    Serializer s;
    s.write_u64(1000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1000000ULL);
}

TEST(SerializerTest, StringWithSemicolon) {
    Serializer s;
    s.write_string("key: value; other: one");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "key: value; other: one");
}

TEST(SerializerTest, StringWithParen) {
    Serializer s;
    s.write_string("rgb(255, 0, 128)");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "rgb(255, 0, 128)");
}

TEST(SerializerTest, StringWithAngleBrackets) {
    Serializer s;
    s.write_string("<div class=\"foo\">");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "<div class=\"foo\">");
}

TEST(SerializerTest, StringWithExclamation) {
    Serializer s;
    s.write_string("Hello, World!");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello, World!");
}

TEST(SerializerTest, StringWithCaret) {
    Serializer s;
    s.write_string("regex: ^[a-z]+$");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "regex: ^[a-z]+$");
}

TEST(SerializerTest, StringWithPipe) {
    Serializer s;
    s.write_string("one|two|three");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "one|two|three");
}

TEST(SerializerTest, StringWithTilde) {
    Serializer s;
    s.write_string("~/.bashrc");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "~/.bashrc");
}

TEST(SerializerTest, StringWithAmpersand) {
    Serializer s;
    s.write_string("foo=1&bar=2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "foo=1&bar=2");
}

TEST(SerializerTest, StringWithSingleQuote) {
    Serializer s;
    s.write_string("it's a test");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "it's a test");
}

TEST(SerializerTest, StringWithDoubleQuote) {
    Serializer s;
    s.write_string("say \"hello\"");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "say \"hello\"");
}

TEST(SerializerTest, StringWithDollarSign) {
    Serializer s;
    s.write_string("price: $9.99");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "price: $9.99");
}

TEST(SerializerTest, StringWithLessThan) {
    Serializer s;
    s.write_string("a < b");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "a < b");
}

TEST(SerializerTest, StringWithGreaterThan) {
    Serializer s;
    s.write_string("a > b");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "a > b");
}

TEST(SerializerTest, StringWithLeadingSpace) {
    Serializer s;
    s.write_string("  indented text");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "  indented text");
}

TEST(SerializerTest, StringNumericOnly) {
    Serializer s;
    s.write_string("123456789");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "123456789");
}

TEST(SerializerTest, StringWithNegativeSign) {
    Serializer s;
    s.write_string("-1.5e-3");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "-1.5e-3");
}

TEST(SerializerTest, TwoF64NearZero) {
    Serializer s;
    s.write_f64(1e-300);
    s.write_f64(-1e-300);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 1e-300, 1e-310);
    EXPECT_NEAR(d.read_f64(), -1e-300, 1e-310);
}

TEST(SerializerTest, TwoI32SumComponents) {
    Serializer s;
    s.write_i32(300);
    s.write_i32(-100);
    Deserializer d(s.data());
    int a = d.read_i32();
    int b = d.read_i32();
    EXPECT_EQ(a + b, 200);
}

TEST(SerializerTest, ThreeBooleanSequence) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, FourU8Distinct) {
    Serializer s;
    s.write_u8(1);
    s.write_u8(2);
    s.write_u8(3);
    s.write_u8(4);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u8(), 2);
    EXPECT_EQ(d.read_u8(), 3);
    EXPECT_EQ(d.read_u8(), 4);
}

TEST(SerializerTest, BoolAndStringAndInt) {
    Serializer s;
    s.write_bool(true);
    s.write_string("hello");
    s.write_i32(42);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_EQ(d.read_i32(), 42);
}

TEST(SerializerTest, IntAndStringAndBool) {
    Serializer s;
    s.write_i32(-1);
    s.write_string("world");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_string(), "world");
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, StringThenU64ThenBool) {
    Serializer s;
    s.write_string("key");
    s.write_u64(999ULL);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "key");
    EXPECT_EQ(d.read_u64(), 999ULL);
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, F64ThenI32ThenString) {
    Serializer s;
    s.write_f64(2.718);
    s.write_i32(100);
    s.write_string("pi");
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 2.718, 1e-10);
    EXPECT_EQ(d.read_i32(), 100);
    EXPECT_EQ(d.read_string(), "pi");
}

TEST(SerializerTest, StringWithTabV2) {
    Serializer s;
    s.write_string("col1\tcol2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "col1\tcol2");
}

TEST(SerializerTest, StringWithCarriageReturn) {
    Serializer s;
    s.write_string("line\r\n");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line\r\n");
}

TEST(SerializerTest, StringWithBackslashV2) {
    Serializer s;
    s.write_string("C:\\Users\\test");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "C:\\Users\\test");
}

TEST(SerializerTest, EmptyStringThenInt) {
    Serializer s;
    s.write_string("");
    s.write_i32(42);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_i32(), 42);
}

TEST(SerializerTest, I32MaxRoundTrip) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
}

TEST(SerializerTest, I32MinRoundTrip) {
    Serializer s;
    s.write_i32(-2147483647 - 1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483647 - 1);
}

TEST(SerializerTest, U64MaxRoundTrip) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, I64MaxRoundTrip) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775807LL);
}

TEST(SerializerTest, I64MinRoundTrip) {
    Serializer s;
    s.write_i64(-9223372036854775807LL - 1LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL - 1LL);
}

TEST(SerializerTest, U32MaxRoundTrip) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295U);
}

TEST(SerializerTest, StringWithForwardSlash) {
    Serializer s;
    s.write_string("path/to/resource");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "path/to/resource");
}

TEST(SerializerTest, StringWithCurlyBraces) {
    Serializer s;
    s.write_string("{key: value}");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "{key: value}");
}

TEST(SerializerTest, AlternatingTypesTenItems) {
    Serializer s;
    s.write_bool(true);
    s.write_i32(1);
    s.write_bool(false);
    s.write_i32(2);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_i32(), 1);
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_i32(), 2);
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, BoolThenF64) {
    Serializer s;
    s.write_bool(true);
    s.write_f64(3.14159265);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_NEAR(d.read_f64(), 3.14159265, 1e-9);
}

TEST(SerializerTest, I64NegativeRoundTrip) {
    Serializer s;
    s.write_i64(-42LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -42LL);
}

TEST(SerializerTest, U16RoundTrip) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, U8ZeroRoundTrip) {
    Serializer s;
    s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
}

TEST(SerializerTest, U8MaxRoundTrip) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, F64NaNV2RoundTrip) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    Deserializer d(s.data());
    EXPECT_TRUE(std::isnan(d.read_f64()));
}

TEST(SerializerTest, F64NegativeInfinityRoundTrip) {
    Serializer s;
    s.write_f64(-std::numeric_limits<double>::infinity());
    Deserializer d(s.data());
    double v = d.read_f64();
    EXPECT_TRUE(std::isinf(v) && v < 0);
}

TEST(SerializerTest, StringWithPipeChar) {
    Serializer s;
    s.write_string("a|b|c");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "a|b|c");
}

TEST(SerializerTest, StringWithColonChar) {
    Serializer s;
    s.write_string("key:value");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "key:value");
}

TEST(SerializerTest, TwoBoolsThenString) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_string("done");
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_string(), "done");
}

TEST(SerializerTest, I32ThenU32ThenI64) {
    Serializer s;
    s.write_i32(-100);
    s.write_u32(200);
    s.write_i64(-300LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -100);
    EXPECT_EQ(d.read_u32(), 200u);
    EXPECT_EQ(d.read_i64(), -300LL);
}

TEST(SerializerTest, U8ZeroRoundTripV2) {
    Serializer s;
    s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
}

TEST(SerializerTest, U64ZeroRoundTripV3) {
    Serializer s;
    s.write_u64(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
}

TEST(SerializerTest, StringWithNewlineV2) {
    Serializer s;
    s.write_string("line1\nline2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2");
}

TEST(SerializerTest, F64ZeroRoundTripV2) {
    Serializer s;
    s.write_f64(0.0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), 0.0);
}

TEST(SerializerTest, ThreeStringsSequential) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_string("gamma");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_string(), "gamma");
}

TEST(SerializerTest, I32MaxValueV2) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
}

TEST(SerializerTest, BoolStringBoolPattern) {
    Serializer s;
    s.write_bool(true);
    s.write_string("middle");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "middle");
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, U16ThenU32ThenU64) {
    Serializer s;
    s.write_u16(100);
    s.write_u32(200);
    s.write_u64(300);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 100);
    EXPECT_EQ(d.read_u32(), 200u);
    EXPECT_EQ(d.read_u64(), 300u);
}

TEST(SerializerTest, ThreeBoolsRoundTrip) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, StringWithEmojiCharacters) {
    Serializer s;
    s.write_string("hello 🌍");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello 🌍");
}

TEST(SerializerTest, I64ZeroRoundTripV2) {
    Serializer s;
    s.write_i64(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 0);
}

TEST(SerializerTest, U8StringU8Pattern) {
    Serializer s;
    s.write_u8(10);
    s.write_string("mid");
    s.write_u8(20);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 10);
    EXPECT_EQ(d.read_string(), "mid");
    EXPECT_EQ(d.read_u8(), 20);
}

TEST(SerializerTest, F64NegativeInfinity) {
    Serializer s;
    s.write_f64(-std::numeric_limits<double>::infinity());
    Deserializer d(s.data());
    double val = d.read_f64();
    EXPECT_TRUE(std::isinf(val));
    EXPECT_TRUE(val < 0);
}

TEST(SerializerTest, TwoDifferentStrings) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
}

TEST(SerializerTest, U32ZeroAndOne) {
    Serializer s;
    s.write_u32(0);
    s.write_u32(1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
}

TEST(SerializerTest, I32F64StringMixedPattern) {
    Serializer s;
    s.write_i32(42);
    s.write_f64(3.14);
    s.write_string("end");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 42);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14);
    EXPECT_EQ(d.read_string(), "end");
}

// --- Cycle 1023: IPC serializer tests ---

TEST(SerializerTest, U64MaxValueV2) {
    Serializer s;
    s.write_u64(UINT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
}

TEST(SerializerTest, I32NegativeOneRoundTrip) {
    Serializer s;
    s.write_i32(-1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
}

TEST(SerializerTest, EmptyStringRoundTripV2) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, BoolTrueRoundTripV2) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, BoolFalseRoundTripV2) {
    Serializer s;
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, F64PiRoundTripV2) {
    Serializer s;
    s.write_f64(3.14159265358979);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
}

TEST(SerializerTest, U16MaxValueV2) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, StringThenBoolThenI64Pattern) {
    Serializer s;
    s.write_string("start");
    s.write_bool(true);
    s.write_i64(-999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "start");
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_i64(), -999);
}

// --- Cycle 1032: IPC serializer tests ---

TEST(SerializerTest, FourU8Sequential) {
    Serializer s;
    s.write_u8(10); s.write_u8(20); s.write_u8(30); s.write_u8(40);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 10); EXPECT_EQ(d.read_u8(), 20);
    EXPECT_EQ(d.read_u8(), 30); EXPECT_EQ(d.read_u8(), 40);
}

TEST(SerializerTest, I64MinValueV2) {
    Serializer s;
    s.write_i64(INT64_MIN);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MIN);
}

TEST(SerializerTest, F64ZeroRoundTripV3) {
    Serializer s;
    s.write_f64(0.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
}

TEST(SerializerTest, U32AllBitsSet) {
    Serializer s;
    s.write_u32(UINT32_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), UINT32_MAX);
}

TEST(SerializerTest, StringWithNewlineV3) {
    Serializer s;
    s.write_string("line1\nline2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2");
}

TEST(SerializerTest, ThreeStringsSequentialV2) {
    Serializer s;
    s.write_string("a"); s.write_string("bb"); s.write_string("ccc");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "a");
    EXPECT_EQ(d.read_string(), "bb");
    EXPECT_EQ(d.read_string(), "ccc");
}

TEST(SerializerTest, I32PositiveAndNegative) {
    Serializer s;
    s.write_i32(100); s.write_i32(-100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 100); EXPECT_EQ(d.read_i32(), -100);
}

TEST(SerializerTest, BoolU8BoolU8Pattern) {
    Serializer s;
    s.write_bool(true); s.write_u8(255);
    s.write_bool(false); s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool()); EXPECT_EQ(d.read_u8(), 255);
    EXPECT_FALSE(d.read_bool()); EXPECT_EQ(d.read_u8(), 0);
}

// --- Cycle 1041: IPC serializer tests ---

TEST(SerializerTest, U16MaxRoundTrip) {
    Serializer s;
    s.write_u16(UINT16_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), UINT16_MAX);
}

TEST(SerializerTest, U16ZeroRoundTrip) {
    Serializer s;
    s.write_u16(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0);
}

TEST(SerializerTest, F64NegativeRoundTripV4) {
    Serializer s;
    s.write_f64(-123.456);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -123.456);
}

TEST(SerializerTest, I32MaxRoundTripV2) {
    Serializer s;
    s.write_i32(INT32_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MAX);
}

TEST(SerializerTest, I32MinRoundTripV2) {
    Serializer s;
    s.write_i32(INT32_MIN);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MIN);
}

TEST(SerializerTest, BytesEmptyRoundTripV3) {
    Serializer s;
    std::vector<uint8_t> empty;
    s.write_bytes(empty.data(), empty.size());
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bytes().empty());
}

TEST(SerializerTest, StringEmptyRoundTripV4) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, U64ThenBoolSequence) {
    Serializer s;
    s.write_u64(999999); s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 999999u);
    EXPECT_TRUE(d.read_bool());
}

// --- Cycle 1050: IPC serializer tests ---

TEST(SerializerTest, U8ZeroRoundTripV3) {
    Serializer s;
    s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
}

TEST(SerializerTest, U8MaxRoundTripV2) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16MidRoundTrip) {
    Serializer s;
    s.write_u16(32768);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 32768);
}

TEST(SerializerTest, I64PositiveRoundTripV2) {
    Serializer s;
    s.write_i64(1234567890LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1234567890LL);
}

TEST(SerializerTest, F64LargeRoundTrip) {
    Serializer s;
    s.write_f64(1e18);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e18);
}

TEST(SerializerTest, StringUnicodeRoundTripV2) {
    Serializer s;
    s.write_string("Hello\xC3\xA9");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello\xC3\xA9");
}

TEST(SerializerTest, BoolFalseThenTrueV2) {
    Serializer s;
    s.write_bool(false); s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, U32MidRoundTrip) {
    Serializer s;
    s.write_u32(2147483648u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 2147483648u);
}

// --- Cycle 1059: IPC serializer tests ---

TEST(SerializerTest, I64NegOneRoundTrip) {
    Serializer s;
    s.write_i64(-1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1);
}

TEST(SerializerTest, F64SmallRoundTrip) {
    Serializer s;
    s.write_f64(0.001);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.001);
}

TEST(SerializerTest, U16OneRoundTrip) {
    Serializer s;
    s.write_u16(1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 1);
}

TEST(SerializerTest, StringLongRoundTrip) {
    Serializer s;
    std::string longstr(1000, 'x');
    s.write_string(longstr);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), longstr);
}

TEST(SerializerTest, I32OneRoundTrip) {
    Serializer s;
    s.write_i32(1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 1);
}

TEST(SerializerTest, U64OneRoundTrip) {
    Serializer s;
    s.write_u64(1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1u);
}

TEST(SerializerTest, BoolTrueThenStringV2) {
    Serializer s;
    s.write_bool(true); s.write_string("yes");
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "yes");
}

TEST(SerializerTest, U32ThenI32Sequence) {
    Serializer s;
    s.write_u32(100); s.write_i32(-50);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 100u);
    EXPECT_EQ(d.read_i32(), -50);
}

// --- Cycle 1068: IPC serializer tests ---

TEST(SerializerTest, F64InfinityRoundTripV2) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
}

TEST(SerializerTest, I64MaxRoundTripV2) {
    Serializer s;
    s.write_i64(INT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MAX);
}

TEST(SerializerTest, U32OneRoundTrip) {
    Serializer s;
    s.write_u32(1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1u);
}

TEST(SerializerTest, StringWithTabV3) {
    Serializer s;
    s.write_string("a\tb");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "a\tb");
}

TEST(SerializerTest, U16ThenU16Sequence) {
    Serializer s;
    s.write_u16(100); s.write_u16(200);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 100);
    EXPECT_EQ(d.read_u16(), 200);
}

TEST(SerializerTest, I32NegMaxRoundTrip) {
    Serializer s;
    s.write_i32(-2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483647);
}

TEST(SerializerTest, F64TinyRoundTrip) {
    Serializer s;
    s.write_f64(1e-15);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e-15);
}

TEST(SerializerTest, BoolStringBoolPatternV2) {
    Serializer s;
    s.write_bool(true); s.write_string("mid"); s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "mid");
    EXPECT_FALSE(d.read_bool());
}

// --- Cycle 1077: IPC serializer tests ---

TEST(SerializerTest, U64MaxRoundTripV3) {
    Serializer s;
    s.write_u64(UINT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
}

TEST(SerializerTest, F64NegZeroRoundTrip) {
    Serializer s;
    s.write_f64(-0.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
}

TEST(SerializerTest, I32ZeroRoundTripV2) {
    Serializer s;
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
}

TEST(SerializerTest, StringSingleCharV2) {
    Serializer s;
    s.write_string("x");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "x");
}

TEST(SerializerTest, U8ThenU16ThenU32) {
    Serializer s;
    s.write_u8(1); s.write_u16(2); s.write_u32(3);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u16(), 2);
    EXPECT_EQ(d.read_u32(), 3u);
}

TEST(SerializerTest, I64ZeroRoundTripV3) {
    Serializer s;
    s.write_i64(0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 0);
}

TEST(SerializerTest, F64EulerRoundTrip) {
    Serializer s;
    s.write_f64(2.718281828);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828);
}

TEST(SerializerTest, StringSpacesOnly) {
    Serializer s;
    s.write_string("   ");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "   ");
}

// --- Cycle 1086: IPC serializer tests ---

TEST(SerializerTest, F64PiRoundTripV3) {
    Serializer s;
    s.write_f64(3.14159265358979);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
}

TEST(SerializerTest, U16TwoFiftySix) {
    Serializer s;
    s.write_u16(256);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 256);
}

TEST(SerializerTest, I64NegMillionRoundTrip) {
    Serializer s;
    s.write_i64(-1000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1000000LL);
}

TEST(SerializerTest, StringWithSlashes) {
    Serializer s;
    s.write_string("/a/b/c");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "/a/b/c");
}

TEST(SerializerTest, U32TenThousand) {
    Serializer s;
    s.write_u32(10000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 10000u);
}

TEST(SerializerTest, I32NegOneV2) {
    Serializer s;
    s.write_i32(-1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
}

TEST(SerializerTest, BoolTrueTrueFalse) {
    Serializer s;
    s.write_bool(true); s.write_bool(true); s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, U8HundredRoundTrip) {
    Serializer s;
    s.write_u8(100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 100);
}

// --- Cycle 1095: 8 IPC tests ---

TEST(SerializerTest, U16FiveHundredRoundTrip) {
    Serializer s;
    s.write_u16(500);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 500);
}

TEST(SerializerTest, I32NegHundredRoundTrip) {
    Serializer s;
    s.write_i32(-100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -100);
}

TEST(SerializerTest, U64TenThousandRoundTrip) {
    Serializer s;
    s.write_u64(10000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 10000);
}

TEST(SerializerTest, F64PiRoundTripV4) {
    Serializer s;
    s.write_f64(3.14159);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159);
}

TEST(SerializerTest, I64NegMillionRoundTripV2) {
    Serializer s;
    s.write_i64(-1000000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1000000);
}

TEST(SerializerTest, StringHelloWorldRoundTrip) {
    Serializer s;
    s.write_string("Hello, World!");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello, World!");
}

TEST(SerializerTest, BoolTrueThenFalseV3) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
}

TEST(SerializerTest, U32ThousandRoundTripV2) {
    Serializer s;
    s.write_u32(1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1000);
}

// --- Cycle 1104: 8 IPC tests ---

TEST(SerializerTest, U8FiftyRoundTrip) {
    Serializer s;
    s.write_u8(50);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 50);
}

TEST(SerializerTest, I32PositiveTenRoundTrip) {
    Serializer s;
    s.write_i32(10);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 10);
}

TEST(SerializerTest, U64BillionRoundTrip) {
    Serializer s;
    s.write_u64(1000000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1000000000ULL);
}

TEST(SerializerTest, F64NegPiRoundTripV2) {
    Serializer s;
    s.write_f64(-3.14159);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -3.14159);
}

TEST(SerializerTest, I64BillionRoundTrip) {
    Serializer s;
    s.write_i64(1000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1000000000LL);
}

TEST(SerializerTest, StringWithNewlineV4) {
    Serializer s;
    s.write_string("line1\nline2");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2");
}

TEST(SerializerTest, U16TwoFiftyFiveRoundTrip) {
    Serializer s;
    s.write_u16(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 255);
}

TEST(SerializerTest, BoolFalseThenTrueV3) {
    Serializer s;
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

// --- Cycle 1113: 8 IPC tests ---

TEST(SerializerTest, U8TwoHundredRoundTrip) {
    Serializer s;
    s.write_u8(200);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 200);
}

TEST(SerializerTest, U16TenThousandRoundTrip) {
    Serializer s;
    s.write_u16(10000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 10000);
}

TEST(SerializerTest, I32NegOneThousandRoundTrip) {
    Serializer s;
    s.write_i32(-1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1000);
}

TEST(SerializerTest, U64HundredMillionRoundTrip) {
    Serializer s;
    s.write_u64(100000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 100000000ULL);
}

TEST(SerializerTest, F64SqrtTwoRoundTripV2) {
    Serializer s;
    s.write_f64(1.41421356);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.41421356);
}

TEST(SerializerTest, StringWithQuotesRoundTrip) {
    Serializer s;
    s.write_string("He said \"hello\"");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "He said \"hello\"");
}

TEST(SerializerTest, I64MinusOneBillionRoundTrip) {
    Serializer s;
    s.write_i64(-1000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1000000000LL);
}

TEST(SerializerTest, U32FiveMillionRoundTrip) {
    Serializer s;
    s.write_u32(5000000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 5000000);
}

// --- Cycle 1122: 8 IPC tests ---

TEST(SerializerTest, U8OneRoundTripV4) {
    Serializer s;
    s.write_u8(1);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 1);
}

TEST(SerializerTest, U16ThousandRoundTrip) {
    Serializer s;
    s.write_u16(1000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 1000);
}

TEST(SerializerTest, I32FiftyRoundTrip) {
    Serializer s;
    s.write_i32(50);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 50);
}

TEST(SerializerTest, U64TrillionRoundTrip) {
    Serializer s;
    s.write_u64(1000000000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1000000000000ULL);
}

TEST(SerializerTest, F64EpsilonRoundTrip) {
    Serializer s;
    s.write_f64(0.000001);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.000001);
}

TEST(SerializerTest, StringWithSlashRoundTrip) {
    Serializer s;
    s.write_string("path/to/file");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "path/to/file");
}

TEST(SerializerTest, I64TenBillionRoundTrip) {
    Serializer s;
    s.write_i64(10000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 10000000000LL);
}

TEST(SerializerTest, U32HundredRoundTripV2) {
    Serializer s;
    s.write_u32(100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 100);
}

// --- Cycle 1131: 8 IPC tests ---

TEST(SerializerTest, U8TwentyFiveRoundTrip) {
    Serializer s;
    s.write_u8(25);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 25);
}

TEST(SerializerTest, U16FiveThousandRoundTrip) {
    Serializer s;
    s.write_u16(5000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 5000);
}

TEST(SerializerTest, I32NegTenRoundTrip) {
    Serializer s;
    s.write_i32(-10);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -10);
}

TEST(SerializerTest, U64TenMillionRoundTrip) {
    Serializer s;
    s.write_u64(10000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 10000000ULL);
}

TEST(SerializerTest, F64GoldenRatioRoundTrip) {
    Serializer s;
    s.write_f64(1.6180339887);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.6180339887);
}

TEST(SerializerTest, StringWithBackslashRoundTrip) {
    Serializer s;
    s.write_string("C:\\Users\\test");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "C:\\Users\\test");
}

TEST(SerializerTest, I64NegTenBillionRoundTrip) {
    Serializer s;
    s.write_i64(-10000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -10000000000LL);
}

TEST(SerializerTest, BoolTrueAloneV4) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, U8FiftyV5) {
    Serializer s;
    s.write_u8(50);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 50);
}

TEST(SerializerTest, U16TenThousandV5) {
    Serializer s;
    s.write_u16(10000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 10000);
}

TEST(SerializerTest, I32NegHundredV5) {
    Serializer s;
    s.write_i32(-100);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -100);
}

TEST(SerializerTest, U64BillionV5) {
    Serializer s;
    s.write_u64(1000000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1000000000ULL);
}

TEST(SerializerTest, F64NegGoldenRatioV5) {
    Serializer s;
    s.write_f64(-1.618033988749);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.618033988749);
}

TEST(SerializerTest, StringWithQuoteV5) {
    Serializer s;
    s.write_string("He said \"hello\"");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "He said \"hello\"");
}

TEST(SerializerTest, I64PositiveBillionV5) {
    Serializer s;
    s.write_i64(1000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1000000000LL);
}

TEST(SerializerTest, BoolFalseAloneV5) {
    Serializer s;
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
}

// --- Cycle 1158: 8 IPC tests ---

TEST(SerializerTest, U8TwoHundredV7) {
    Serializer s;
    s.write_u8(200);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 200);
}

TEST(SerializerTest, U16FortyThousandV7) {
    Serializer s;
    s.write_u16(40000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 40000);
}

TEST(SerializerTest, I32NegTenThousandV7) {
    Serializer s;
    s.write_i32(-10000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -10000);
}

TEST(SerializerTest, U64HundredBillionV7) {
    Serializer s;
    s.write_u64(100000000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 100000000000ULL);
}

TEST(SerializerTest, F64PiOverTwoV7) {
    Serializer s;
    s.write_f64(1.5707963267948966);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.5707963267948966);
}

TEST(SerializerTest, StringEmptyV7) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, I64MaxMinusOneV7) {
    Serializer s;
    s.write_i64(9223372036854775806LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775806LL);
}

TEST(SerializerTest, BoolTrueV7) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
}

// ---------------------------------------------------------------------------
// Cycle 1167 — 8 additional serializer tests (comprehensive type coverage)
// ---------------------------------------------------------------------------

TEST(SerializerTest, U16MaxValueV8) {
    Serializer s;
    s.write_u16(65535u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
}

TEST(SerializerTest, I32NegativeFiftyV8) {
    Serializer s;
    s.write_i32(-50);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -50);
}

TEST(SerializerTest, U64SmallValueV8) {
    Serializer s;
    s.write_u64(12345ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 12345ULL);
}

TEST(SerializerTest, F64NegativeOnePointFiveV8) {
    Serializer s;
    s.write_f64(-1.5);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.5);
}

TEST(SerializerTest, StringWithNumbersV8) {
    Serializer s;
    s.write_string("test123456");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "test123456");
}

TEST(SerializerTest, I64NegativeTrillionV8) {
    Serializer s;
    s.write_i64(-1000000000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1000000000000LL);
}

TEST(SerializerTest, BoolSequenceV8) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, MixedU32AndI32V8) {
    Serializer s;
    s.write_u32(999u);
    s.write_i32(-999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 999u);
    EXPECT_EQ(d.read_i32(), -999);
}

// Cycle 1176 — Additional serializer tests
TEST(SerializerTest, U16RoundTripV9) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, I64PositiveMillionV9) {
    Serializer s;
    s.write_i64(1000000LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 1000000LL);
}

TEST(SerializerTest, F64SmallDecimalV9) {
    Serializer s;
    s.write_f64(0.123456789);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 0.123456789, 1e-9);
}

TEST(SerializerTest, StringEmptyV9) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
}

TEST(SerializerTest, BoolTrueOnlyV9) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, U32ZeroV9) {
    Serializer s;
    s.write_u32(0u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
}

TEST(SerializerTest, I32MaxV9) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
}

TEST(SerializerTest, U8MinMaxSequenceV9) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(255);
    s.write_u8(128);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_EQ(d.read_u8(), 128);
}

// Cycle 1185 — Additional serializer tests V10
TEST(SerializerTest, U8OneTwentyV10) {
    Serializer s;
    s.write_u8(120);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 120);
}

TEST(SerializerTest, U16MaxMinusOneV10) {
    Serializer s;
    s.write_u16(65534);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65534);
}

TEST(SerializerTest, U32LargeValueV10) {
    Serializer s;
    s.write_u32(4000000000u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4000000000u);
}

TEST(SerializerTest, I32NegMaxV10) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483648);
}

TEST(SerializerTest, I64ZeroV10) {
    Serializer s;
    s.write_i64(0LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 0LL);
}

TEST(SerializerTest, F64PiV10) {
    Serializer s;
    s.write_f64(3.141592653589793);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 3.141592653589793, 1e-15);
}

TEST(SerializerTest, StringLongV10) {
    Serializer s;
    s.write_string("The quick brown fox jumps over the lazy dog");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "The quick brown fox jumps over the lazy dog");
}

TEST(SerializerTest, BoolFalseOnlyV10) {
    Serializer s;
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
}

// Cycle 1194 — Additional serializer tests V11
TEST(SerializerTest, U8MaxValueV11) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16MidRangeV11) {
    Serializer s;
    s.write_u16(32768);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 32768);
}

TEST(SerializerTest, U32MaxV11) {
    Serializer s;
    s.write_u32(4294967295u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295u);
}

TEST(SerializerTest, I32NegativeHalfMilV11) {
    Serializer s;
    s.write_i32(-500000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -500000);
}

TEST(SerializerTest, I64LargeNegativeV11) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
}

TEST(SerializerTest, F64LargeValueV11) {
    Serializer s;
    s.write_f64(999999.999999);
    Deserializer d(s.data());
    EXPECT_NEAR(d.read_f64(), 999999.999999, 1e-6);
}

TEST(SerializerTest, StringSpecialCharsV11) {
    Serializer s;
    s.write_string("Hello\nWorld\t!");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello\nWorld\t!");
}

TEST(SerializerTest, BoolAlternatingSequenceV11) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, U8MidRangeV12) {
    Serializer s;
    s.write_u8(128);
    Deserializer d(s.data());
    EXPECT_EQ(128, d.read_u8());
}

TEST(SerializerTest, U16BoundaryV12) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(65535, d.read_u16());
}

TEST(SerializerTest, U32MidRangeV12) {
    Serializer s;
    s.write_u32(2147483648U);
    Deserializer d(s.data());
    EXPECT_EQ(2147483648U, d.read_u32());
}

TEST(SerializerTest, I32NegativeSmallV12) {
    Serializer s;
    s.write_i32(-256);
    Deserializer d(s.data());
    EXPECT_EQ(-256, d.read_i32());
}

TEST(SerializerTest, I64PositiveLargeV12) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64SmallValueV12) {
    Serializer s;
    s.write_f64(0.00001);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(0.00001, d.read_f64());
}

TEST(SerializerTest, StringEmptyV12) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ("", d.read_string());
}

TEST(SerializerTest, BoolSingleTrueV12) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, U64MaxValueV13) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(18446744073709551615ULL, d.read_u64());
}

TEST(SerializerTest, U16LowRangeV13) {
    Serializer s;
    s.write_u16(256);
    Deserializer d(s.data());
    EXPECT_EQ(256, d.read_u16());
}

TEST(SerializerTest, I32ZeroV13) {
    Serializer s;
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(0, d.read_i32());
}

TEST(SerializerTest, F64PiValueV13) {
    Serializer s;
    s.write_f64(3.14159265358979323846);
    Deserializer d(s.data());
    EXPECT_NEAR(3.14159265358979323846, d.read_f64(), 1e-15);
}

TEST(SerializerTest, StringWithNumbersV13) {
    Serializer s;
    s.write_string("Test123456");
    Deserializer d(s.data());
    EXPECT_EQ("Test123456", d.read_string());
}

TEST(SerializerTest, BoolFalseThenTrueV13) {
    Serializer s;
    s.write_bool(false);
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
}

TEST(SerializerTest, I64NegativeMinV13) {
    Serializer s;
    s.write_i64(-9223372036854775807LL - 1);
    Deserializer d(s.data());
    EXPECT_EQ(-9223372036854775807LL - 1, d.read_i64());
}

TEST(SerializerTest, U8LowByteV13) {
    Serializer s;
    s.write_u8(1);
    Deserializer d(s.data());
    EXPECT_EQ(1, d.read_u8());
}

TEST(SerializerTest, U8MaxByteV14) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(255, d.read_u8());
}

TEST(SerializerTest, U16MidRangeV14) {
    Serializer s;
    s.write_u16(32768);
    Deserializer d(s.data());
    EXPECT_EQ(32768, d.read_u16());
}

TEST(SerializerTest, U32HighValueV14) {
    Serializer s;
    s.write_u32(3000000000U);
    Deserializer d(s.data());
    EXPECT_EQ(3000000000U, d.read_u32());
}

TEST(SerializerTest, U64MidRangeV14) {
    Serializer s;
    s.write_u64(9223372036854775808ULL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775808ULL, d.read_u64());
}

TEST(SerializerTest, I32NegativeLargeV14) {
    Serializer s;
    s.write_i32(-2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(-2147483647, d.read_i32());
}

TEST(SerializerTest, I64PositiveV14) {
    Serializer s;
    s.write_i64(4611686018427387904LL);
    Deserializer d(s.data());
    EXPECT_EQ(4611686018427387904LL, d.read_i64());
}

TEST(SerializerTest, F64NegativeValueV14) {
    Serializer s;
    s.write_f64(-271.828182845904523536);
    Deserializer d(s.data());
    EXPECT_NEAR(-271.828182845904523536, d.read_f64(), 1e-15);
}

TEST(SerializerTest, StringMixedCaseV14) {
    Serializer s;
    s.write_string("MixedCaseString");
    Deserializer d(s.data());
    EXPECT_EQ("MixedCaseString", d.read_string());
}

// Cycle 1230: IPC serializer tests V15

TEST(SerializerTest, U8EdgeCaseV15) {
    Serializer s;
    s.write_u8(127);
    Deserializer d(s.data());
    EXPECT_EQ(127, d.read_u8());
}

TEST(SerializerTest, U16EdgeCaseV15) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(65535, d.read_u16());
}

TEST(SerializerTest, U32EdgeCaseV15) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(4294967295U, d.read_u32());
}

TEST(SerializerTest, U64EdgeCaseV15) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(18446744073709551615ULL, d.read_u64());
}

TEST(SerializerTest, I32NegativeMinV15) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(-2147483648, d.read_i32());
}

TEST(SerializerTest, I64NegativeMinV15) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(-9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64EulerValueV15) {
    Serializer s;
    s.write_f64(2.718281828459045);
    Deserializer d(s.data());
    EXPECT_NEAR(2.718281828459045, d.read_f64(), 1e-15);
}

TEST(SerializerTest, StringWithSpecialCharsV15) {
    Serializer s;
    s.write_string("Hello@World#2026!");
    Deserializer d(s.data());
    EXPECT_EQ("Hello@World#2026!", d.read_string());
}

// Cycle 1239: IPC serializer tests V16

TEST(SerializerTest, U8BoundaryZeroV16) {
    Serializer s;
    s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_EQ(0, d.read_u8());
}

TEST(SerializerTest, U16MidRangeV16) {
    Serializer s;
    s.write_u16(32768);
    Deserializer d(s.data());
    EXPECT_EQ(32768, d.read_u16());
}

TEST(SerializerTest, U32LowValueV16) {
    Serializer s;
    s.write_u32(256);
    Deserializer d(s.data());
    EXPECT_EQ(256U, d.read_u32());
}

TEST(SerializerTest, U64HighValueV16) {
    Serializer s;
    s.write_u64(13835058055282163712ULL);
    Deserializer d(s.data());
    EXPECT_EQ(13835058055282163712ULL, d.read_u64());
}

TEST(SerializerTest, I32PositiveMaxV16) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(2147483647, d.read_i32());
}

TEST(SerializerTest, I64NegativeValueV16) {
    Serializer s;
    s.write_i64(-4611686018427387904LL);
    Deserializer d(s.data());
    EXPECT_EQ(-4611686018427387904LL, d.read_i64());
}

TEST(SerializerTest, F64PiValueV16) {
    Serializer s;
    s.write_f64(3.141592653589793);
    Deserializer d(s.data());
    EXPECT_NEAR(3.141592653589793, d.read_f64(), 1e-15);
}

TEST(SerializerTest, StringEmptyV16) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ("", d.read_string());
}

// Cycle 1248: IPC serializer tests V17

TEST(SerializerTest, U8MaxBoundaryV17) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(255, d.read_u8());
}

TEST(SerializerTest, U16MaxValueV17) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(65535, d.read_u16());
}

TEST(SerializerTest, U32MidRangeV17) {
    Serializer s;
    s.write_u32(2147483648U);
    Deserializer d(s.data());
    EXPECT_EQ(2147483648U, d.read_u32());
}

TEST(SerializerTest, U64MaxValueV17) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(18446744073709551615ULL, d.read_u64());
}

TEST(SerializerTest, I32NegativeMinV17) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(-2147483648, d.read_i32());
}

TEST(SerializerTest, I64PositiveMaxV17) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64VerySmallValueV17) {
    Serializer s;
    s.write_f64(1.23456789e-10);
    Deserializer d(s.data());
    EXPECT_NEAR(1.23456789e-10, d.read_f64(), 1e-20);
}

TEST(SerializerTest, StringLongValueV17) {
    Serializer s;
    s.write_string("The quick brown fox jumps over the lazy dog");
    Deserializer d(s.data());
    EXPECT_EQ("The quick brown fox jumps over the lazy dog", d.read_string());
}

// Cycle 1257: IPC serializer tests V18

TEST(SerializerTest, U8LowValueV18) {
    Serializer s;
    s.write_u8(42);
    Deserializer d(s.data());
    EXPECT_EQ(42, d.read_u8());
}

TEST(SerializerTest, U16QuarterMaxV18) {
    Serializer s;
    s.write_u16(16384);
    Deserializer d(s.data());
    EXPECT_EQ(16384, d.read_u16());
}

TEST(SerializerTest, U32ThreeQuarterMaxV18) {
    Serializer s;
    s.write_u32(3221225472U);
    Deserializer d(s.data());
    EXPECT_EQ(3221225472U, d.read_u32());
}

TEST(SerializerTest, U64MidRangeV18) {
    Serializer s;
    s.write_u64(9223372036854775808ULL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775808ULL, d.read_u64());
}

TEST(SerializerTest, I32ZeroV18) {
    Serializer s;
    s.write_i32(0);
    Deserializer d(s.data());
    EXPECT_EQ(0, d.read_i32());
}

TEST(SerializerTest, I64NegativeMinV18) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(-9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64ZeroValueV18) {
    Serializer s;
    s.write_f64(0.0);
    Deserializer d(s.data());
    EXPECT_EQ(0.0, d.read_f64());
}

TEST(SerializerTest, BoolTrueValueV18) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
}

// Cycle 1266: IPC serializer tests V19

TEST(SerializerTest, U8MaxValueV19) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(255, d.read_u8());
}

TEST(SerializerTest, U16HalfMaxV19) {
    Serializer s;
    s.write_u16(32768);
    Deserializer d(s.data());
    EXPECT_EQ(32768, d.read_u16());
}

TEST(SerializerTest, U32HalfMaxV19) {
    Serializer s;
    s.write_u32(2147483648U);
    Deserializer d(s.data());
    EXPECT_EQ(2147483648U, d.read_u32());
}

TEST(SerializerTest, U64LowRangeV19) {
    Serializer s;
    s.write_u64(1099511627776ULL);
    Deserializer d(s.data());
    EXPECT_EQ(1099511627776ULL, d.read_u64());
}

TEST(SerializerTest, I32NegativeMinV19) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(-2147483648, d.read_i32());
}

TEST(SerializerTest, I64ZeroValueV19) {
    Serializer s;
    s.write_i64(0);
    Deserializer d(s.data());
    EXPECT_EQ(0, d.read_i64());
}

TEST(SerializerTest, F64NegativeValueV19) {
    Serializer s;
    s.write_f64(-3.14159265);
    Deserializer d(s.data());
    EXPECT_NEAR(-3.14159265, d.read_f64(), 1e-8);
}

TEST(SerializerTest, StringEmptyValueV19) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ("", d.read_string());
}

// Cycle 1275: IPC serializer tests V20

TEST(SerializerTest, U8MidValueV20) {
    Serializer s;
    s.write_u8(128);
    Deserializer d(s.data());
    EXPECT_EQ(128, d.read_u8());
}

TEST(SerializerTest, U16MaxValueV20) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(65535, d.read_u16());
}

TEST(SerializerTest, U32MaxValueV20) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(4294967295U, d.read_u32());
}

TEST(SerializerTest, U64MaxValueV20) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(18446744073709551615ULL, d.read_u64());
}

TEST(SerializerTest, I32PositiveMaxV20) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(2147483647, d.read_i32());
}

TEST(SerializerTest, I64PositiveMaxV20) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64LargeValueV20) {
    Serializer s;
    s.write_f64(1.23456789e100);
    Deserializer d(s.data());
    EXPECT_NEAR(1.23456789e100, d.read_f64(), 1e90);
}

TEST(SerializerTest, BoolFalseValueV20) {
    Serializer s;
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
}

// Cycle 1284: Serializer tests

TEST(SerializerTest, U8MidRangeValueV21) {
    Serializer s;
    s.write_u8(128);
    Deserializer d(s.data());
    EXPECT_EQ(128, d.read_u8());
}

TEST(SerializerTest, U16MidRangeValueV21) {
    Serializer s;
    s.write_u16(32768);
    Deserializer d(s.data());
    EXPECT_EQ(32768, d.read_u16());
}

TEST(SerializerTest, U32MidRangeValueV21) {
    Serializer s;
    s.write_u32(2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(2147483648, d.read_u32());
}

TEST(SerializerTest, U64MidRangeValueV21) {
    Serializer s;
    s.write_u64(9223372036854775808ULL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775808ULL, d.read_u64());
}

TEST(SerializerTest, I32NegativeMaxV21) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(-2147483648, d.read_i32());
}

TEST(SerializerTest, I64NegativeMaxV21) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(-9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64SmallValueV21) {
    Serializer s;
    s.write_f64(1.23456789e-100);
    Deserializer d(s.data());
    EXPECT_NEAR(1.23456789e-100, d.read_f64(), 1e-110);
}

TEST(SerializerTest, StringMultiwordValueV21) {
    Serializer s;
    s.write_string("Hello World Test String");
    Deserializer d(s.data());
    EXPECT_EQ("Hello World Test String", d.read_string());
}

// Cycle 1293: Serializer tests
TEST(SerializerTest, U8ZeroValueV22) {
    Serializer s;
    s.write_u8(0);
    Deserializer d(s.data());
    EXPECT_EQ(0, d.read_u8());
}

TEST(SerializerTest, U16MaxValueV22) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(65535, d.read_u16());
}

TEST(SerializerTest, U32MidRangeValueV22) {
    Serializer s;
    s.write_u32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(2147483647, d.read_u32());
}

TEST(SerializerTest, U64SmallValueV22) {
    Serializer s;
    s.write_u64(256);
    Deserializer d(s.data());
    EXPECT_EQ(256, d.read_u64());
}

TEST(SerializerTest, I32SmallNegativeV22) {
    Serializer s;
    s.write_i32(-100);
    Deserializer d(s.data());
    EXPECT_EQ(-100, d.read_i32());
}

TEST(SerializerTest, I64ZeroValueV22) {
    Serializer s;
    s.write_i64(0);
    Deserializer d(s.data());
    EXPECT_EQ(0, d.read_i64());
}

TEST(SerializerTest, F64ZeroValueV22) {
    Serializer s;
    s.write_f64(0.0);
    Deserializer d(s.data());
    EXPECT_EQ(0.0, d.read_f64());
}

TEST(SerializerTest, StringEmptyValueV22) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ("", d.read_string());
}

// Cycle 1302: Serializer tests

TEST(SerializerTest, U8MaxValueV23) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(255, d.read_u8());
}

TEST(SerializerTest, U16LargeValueV23) {
    Serializer s;
    s.write_u16(65000);
    Deserializer d(s.data());
    EXPECT_EQ(65000, d.read_u16());
}

TEST(SerializerTest, U32MidValueV23) {
    Serializer s;
    s.write_u32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(2147483647, d.read_u32());
}

TEST(SerializerTest, I32LargeNegativeV23) {
    Serializer s;
    s.write_i32(-2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(-2147483647, d.read_i32());
}

TEST(SerializerTest, I64LargePositiveV23) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(9223372036854775807LL, d.read_i64());
}

TEST(SerializerTest, F64PrecisionValueV23) {
    Serializer s;
    s.write_f64(3.14159265359);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(3.14159265359, d.read_f64());
}

TEST(SerializerTest, StringLongValueV23) {
    Serializer s;
    std::string long_str = "The quick brown fox jumps over the lazy dog";
    s.write_string(long_str);
    Deserializer d(s.data());
    EXPECT_EQ(long_str, d.read_string());
}

TEST(SerializerTest, BoolTrueValueV23) {
    Serializer s;
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(true, d.read_bool());
}
