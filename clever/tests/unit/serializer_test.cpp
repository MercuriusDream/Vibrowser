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
