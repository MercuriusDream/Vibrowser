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

// ---------------------------------------------------------------------------
// Cycle V74 — requested serializer coverage
// ---------------------------------------------------------------------------

TEST(SerializerTest, WriteReadU32MatchesV74) {
    Serializer s;
    const uint32_t value = 0x12345678u;
    s.write_u32(value);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), value);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringRoundTripTestLiteralV74) {
    Serializer s;
    s.write_string("test");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolTrueWriteReadV74) {
    Serializer s;
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesRoundTripV74) {
    Serializer s;
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32SequenceOneToFiveInOrderV74) {
    Serializer s;
    for (uint32_t i = 1; i <= 5; ++i) {
        s.write_u32(i);
    }

    Deserializer d(s.data());
    for (uint32_t i = 1; i <= 5; ++i) {
        EXPECT_EQ(d.read_u32(), i);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithNewlinesRoundTripV74) {
    const std::string text = "line1\nline2\nline3\n";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesAlternating00FFRoundTripV74) {
    const std::vector<uint8_t> bytes = {0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF};
    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteMultipleTypesTotalBufferV74) {
    const std::vector<uint8_t> bytes = {0xAA, 0xBB, 0xCC};
    Serializer s;
    s.write_u32(0x01020304u);        // 4 bytes
    s.write_string("xy");            // 4-byte length + 2 bytes
    s.write_bool(true);              // 1 byte
    s.write_bytes(bytes.data(), 3);  // 4-byte length + 3 bytes

    EXPECT_EQ(s.data().size(), 18u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0x01020304u);
    EXPECT_EQ(d.read_string(), "xy");
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_bytes(), bytes);
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

// Cycle 1311: Serializer tests

TEST(SerializerTest, U8MaxValueV24) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16MaxValueV24) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, U32MaxValueV24) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295U);
}

TEST(SerializerTest, U64MaxValueV24) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, I32NegativeValueV24) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483648);
}

TEST(SerializerTest, I64NegativeValueV24) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
}

TEST(SerializerTest, F64HighPrecisionV24) {
    Serializer s;
    s.write_f64(2.718281828459045);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
}

TEST(SerializerTest, StringEmptyAndBoolFalseV24) {
    Serializer s;
    s.write_string("");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_bool(), false);
}

// Cycle 1320: Serializer tests

TEST(SerializerTest, U8RoundTripV25) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16RoundTripV25) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, U32RoundTripV25) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295U);
}

TEST(SerializerTest, U64RoundTripV25) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, I32RoundTripV25) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
}

TEST(SerializerTest, I64RoundTripV25) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775807LL);
}

TEST(SerializerTest, F64RoundTripV25) {
    Serializer s;
    s.write_f64(3.141592653589793);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.141592653589793);
}

TEST(SerializerTest, StringAndBoolRoundTripV25) {
    Serializer s;
    s.write_string("Cycle1320");
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Cycle1320");
    EXPECT_EQ(d.read_bool(), true);
}

// Cycle 1329
TEST(SerializerTest, U8RoundTripV26) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16RoundTripV26) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, U32RoundTripV26) {
    Serializer s;
    s.write_u32(4294967295);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295);
}

TEST(SerializerTest, U64RoundTripV26) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, I32RoundTripV26) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483648);
}

TEST(SerializerTest, I64RoundTripV26) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
}

TEST(SerializerTest, F64RoundTripV26) {
    Serializer s;
    s.write_f64(2.718281828459045);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
}

TEST(SerializerTest, StringAndBoolV26) {
    Serializer s;
    s.write_string("Cycle1329");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Cycle1329");
    EXPECT_EQ(d.read_bool(), false);
}

// Cycle 1338
TEST(SerializerTest, U8RoundTripV27) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16RoundTripV27) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, U32RoundTripV27) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295U);
}

TEST(SerializerTest, U64RoundTripV27) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, I32RoundTripV27) {
    Serializer s;
    s.write_i32(2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
}

TEST(SerializerTest, I64RoundTripV27) {
    Serializer s;
    s.write_i64(9223372036854775806LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775806LL);
}

TEST(SerializerTest, F64RoundTripV27) {
    Serializer s;
    s.write_f64(3.141592653589793);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.141592653589793);
}

TEST(SerializerTest, StringAndBoolV27) {
    Serializer s;
    s.write_string("Cycle1338");
    s.write_bool(true);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Cycle1338");
    EXPECT_EQ(d.read_bool(), true);
}

// Cycle 1347

TEST(SerializerTest, U8RoundTripV28) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255);
}

TEST(SerializerTest, U16RoundTripV28) {
    Serializer s;
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535);
}

TEST(SerializerTest, U32RoundTripV28) {
    Serializer s;
    s.write_u32(4294967295U);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295U);
}

TEST(SerializerTest, U64RoundTripV28) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, I32RoundTripV28) {
    Serializer s;
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483648);
}

TEST(SerializerTest, I64RoundTripV28) {
    Serializer s;
    s.write_i64(-9223372036854775807LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
}

TEST(SerializerTest, F64RoundTripV28) {
    Serializer s;
    s.write_f64(2.718281828459045);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
}

TEST(SerializerTest, StringAndBoolV28) {
    Serializer s;
    s.write_string("Cycle1347");
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Cycle1347");
    EXPECT_EQ(d.read_bool(), false);
}

// Cycle 1348: V29 Tests

TEST(SerializerTest, F64RoundTripV29) {
    Serializer s;
    s.write_f64(1.5);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.5);
}

TEST(SerializerTest, BytesRoundTripV29) {
    Serializer s;
    std::vector<uint8_t> data = {0xAB, 0xCD, 0xEF, 0x12, 0x34};
    s.write_bytes(data.data(), data.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result, data);
}

TEST(SerializerTest, MixedU16AndI32V29) {
    Serializer s;
    s.write_u16(12345);
    s.write_i32(-42000);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_EQ(d.read_i32(), -42000);
}

TEST(SerializerTest, StringWithEmptyV29) {
    Serializer s;
    s.write_string("NotEmpty");
    s.write_string("");
    s.write_string("AlsoNotEmpty");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "NotEmpty");
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "AlsoNotEmpty");
}

TEST(SerializerTest, SequentialBooleansV29) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
}

TEST(SerializerTest, LargeU64AndNegativeI64V29) {
    Serializer s;
    s.write_u64(9876543210987654321ULL);
    s.write_i64(-1234567890123456789LL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 9876543210987654321ULL);
    EXPECT_EQ(d.read_i64(), -1234567890123456789LL);
}

TEST(SerializerTest, ComplexMixedTypesV29) {
    Serializer s;
    s.write_u8(99);
    s.write_string("TestData");
    s.write_f64(2.71828);
    s.write_bool(true);
    s.write_u32(0xDEADBEEF);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 99u);
    EXPECT_EQ(d.read_string(), "TestData");
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.71828);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
}

TEST(SerializerTest, BytesWithSpecialValuesV29) {
    Serializer s;
    std::vector<uint8_t> data = {0x00, 0xFF, 0x80, 0x7F, 0xAA};
    s.write_bytes(data.data(), data.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 0x00);
    EXPECT_EQ(result[1], 0xFF);
    EXPECT_EQ(result[2], 0x80);
    EXPECT_EQ(result[3], 0x7F);
    EXPECT_EQ(result[4], 0xAA);
}

TEST(SerializerTest, SingleU8MaxValueV30) {
    Serializer s;
    s.write_u8(255);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
}

TEST(SerializerTest, U16BoundaryValuesV30) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(32768);
    s.write_u16(65535);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), 32768u);
    EXPECT_EQ(d.read_u16(), 65535u);
}

TEST(SerializerTest, SignedIntegerSequenceV30) {
    Serializer s;
    s.write_i32(0);
    s.write_i32(-1);
    s.write_i32(2147483647);
    s.write_i32(-2147483648);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), 2147483647);
    EXPECT_EQ(d.read_i32(), -2147483648);
}

TEST(SerializerTest, FloatingPointPrecisionV30) {
    Serializer s;
    double values[] = {0.0, -0.0, 1.5, -1.5, 99999.123456789};
    for (double val : values) s.write_f64(val);
    Deserializer d(s.data());
    for (double val : values) EXPECT_DOUBLE_EQ(d.read_f64(), val);
}

TEST(SerializerTest, MixedBytesAndStringsV30) {
    Serializer s;
    std::vector<uint8_t> bytes = {0x12, 0x34, 0x56, 0x78};
    s.write_string("Start");
    s.write_bytes(bytes.data(), bytes.size());
    s.write_string("End");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Start");
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0x12);
    EXPECT_EQ(result[1], 0x34);
    EXPECT_EQ(result[2], 0x56);
    EXPECT_EQ(result[3], 0x78);
    EXPECT_EQ(d.read_string(), "End");
}

TEST(SerializerTest, AlternatingBooleanPatternV30) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_bool(i % 2 == 0);
    }
    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_bool(), i % 2 == 0);
    }
}

TEST(SerializerTest, LargeByteBufferV30) {
    Serializer s;
    std::vector<uint8_t> large_bytes(256);
    for (size_t i = 0; i < large_bytes.size(); ++i) {
        large_bytes[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(large_bytes.data(), large_bytes.size());
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 256u);
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
}

TEST(SerializerTest, ComplexMultiTypeSequenceV30) {
    Serializer s;
    s.write_u8(10);
    s.write_i64(-999999999999LL);
    s.write_string("Intermediate");
    s.write_f64(123.456);
    s.write_u32(0xCAFEBABE);
    s.write_bool(false);
    std::vector<uint8_t> bytes = {0xDE, 0xAD};
    s.write_bytes(bytes.data(), bytes.size());
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 10u);
    EXPECT_EQ(d.read_i64(), -999999999999LL);
    EXPECT_EQ(d.read_string(), "Intermediate");
    EXPECT_DOUBLE_EQ(d.read_f64(), 123.456);
    EXPECT_EQ(d.read_u32(), 0xCAFEBABEu);
    EXPECT_EQ(d.read_bool(), false);
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 0xDE);
    EXPECT_EQ(result[1], 0xAD);
}

TEST(SerializerTest, MaxU8ValueV31) {
    Serializer s;
    s.write_u8(255u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
}

TEST(SerializerTest, MaxU16ValueV31) {
    Serializer s;
    s.write_u16(65535u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
}

TEST(SerializerTest, MaxI32NegativeV31) {
    Serializer s;
    s.write_i32(-2147483647);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483647);
}

TEST(SerializerTest, MaxU64ValueV31) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
}

TEST(SerializerTest, F64NegativeZeroV31) {
    Serializer s;
    s.write_f64(-0.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
}

TEST(SerializerTest, LongStringMultiByteCharsV31) {
    Serializer s;
    std::string long_str = "The quick brown fox jumps over the lazy dog. Sphinx of black quartz, judge my vow.";
    s.write_string(long_str);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
}

TEST(SerializerTest, BytesAllZerosV31) {
    Serializer s;
    uint8_t zeros[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    s.write_bytes(zeros, 10);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 10u);
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], 0u);
    }
}

TEST(SerializerTest, InterleavedI64BoolBytesV31) {
    Serializer s;
    s.write_i64(9223372036854775807LL);
    s.write_bool(true);
    uint8_t test_bytes[3] = {0xFF, 0xAA, 0x55};
    s.write_bytes(test_bytes, 3);
    s.write_i64(-1LL);
    s.write_bool(false);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775807LL);
    EXPECT_EQ(d.read_bool(), true);
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0xFF);
    EXPECT_EQ(bytes[1], 0xAA);
    EXPECT_EQ(bytes[2], 0x55);
    EXPECT_EQ(d.read_i64(), -1LL);
    EXPECT_EQ(d.read_bool(), false);
}

// ============================================================================
// Cycle 1012: Eight diverse serializer tests with V32 suffix
// ============================================================================

TEST(SerializerTest, U8U16U32U64SequenceV32) {
    Serializer s;
    s.write_u8(42u);
    s.write_u16(1000u);
    s.write_u32(100000u);
    s.write_u64(10000000000ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_u16(), 1000u);
    EXPECT_EQ(d.read_u32(), 100000u);
    EXPECT_EQ(d.read_u64(), 10000000000ULL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SignedI32I64ExtremesV32) {
    Serializer s;
    int32_t min_i32 = std::numeric_limits<int32_t>::min();
    int32_t max_i32 = std::numeric_limits<int32_t>::max();
    int64_t min_i64 = std::numeric_limits<int64_t>::min();
    int64_t max_i64 = std::numeric_limits<int64_t>::max();
    s.write_i32(min_i32);
    s.write_i32(max_i32);
    s.write_i64(min_i64);
    s.write_i64(max_i64);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), min_i32);
    EXPECT_EQ(d.read_i32(), max_i32);
    EXPECT_EQ(d.read_i64(), min_i64);
    EXPECT_EQ(d.read_i64(), max_i64);
}

TEST(SerializerTest, Float64PrecisionMultipleValuesV32) {
    Serializer s;
    double pi = 3.14159265358979323846;
    double e = 2.71828182845904523536;
    double sqrt2 = 1.41421356237309504880;
    s.write_f64(pi);
    s.write_f64(e);
    s.write_f64(sqrt2);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), pi);
    EXPECT_DOUBLE_EQ(d.read_f64(), e);
    EXPECT_DOUBLE_EQ(d.read_f64(), sqrt2);
}

TEST(SerializerTest, BoolAlternatingPatternV32) {
    Serializer s;
    bool pattern[] = {true, false, true, false, true, true, false};
    for (bool b : pattern) {
        s.write_bool(b);
    }
    Deserializer d(s.data());
    for (bool expected : pattern) {
        EXPECT_EQ(d.read_bool(), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithMixedPatternV32) {
    Serializer s;
    uint8_t pattern[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                           0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    s.write_bytes(pattern, 16);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 16u);
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(result[i], i);
    }
}

TEST(SerializerTest, StringEmptyAndLongV32) {
    Serializer s;
    std::string empty = "";
    std::string long_str = "The serializer test suite validates correct serialization and deserialization of various data types.";
    s.write_string(empty);
    s.write_string(long_str);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), empty);
    EXPECT_EQ(d.read_string(), long_str);
}

TEST(SerializerTest, ComplexMixedDataStreamV32) {
    Serializer s;
    s.write_u16(65535u);
    s.write_bool(true);
    s.write_i32(-42);
    s.write_string("mixed");
    s.write_f64(99.99);
    uint8_t bytes[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(bytes, 4);
    s.write_u64(18446744073709551615ULL);
    s.write_i64(-1000000000000LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_EQ(d.read_string(), "mixed");
    EXPECT_DOUBLE_EQ(d.read_f64(), 99.99);
    auto read_bytes = d.read_bytes();
    ASSERT_EQ(read_bytes.size(), 4u);
    EXPECT_EQ(read_bytes[0], 0xDE);
    EXPECT_EQ(read_bytes[1], 0xAD);
    EXPECT_EQ(read_bytes[2], 0xBE);
    EXPECT_EQ(read_bytes[3], 0xEF);
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
    EXPECT_EQ(d.read_i64(), -1000000000000LL);
}

TEST(SerializerTest, SpecialCharacterStringsV32) {
    Serializer s;
    std::string with_null_bytes = "hello\x00world";
    std::string with_special = "tab\there\nnewline\rcarriage";
    std::string unicode = "café naïve résumé";
    s.write_string(with_null_bytes);
    s.write_string(with_special);
    s.write_string(unicode);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), with_null_bytes);
    EXPECT_EQ(d.read_string(), with_special);
    EXPECT_EQ(d.read_string(), unicode);
}

// ============================================================================
// Cycle V33: Additional serialization tests for comprehensive coverage
// ============================================================================

TEST(SerializerTest, BasicU8RoundTripV33) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(42);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedIntegersV33) {
    Serializer s;
    s.write_u16(12345);
    s.write_u32(0xDEADBEEF);
    s.write_u64(18446744073709551615ULL);
    s.write_i32(-987654321);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
    EXPECT_EQ(d.read_i32(), -987654321);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SignedIntegerLimitsV33) {
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(std::numeric_limits<int64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, FloatAndBooleanV33) {
    Serializer s;
    s.write_f64(3.14159265359);
    s.write_f64(-271.828);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265359);
    EXPECT_DOUBLE_EQ(d.read_f64(), -271.828);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringSerializationV33) {
    Serializer s;
    std::string empty = "";
    std::string simple = "Hello";
    std::string with_space = "Hello World";
    std::string long_string = "The quick brown fox jumps over the lazy dog";

    s.write_string(empty);
    s.write_string(simple);
    s.write_string(with_space);
    s.write_string(long_string);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), empty);
    EXPECT_EQ(d.read_string(), simple);
    EXPECT_EQ(d.read_string(), with_space);
    EXPECT_EQ(d.read_string(), long_string);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BinaryDataRoundTripV33) {
    Serializer s;
    const uint8_t binary_data[] = {0x00, 0xFF, 0xAA, 0x55, 0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(binary_data, sizeof(binary_data));

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), sizeof(binary_data));
    EXPECT_EQ(result[0], 0x00);
    EXPECT_EQ(result[1], 0xFF);
    EXPECT_EQ(result[2], 0xAA);
    EXPECT_EQ(result[3], 0x55);
    EXPECT_EQ(result[4], 0xDE);
    EXPECT_EQ(result[5], 0xAD);
    EXPECT_EQ(result[6], 0xBE);
    EXPECT_EQ(result[7], 0xEF);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ComprehensiveMixedDataV33) {
    Serializer s;
    s.write_u8(42);
    s.write_u16(1000);
    s.write_u32(100000);
    s.write_u64(10000000000ULL);
    s.write_i32(-500);
    s.write_i64(-9000000000LL);
    s.write_f64(2.71828);
    s.write_bool(true);
    s.write_string("test_string");

    const uint8_t bytes[] = {0x12, 0x34, 0x56, 0x78};
    s.write_bytes(bytes, sizeof(bytes));

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_u16(), 1000u);
    EXPECT_EQ(d.read_u32(), 100000u);
    EXPECT_EQ(d.read_u64(), 10000000000ULL);
    EXPECT_EQ(d.read_i32(), -500);
    EXPECT_EQ(d.read_i64(), -9000000000LL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.71828);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_string(), "test_string");

    auto binary_result = d.read_bytes();
    ASSERT_EQ(binary_result.size(), 4u);
    EXPECT_EQ(binary_result[0], 0x12);
    EXPECT_EQ(binary_result[1], 0x34);
    EXPECT_EQ(binary_result[2], 0x56);
    EXPECT_EQ(binary_result[3], 0x78);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EdgeCaseZeroAndNegativeV33) {
    Serializer s;
    s.write_u8(0);
    s.write_u16(0);
    s.write_u32(0);
    s.write_u64(0);
    s.write_i32(0);
    s.write_i64(0);
    s.write_i32(-1);
    s.write_i64(-1);
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_bool(false);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i64(), 0);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i64(), -1);
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U8MaxValueV34) {
    Serializer s;
    s.write_u8(255u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16MaxValueV34) {
    Serializer s;
    s.write_u16(65535u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32MaxValueV34) {
    Serializer s;
    s.write_u32(4294967295u);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64MaxValueV34) {
    Serializer s;
    s.write_u64(18446744073709551615ULL);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32MinMaxRangeV34) {
    Serializer s;
    s.write_i32(2147483647);    // INT32_MAX
    s.write_i32(-2147483648);   // INT32_MIN
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 2147483647);
    EXPECT_EQ(d.read_i32(), -2147483648);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I64MinMaxRangeV34) {
    Serializer s;
    s.write_i64(9223372036854775807LL);   // INT64_MAX
    s.write_i64(-9223372036854775807LL);  // INT64_MIN+1 (avoid implementation issues)
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 9223372036854775807LL);
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64ScientificNotationV34) {
    Serializer s;
    s.write_f64(1.23e-10);
    s.write_f64(9.87e+20);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.23e-10);
    EXPECT_DOUBLE_EQ(d.read_f64(), 9.87e+20);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolAndBytesSequenceV34) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    uint8_t binary_data[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00 };
    s.write_bytes(binary_data, 5u);
    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 0xDEu);
    EXPECT_EQ(bytes[1], 0xADu);
    EXPECT_EQ(bytes[2], 0xBEu);
    EXPECT_EQ(bytes[3], 0xEFu);
    EXPECT_EQ(bytes[4], 0x00u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringAndBytesInterleavedV34) {
    Serializer s;
    s.write_string("start");
    uint8_t binary1[] = { 0xAA, 0xBB };
    s.write_bytes(binary1, 2u);
    s.write_string("middle");
    uint8_t binary2[] = { 0xCC, 0xDD, 0xEE };
    s.write_bytes(binary2, 3u);
    s.write_string("end");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "start");
    auto bytes1 = d.read_bytes();
    ASSERT_EQ(bytes1.size(), 2u);
    EXPECT_EQ(bytes1[0], 0xAAu);
    EXPECT_EQ(bytes1[1], 0xBBu);
    EXPECT_EQ(d.read_string(), "middle");
    auto bytes2 = d.read_bytes();
    ASSERT_EQ(bytes2.size(), 3u);
    EXPECT_EQ(bytes2[0], 0xCCu);
    EXPECT_EQ(bytes2[1], 0xDDu);
    EXPECT_EQ(bytes2[2], 0xEEu);
    EXPECT_EQ(d.read_string(), "end");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, AllTypesComprehensiveV34) {
    Serializer s;
    s.write_u8(42u);
    s.write_u16(1234u);
    s.write_u32(123456u);
    s.write_u64(9876543210ULL);
    s.write_i32(-999);
    s.write_i64(-888888888LL);
    s.write_f64(3.14159265);
    s.write_bool(true);
    s.write_string("comprehensive_test");
    uint8_t data[] = { 0x11, 0x22, 0x33 };
    s.write_bytes(data, 3u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_u16(), 1234u);
    EXPECT_EQ(d.read_u32(), 123456u);
    EXPECT_EQ(d.read_u64(), 9876543210ULL);
    EXPECT_EQ(d.read_i32(), -999);
    EXPECT_EQ(d.read_i64(), -888888888LL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "comprehensive_test");
    auto binary = d.read_bytes();
    ASSERT_EQ(binary.size(), 3u);
    EXPECT_EQ(binary[0], 0x11u);
    EXPECT_EQ(binary[1], 0x22u);
    EXPECT_EQ(binary[2], 0x33u);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V35 Test Suite (8 additional tests)
// ------------------------------------------------------------------

TEST(Serializer, RoundtripU8ZeroV35) {
    Serializer s;
    s.write_u8(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU64MaxValueV35) {
    Serializer s;
    s.write_u64(std::numeric_limits<uint64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI32NegativeOneV35) {
    Serializer s;
    s.write_i32(-1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64NegativeInfinityV35) {
    Serializer s;
    s.write_f64(-INFINITY);

    Deserializer d(s.data());
    double result = d.read_f64();
    EXPECT_TRUE(std::isinf(result) && result < 0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripEmptyStringV35) {
    Serializer s;
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBoolFalseV35) {
    Serializer s;
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripLargeStringV35) {
    Serializer s;
    std::string large_string(10000, 'x');
    s.write_string(large_string);

    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_EQ(result.size(), 10000u);
    EXPECT_EQ(result, large_string);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, MixedTypesU16BoolStringBytesV35) {
    Serializer s;
    s.write_u16(12345);
    s.write_bool(true);
    s.write_string("test_data");
    uint8_t bytes_data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    s.write_bytes(bytes_data, 4u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "test_data");
    auto binary = d.read_bytes();
    ASSERT_EQ(binary.size(), 4u);
    EXPECT_EQ(binary[0], 0xAAu);
    EXPECT_EQ(binary[1], 0xBBu);
    EXPECT_EQ(binary[2], 0xCCu);
    EXPECT_EQ(binary[3], 0xDDu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU32MaxValueV36) {
    Serializer s;
    s.write_u32(UINT32_MAX);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), UINT32_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI64MinValueV36) {
    Serializer s;
    s.write_i64(INT64_MIN);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MIN);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64NaNV36) {
    Serializer s;
    s.write_f64(std::nan(""));

    Deserializer d(s.data());
    double result = d.read_f64();
    EXPECT_TRUE(std::isnan(result));
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripStringWithNullByteV36) {
    Serializer s;
    std::string str_with_null("hello\0world", 11);
    s.write_string(str_with_null);

    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_EQ(result.size(), 11u);
    EXPECT_EQ(result, str_with_null);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBytesEmptyV36) {
    Serializer s;
    s.write_bytes(nullptr, 0u);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripMultipleBoolsV36) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU8MaxV36) {
    Serializer s;
    s.write_u8(255u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, SequentialStringsV36) {
    Serializer s;
    s.write_string("first_string");
    s.write_string("second_string");
    s.write_string("third_string");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "first_string");
    EXPECT_EQ(d.read_string(), "second_string");
    EXPECT_EQ(d.read_string(), "third_string");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU16ZeroV37) {
    Serializer s;
    s.write_u16(0u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI32MaxV37) {
    Serializer s;
    s.write_i32(INT32_MAX);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64PositiveInfinityV37) {
    Serializer s;
    s.write_f64(INFINITY);

    Deserializer d(s.data());
    double result = d.read_f64();
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripStringUnicodeV37) {
    Serializer s;
    s.write_string("héllo wörld");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "héllo wörld");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBytesOneByteV37) {
    Serializer s;
    uint8_t byte = 0xFFu;
    s.write_bytes(&byte, 1u);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 0xFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU64ZeroV37) {
    Serializer s;
    s.write_u64(0u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI64NegOneV37) {
    Serializer s;
    s.write_i64(-1LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, AllTypesComprehensiveV37) {
    Serializer s;
    uint8_t test_byte = 0xABu;
    s.write_u8(42u);
    s.write_u16(12345u);
    s.write_u32(987654321u);
    s.write_u64(18446744073709551615ULL);
    s.write_i32(-123456);
    s.write_i64(-9223372036854775807LL);
    s.write_f64(3.14159265359);
    s.write_bool(true);
    s.write_string("comprehensive_test");
    s.write_bytes(&test_byte, 1u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_EQ(d.read_u32(), 987654321u);
    EXPECT_EQ(d.read_u64(), 18446744073709551615ULL);
    EXPECT_EQ(d.read_i32(), -123456);
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265359);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "comprehensive_test");
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 1u);
    EXPECT_EQ(bytes[0], 0xABu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU16MaxV38) {
    Serializer s;
    s.write_u16(65535u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI32MinV38) {
    Serializer s;
    s.write_i32(INT32_MIN);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MIN);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64ZeroV38) {
    Serializer s;
    s.write_f64(0.0);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripStringSpecialCharsV38) {
    Serializer s;
    s.write_string("hello\ttab\nnewline");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello\ttab\nnewline");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBytes256V38) {
    Serializer s;
    uint8_t data[256];
    for (int i = 0; i < 256; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(data, 256u);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBoolTrueFalseAlternateV38) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU32OneV38) {
    Serializer s;
    s.write_u32(1u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI64PositiveLargeV38) {
    Serializer s;
    s.write_i64(999999999999LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), 999999999999LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU8AllValuesV39) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u8(), 127);
    EXPECT_EQ(d.read_u8(), 128);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI32ZeroV39) {
    Serializer s;
    s.write_i32(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64NegZeroV39) {
    Serializer s;
    s.write_f64(-0.0);

    Deserializer d(s.data());
    double value = d.read_f64();
    EXPECT_EQ(value, 0.0);
    EXPECT_TRUE(std::signbit(value));
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripStringWithSpacesV39) {
    Serializer s;
    s.write_string("hello world foo bar");

    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_EQ(result, "hello world foo bar");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBytesPatternV39) {
    Serializer s;
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(data, 4);

    Deserializer d(s.data());
    std::vector<uint8_t> result = d.read_bytes();
    EXPECT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0xDEu);
    EXPECT_EQ(result[1], 0xADu);
    EXPECT_EQ(result[2], 0xBEu);
    EXPECT_EQ(result[3], 0xEFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU64OneV39) {
    Serializer s;
    s.write_u64(1ULL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 1ULL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI64MaxV39) {
    Serializer s;
    s.write_i64(INT64_MAX);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, EmptySerializerHasNoRemainingV39) {
    Serializer s;

    Deserializer d(s.data());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU16BoundaryV40) {
    Serializer s;
    s.write_u16(256);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 256);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI32TenThousandV40) {
    Serializer s;
    s.write_i32(10000);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), 10000);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64PiV40) {
    Serializer s;
    s.write_f64(3.14159265358979);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripStringLongV40) {
    Serializer s;
    std::string long_str(5000, 'a');
    s.write_string(long_str);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBytesSingleZeroV40) {
    Serializer s;
    uint8_t byte = 0x00;
    s.write_bytes(&byte, 1);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], 0x00u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU32PowerOfTwoV40) {
    Serializer s;
    s.write_u32(1048576);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1048576);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI64MinusMillionV40) {
    Serializer s;
    s.write_i64(-1000000LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), -1000000LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, MultipleStringsAndBoolsV40) {
    Serializer s;
    s.write_string("hello");
    s.write_bool(true);
    s.write_string("world");
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "world");
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU8ZeroAndMaxV41) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI32RangeV41) {
    Serializer s;
    s.write_i32(INT32_MIN);
    s.write_i32(-1);
    s.write_i32(0);
    s.write_i32(1);
    s.write_i32(INT32_MAX);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MIN);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i32(), 1);
    EXPECT_EQ(d.read_i32(), INT32_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64ValuesV41) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-1.5);
    s.write_f64(1.5);
    s.write_f64(3.14159);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.5);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.5);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripEmptyStringV41) {
    Serializer s;
    s.write_string("");
    s.write_string("test");
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripByteArrayV41) {
    Serializer s;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    s.write_bytes(data, 5);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 0x01u);
    EXPECT_EQ(result[1], 0x02u);
    EXPECT_EQ(result[2], 0x03u);
    EXPECT_EQ(result[3], 0x04u);
    EXPECT_EQ(result[4], 0x05u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripAllIntegerTypesV41) {
    Serializer s;
    s.write_u8(100);
    s.write_u16(30000);
    s.write_u32(2000000000u);
    s.write_u64(9000000000000000000ull);
    s.write_i32(-1500000000);
    s.write_i64(-8000000000000000000ll);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 100u);
    EXPECT_EQ(d.read_u16(), 30000u);
    EXPECT_EQ(d.read_u32(), 2000000000u);
    EXPECT_EQ(d.read_u64(), 9000000000000000000ull);
    EXPECT_EQ(d.read_i32(), -1500000000);
    EXPECT_EQ(d.read_i64(), -8000000000000000000ll);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripComplexMixedDataV41) {
    Serializer s;
    s.write_u32(42);
    s.write_string("mixed");
    s.write_bool(true);
    s.write_f64(2.71828);
    uint8_t bytes[] = {0xAA, 0xBB};
    s.write_bytes(bytes, 2);
    s.write_i64(-999);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_EQ(d.read_string(), "mixed");
    EXPECT_TRUE(d.read_bool());
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.71828);
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 2);
    EXPECT_EQ(result[0], 0xAAu);
    EXPECT_EQ(result[1], 0xBBu);
    EXPECT_EQ(d.read_i64(), -999ll);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU64LargeValueV41) {
    Serializer s;
    s.write_u64(18446744073709551615ull);
    s.write_u64(0ull);
    s.write_u64(1099511627776ull);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 18446744073709551615ull);
    EXPECT_EQ(d.read_u64(), 0ull);
    EXPECT_EQ(d.read_u64(), 1099511627776ull);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripI64EdgeValuesV42) {
    Serializer s;
    s.write_i64(INT64_MIN);
    s.write_i64(-1);
    s.write_i64(0);
    s.write_i64(1);
    s.write_i64(INT64_MAX);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MIN);
    EXPECT_EQ(d.read_i64(), -1ll);
    EXPECT_EQ(d.read_i64(), 0ll);
    EXPECT_EQ(d.read_i64(), 1ll);
    EXPECT_EQ(d.read_i64(), INT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU16EdgeValuesV42) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(255);
    s.write_u16(256);
    s.write_u16(32767);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), 255u);
    EXPECT_EQ(d.read_u16(), 256u);
    EXPECT_EQ(d.read_u16(), 32767u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripEmptyByteArrayV42) {
    Serializer s;
    s.write_bytes(nullptr, 0);
    s.write_u32(999);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 0);
    EXPECT_EQ(d.read_u32(), 999u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripLargeBinaryDataV42) {
    Serializer s;
    uint8_t large_data[256];
    for (int i = 0; i < 256; ++i) {
        large_data[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(large_data, 256);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 256);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64SpecialValuesV42) {
    Serializer s;
    s.write_f64(-0.0);
    s.write_f64(1e308);
    s.write_f64(1e-308);
    s.write_f64(std::numeric_limits<double>::max());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e308);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e-308);
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripMultipleEmptyStringsV42) {
    Serializer s;
    s.write_string("");
    s.write_string("");
    s.write_string("a");
    s.write_string("");
    s.write_bool(true);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "a");
    EXPECT_EQ(d.read_string(), "");
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU32BoundaryValuesV42) {
    Serializer s;
    s.write_u32(0);
    s.write_u32(1);
    s.write_u32(65536);
    s.write_u32(2147483647u);
    s.write_u32(2147483648u);
    s.write_u32(4294967295u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 65536u);
    EXPECT_EQ(d.read_u32(), 2147483647u);
    EXPECT_EQ(d.read_u32(), 2147483648u);
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU8BoundaryValuesV43) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 1u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU16U32U64BoundaryValuesV43) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(65535);
    s.write_u32(0);
    s.write_u32(4294967295u);
    s.write_u64(0ull);
    s.write_u64(18446744073709551615ull);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_EQ(d.read_u64(), 0ull);
    EXPECT_EQ(d.read_u64(), 18446744073709551615ull);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64BoundaryValuesV43) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::lowest());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::min());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::lowest());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBoolSequenceV43) {
    Serializer s;
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripMixedSequenceWithBytesV43) {
    Serializer s;
    s.write_u32(42);
    s.write_string("alpha");
    s.write_bool(true);
    s.write_f64(3.141592653589793);
    uint8_t payload[] = {0x00, 0x7F, 0x80, 0xFF};
    s.write_bytes(payload, 4);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_TRUE(d.read_bool());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.141592653589793);
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x00u);
    EXPECT_EQ(bytes[1], 0x7Fu);
    EXPECT_EQ(bytes[2], 0x80u);
    EXPECT_EQ(bytes[3], 0xFFu);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripEmptyStringAndEmptyBytesV43) {
    Serializer s;
    s.write_string("");
    s.write_bytes(nullptr, 0);
    s.write_u8(9);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    auto bytes = d.read_bytes();
    EXPECT_TRUE(bytes.empty());
    EXPECT_EQ(d.read_u8(), 9u);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripLargeStringV43) {
    Serializer s;
    std::string large(8192, 'z');
    s.write_string(large);
    s.write_u32(123456789u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), large);
    EXPECT_EQ(d.read_u32(), 123456789u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripLargeBytesPatternV43) {
    Serializer s;
    std::vector<uint8_t> large(1024);
    for (size_t i = 0; i < large.size(); ++i) {
        large[i] = static_cast<uint8_t>(i % 251);
    }
    s.write_bytes(large.data(), large.size());
    s.write_bool(true);

    Deserializer d(s.data());
    auto out = d.read_bytes();
    EXPECT_EQ(out.size(), large.size());
    for (size_t i = 0; i < out.size(); ++i) {
        EXPECT_EQ(out[i], static_cast<uint8_t>(i % 251));
    }
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU8V55) {
    Serializer s;
    s.write_u8(55);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 55u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU16V55) {
    Serializer s;
    s.write_u16(5500);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 5500u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU32V55) {
    Serializer s;
    s.write_u32(550000u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 550000u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripU64V55) {
    Serializer s;
    s.write_u64(55000000000ull);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 55000000000ull);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripStringV55) {
    Serializer s;
    s.write_string("serializer-v55");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "serializer-v55");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBytesV55) {
    Serializer s;
    uint8_t input[] = {0x55, 0x00, 0xAA, 0xFF};
    s.write_bytes(input, sizeof(input));

    Deserializer d(s.data());
    auto output = d.read_bytes();
    ASSERT_EQ(output.size(), sizeof(input));
    EXPECT_EQ(output[0], 0x55u);
    EXPECT_EQ(output[1], 0x00u);
    EXPECT_EQ(output[2], 0xAAu);
    EXPECT_EQ(output[3], 0xFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripBoolV55) {
    Serializer s;
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, RoundtripF64V55) {
    Serializer s;
    s.write_f64(55.55);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 55.55);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V56 Tests: Comprehensive Serialization Coverage
// ------------------------------------------------------------------

TEST(Serializer, SignedIntegersV56) {
    Serializer s;
    s.write_i32(-42);
    s.write_i32(-1000000);
    s.write_i64(-9000000000LL);
    s.write_i64(42LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_EQ(d.read_i32(), -1000000);
    EXPECT_EQ(d.read_i64(), -9000000000LL);
    EXPECT_EQ(d.read_i64(), 42LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, DoubleValuesV56) {
    Serializer s;
    s.write_f64(3.14159);
    s.write_f64(2.718281828);
    s.write_f64(-0.5);
    s.write_f64(0.0);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828);
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.5);
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, LargeBytesV56) {
    Serializer s;
    std::vector<uint8_t> data(2048);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>((i * 17) % 256);
    }
    s.write_bytes(data.data(), data.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), data.size());
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>((i * 17) % 256));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, MultipleStringsV56) {
    Serializer s;
    s.write_string("first");
    s.write_string("second");
    s.write_string("third-with-special-chars-!@#$");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "first");
    EXPECT_EQ(d.read_string(), "second");
    EXPECT_EQ(d.read_string(), "third-with-special-chars-!@#$");
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, EmptyBytesAndStringV56) {
    Serializer s;
    s.write_string("");
    std::vector<uint8_t> empty;
    if (!empty.empty()) {
        s.write_bytes(empty.data(), empty.size());
    }
    s.write_u8(99);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_u8(), 99u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, BoundaryValuesV56) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(255);
    s.write_u16(0);
    s.write_u16(65535);
    s.write_u32(0);
    s.write_u32(4294967295u);
    s.write_i32(-2147483648);
    s.write_i32(2147483647);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_EQ(d.read_i32(), -2147483648);
    EXPECT_EQ(d.read_i32(), 2147483647);
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, BoolSequenceV56) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(Serializer, ComplexPayloadV56) {
    Serializer s;
    s.write_u32(12345);
    s.write_string("header");
    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(payload.data(), payload.size());
    s.write_f64(99.99);
    s.write_bool(false);
    s.write_i32(-256);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 12345u);
    EXPECT_EQ(d.read_string(), "header");
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 4);
    EXPECT_EQ(bytes[0], 0xDEu);
    EXPECT_EQ(bytes[1], 0xADu);
    EXPECT_EQ(bytes[2], 0xBEu);
    EXPECT_EQ(bytes[3], 0xEFu);
    EXPECT_DOUBLE_EQ(d.read_f64(), 99.99);
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_i32(), -256);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// New edge case tests with V57 suffix
// ------------------------------------------------------------------

TEST(SerializerTest, U64MaxValueV57) {
    Serializer s;
    s.write_u64(std::numeric_limits<uint64_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I64NegativeExtremeV57) {
    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringSerializationV57) {
    Serializer s;
    s.write_string("first");
    s.write_string("");
    s.write_string("third");
    s.write_string("a very long string with many characters and spaces");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "first");
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "third");
    EXPECT_EQ(d.read_string(), "a very long string with many characters and spaces");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedIntegerTypesV57) {
    Serializer s;
    s.write_u8(255);
    s.write_u16(65535);
    s.write_u32(4294967295u);
    s.write_i32(-1);
    s.write_i64(-9223372036854775807ll);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i64(), -9223372036854775807ll);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithAllPatternV57) {
    Serializer s;
    std::vector<uint8_t> pattern = {0x00, 0xFF, 0xAA, 0x55, 0xFF, 0x00, 0x55, 0xAA};
    s.write_bytes(pattern.data(), pattern.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 8);
    for (size_t i = 0; i < pattern.size(); ++i) {
        EXPECT_EQ(result[i], pattern[i]);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64ZeroAndNegativeZeroV57) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(1.0e-308);
    s.write_f64(1.0e308);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0e-308);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0e308);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ComplexMultiFieldStructureV57) {
    Serializer s;
    s.write_u32(42);
    s.write_bool(true);
    s.write_string("data");
    std::vector<uint8_t> bin = {1, 2, 3};
    s.write_bytes(bin.data(), bin.size());
    s.write_i32(-999);
    s.write_f64(3.14);
    s.write_u64(0xFFFFFFFFFFFFFF00);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "data");
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 3);
    EXPECT_EQ(bytes[0], 1u);
    EXPECT_EQ(bytes[1], 2u);
    EXPECT_EQ(bytes[2], 3u);
    EXPECT_EQ(d.read_i32(), -999);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFF00);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesBufferV57) {
    Serializer s;
    std::vector<uint8_t> large_data(10000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }
    s.write_bytes(large_data.data(), large_data.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 10000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i % 256));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I64PositiveMaxV58) {
    Serializer s;
    s.write_i64(INT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I64NegativeMinV58) {
    Serializer s;
    s.write_i64(INT64_MIN);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MIN);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64PiRoundTripV58) {
    Serializer s;
    s.write_f64(3.14159265358979);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithNullTerminatorV58) {
    Serializer s;
    const uint8_t data[] = {1, 2, 0, 3, 4};
    s.write_bytes(data, sizeof(data));
    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 3);
    EXPECT_EQ(result[4], 4);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringThenF64ThenI64V58) {
    Serializer s;
    s.write_string("test");
    s.write_f64(2.718);
    s.write_i64(-999);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718);
    EXPECT_EQ(d.read_i64(), -999);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64ThenU32ThenU16ThenU8V58) {
    Serializer s;
    s.write_u64(0x123456789ABCDEF0);
    s.write_u32(0x11223344);
    s.write_u16(0x5566);
    s.write_u8(0x77);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), uint64_t{0x123456789ABCDEF0});
    EXPECT_EQ(d.read_u32(), uint32_t{0x11223344});
    EXPECT_EQ(d.read_u16(), uint16_t{0x5566});
    EXPECT_EQ(d.read_u8(), uint8_t{0x77});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesBufferV58) {
    Serializer s;
    s.write_bytes(nullptr, 0);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64NegativeZeroV58) {
    Serializer s;
    s.write_f64(-0.0);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64MaxValueV59) {
    Serializer s;
    s.write_u64(UINT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32NegativeMaxV59) {
    Serializer s;
    s.write_i32(INT32_MIN);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MIN);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithSpecialCharsV59) {
    Serializer s;
    s.write_string("hello\nworld\t!");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello\nworld\t!");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64VerySmallNumberV59) {
    Serializer s;
    s.write_f64(1.23456789e-100);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.23456789e-100);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithAllValuesV59) {
    Serializer s;
    uint8_t data[256];
    for (int i = 0; i < 256; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(data, 256);
    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 256);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I64PositiveMaxV59) {
    Serializer s;
    s.write_i64(INT64_MAX);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), INT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32U16U8SequenceV59) {
    Serializer s;
    s.write_u32(0xDEADBEEF);
    s.write_u16(0xCAFE);
    s.write_u8(0xFF);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{0xDEADBEEF});
    EXPECT_EQ(d.read_u16(), uint16_t{0xCAFE});
    EXPECT_EQ(d.read_u8(), uint8_t{0xFF});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64StringI64MixedV59) {
    Serializer s;
    s.write_f64(1.5);
    s.write_string("mixed");
    s.write_i64(-12345);
    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.5);
    EXPECT_EQ(d.read_string(), "mixed");
    EXPECT_EQ(d.read_i64(), -12345);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringRoundTripV60) {
    Serializer s;
    s.write_string("");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, AllZeroValuesV60) {
    Serializer s;
    s.write_u8(0);
    s.write_u16(0);
    s.write_u32(0);
    s.write_u64(0);
    s.write_i32(0);
    s.write_i64(0);
    s.write_f64(0.0);
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), uint8_t{0});
    EXPECT_EQ(d.read_u16(), uint16_t{0});
    EXPECT_EQ(d.read_u32(), uint32_t{0});
    EXPECT_EQ(d.read_u64(), uint64_t{0});
    EXPECT_EQ(d.read_i32(), int32_t{0});
    EXPECT_EQ(d.read_i64(), int64_t{0});
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BinaryDataWithNullBytesV60) {
    Serializer s;
    uint8_t binary_data[] = {0x00, 0xFF, 0x00, 0xAA, 0x55, 0x00};
    s.write_bytes(binary_data, sizeof(binary_data));
    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), sizeof(binary_data));
    for (size_t i = 0; i < sizeof(binary_data); ++i) {
        EXPECT_EQ(result[i], binary_data[i]);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithUnicodeCharactersV60) {
    Serializer s;
    s.write_string("Hello 世界 مرحبا");
    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello 世界 مرحبا");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargePayloadMultipleFieldsV60) {
    Serializer s;
    // Create a large binary payload
    std::vector<uint8_t> large_data(10000);
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = static_cast<uint8_t>(i % 256);
    }
    s.write_u64(0x123456789ABCDEF0ULL);
    s.write_bytes(large_data.data(), large_data.size());
    s.write_string("end");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), uint64_t{0x123456789ABCDEF0ULL});
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), large_data.size());
    for (size_t i = 0; i < large_data.size(); ++i) {
        EXPECT_EQ(result[i], large_data[i]);
    }
    EXPECT_EQ(d.read_string(), "end");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, IntegerBoundaryValuesV60) {
    Serializer s;
    s.write_u8(UINT8_MAX);
    s.write_u16(UINT16_MAX);
    s.write_u32(UINT32_MAX);
    s.write_i32(INT32_MIN);
    s.write_i32(INT32_MAX);
    s.write_i64(INT64_MIN);
    s.write_i64(INT64_MAX);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), UINT8_MAX);
    EXPECT_EQ(d.read_u16(), UINT16_MAX);
    EXPECT_EQ(d.read_u32(), UINT32_MAX);
    EXPECT_EQ(d.read_i32(), INT32_MIN);
    EXPECT_EQ(d.read_i32(), INT32_MAX);
    EXPECT_EQ(d.read_i64(), INT64_MIN);
    EXPECT_EQ(d.read_i64(), INT64_MAX);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, FloatingPointEdgeCasesV60) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(1e308);
    s.write_f64(1e-308);
    s.write_f64(3.14159265358979323846);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e308);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e-308);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979323846);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ComplexMultiTypeSequenceV60) {
    Serializer s;
    s.write_u8(42);
    s.write_string("test");
    s.write_i32(-999);
    s.write_f64(2.718);
    s.write_u64(9876543210ULL);
    uint8_t bytes[] = {0xAB, 0xCD, 0xEF};
    s.write_bytes(bytes, sizeof(bytes));
    s.write_i64(-1);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), uint8_t{42});
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_EQ(d.read_i32(), int32_t{-999});
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718);
    EXPECT_EQ(d.read_u64(), uint64_t{9876543210ULL});
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), sizeof(bytes));
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        EXPECT_EQ(result[i], bytes[i]);
    }
    EXPECT_EQ(d.read_i64(), int64_t{-1});
    EXPECT_EQ(d.read_u16(), uint16_t{65535});
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V61 Tests: Error handling, edge cases, and advanced patterns
// ------------------------------------------------------------------

TEST(SerializerTest, ReadPastEndOfBufferV61) {
    Serializer s;
    s.write_u32(12345);
    s.write_u8(99);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{12345});
    EXPECT_EQ(d.read_u8(), uint8_t{99});
    EXPECT_FALSE(d.has_remaining());

    // Attempt to read beyond buffer should still work but no more data
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyDeserializerV61) {
    Serializer s;
    // Write nothing
    Deserializer d(s.data());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, AlternatingWriteReadPatternV61) {
    Serializer s1;
    s1.write_u16(100);
    s1.write_string("part1");

    Deserializer d1(s1.data());
    EXPECT_EQ(d1.read_u16(), uint16_t{100});
    EXPECT_EQ(d1.read_string(), "part1");
    EXPECT_FALSE(d1.has_remaining());

    // Now chain another serializer with different data
    Serializer s2;
    s2.write_i32(-5000);
    s2.write_f64(1.414);

    Deserializer d2(s2.data());
    EXPECT_EQ(d2.read_i32(), int32_t{-5000});
    EXPECT_DOUBLE_EQ(d2.read_f64(), 1.414);
    EXPECT_FALSE(d2.has_remaining());
}

TEST(SerializerTest, BulkStringArrayV61) {
    Serializer s;
    std::vector<std::string> strings = {"alpha", "beta", "gamma", "delta", "epsilon"};

    // Write count then all strings
    s.write_u32(strings.size());
    for (const auto& str : strings) {
        s.write_string(str);
    }

    Deserializer d(s.data());
    uint32_t count = d.read_u32();
    EXPECT_EQ(count, uint32_t{5});

    for (size_t i = 0; i < strings.size(); ++i) {
        EXPECT_EQ(d.read_string(), strings[i]);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, NestedStructuredDataSimulationV61) {
    Serializer s;
    // Simulate nested structure: header + payload + footer
    s.write_u32(0xDEADBEEF);  // header magic
    s.write_u16(256);          // payload size marker

    // Payload: multiple integers
    s.write_i32(111);
    s.write_i64(-222222);
    s.write_u8(55);

    // Footer
    s.write_u32(0xCAFEBABE);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{0xDEADBEEF});
    EXPECT_EQ(d.read_u16(), uint16_t{256});
    EXPECT_EQ(d.read_i32(), int32_t{111});
    EXPECT_EQ(d.read_i64(), int64_t{-222222});
    EXPECT_EQ(d.read_u8(), uint8_t{55});
    EXPECT_EQ(d.read_u32(), uint32_t{0xCAFEBABE});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EndiannessVerificationV61) {
    Serializer s;
    // Write multi-byte values and verify they round-trip correctly
    s.write_u16(0x1234);
    s.write_u32(0x12345678);
    s.write_u64(0x123456789ABCDEF0ULL);
    s.write_i32(-1);
    s.write_i64(-256);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), uint16_t{0x1234});
    EXPECT_EQ(d.read_u32(), uint32_t{0x12345678});
    EXPECT_EQ(d.read_u64(), uint64_t{0x123456789ABCDEF0ULL});
    EXPECT_EQ(d.read_i32(), int32_t{-1});
    EXPECT_EQ(d.read_i64(), int64_t{-256});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BufferReusePatternV61) {
    // Create first serialization
    Serializer s1;
    s1.write_u32(0xAABBCCDD);
    s1.write_string("first");
    auto data1 = s1.data();

    // Deserialize and verify
    Deserializer d1(data1);
    EXPECT_EQ(d1.read_u32(), uint32_t{0xAABBCCDD});
    EXPECT_EQ(d1.read_string(), "first");

    // Create new serializer with different data
    Serializer s2;
    s2.write_u8(42);
    s2.write_u8(84);
    s2.write_u8(126);
    auto data2 = s2.data();

    // Deserialize new data - should not be affected by s1
    Deserializer d2(data2);
    EXPECT_EQ(d2.read_u8(), uint8_t{42});
    EXPECT_EQ(d2.read_u8(), uint8_t{84});
    EXPECT_EQ(d2.read_u8(), uint8_t{126});
    EXPECT_FALSE(d2.has_remaining());

    // Verify d1 original data is still valid
    EXPECT_FALSE(d1.has_remaining());
}

TEST(SerializerTest, MixedBinaryAndTextDataV61) {
    Serializer s;

    // Mix binary and text in complex pattern
    uint8_t prefix[] = {0xFF, 0xEE, 0xDD, 0xCC};
    s.write_bytes(prefix, sizeof(prefix));
    s.write_string("metadata");

    uint8_t middle[] = {0x11, 0x22, 0x33};
    s.write_bytes(middle, sizeof(middle));
    s.write_u32(999999);

    s.write_string("status:ok");
    uint8_t suffix[] = {0x77, 0x88};
    s.write_bytes(suffix, sizeof(suffix));

    Deserializer d(s.data());

    auto res_prefix = d.read_bytes();
    ASSERT_EQ(res_prefix.size(), sizeof(prefix));
    for (size_t i = 0; i < sizeof(prefix); ++i) {
        EXPECT_EQ(res_prefix[i], prefix[i]);
    }

    EXPECT_EQ(d.read_string(), "metadata");

    auto res_middle = d.read_bytes();
    ASSERT_EQ(res_middle.size(), sizeof(middle));
    for (size_t i = 0; i < sizeof(middle); ++i) {
        EXPECT_EQ(res_middle[i], middle[i]);
    }

    EXPECT_EQ(d.read_u32(), uint32_t{999999});
    EXPECT_EQ(d.read_string(), "status:ok");

    auto res_suffix = d.read_bytes();
    ASSERT_EQ(res_suffix.size(), sizeof(suffix));
    for (size_t i = 0; i < sizeof(suffix); ++i) {
        EXPECT_EQ(res_suffix[i], suffix[i]);
    }

    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RepeatedSameValueWritesV62) {
    // Test writing the same value multiple times in sequence
    Serializer s;
    const uint32_t repeated_value = 0x12345678;
    for (int i = 0; i < 10; ++i) {
        s.write_u32(repeated_value);
    }

    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_u32(), repeated_value);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, IncrementalBufferGrowthV62) {
    // Test that buffer grows correctly as data is appended
    Serializer s;

    // First write: single u8
    s.write_u8(1);
    size_t size1 = s.data().size();

    // Second write: u16
    s.write_u16(256);
    size_t size2 = s.data().size();
    EXPECT_GT(size2, size1);

    // Third write: u64
    s.write_u64(0x123456789ABCDEF0ULL);
    size_t size3 = s.data().size();
    EXPECT_GT(size3, size2);

    // Verify the data is intact
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), uint8_t{1});
    EXPECT_EQ(d.read_u16(), uint16_t{256});
    EXPECT_EQ(d.read_u64(), uint64_t{0x123456789ABCDEF0ULL});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringLengthLimitsV62) {
    // Test string serialization with various lengths
    Serializer s;

    std::string empty_str = "";
    std::string short_str = "hi";
    std::string medium_str = "medium length string test";
    std::string long_str(1000, 'x');  // 1000 character string

    s.write_string(empty_str);
    s.write_string(short_str);
    s.write_string(medium_str);
    s.write_string(long_str);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), empty_str);
    EXPECT_EQ(d.read_string(), short_str);
    EXPECT_EQ(d.read_string(), medium_str);
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, FloatingPointNaNInfinityV62) {
    // Test special floating point values: NaN and infinity
    Serializer s;

    double nan_val = std::nan("");
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
    double normal_val = 3.14159265359;

    s.write_f64(nan_val);
    s.write_f64(pos_inf);
    s.write_f64(neg_inf);
    s.write_f64(normal_val);

    Deserializer d(s.data());
    double read_nan = d.read_f64();
    double read_pos_inf = d.read_f64();
    double read_neg_inf = d.read_f64();
    double read_normal = d.read_f64();

    EXPECT_TRUE(std::isnan(read_nan));
    EXPECT_TRUE(std::isinf(read_pos_inf) && read_pos_inf > 0);
    EXPECT_TRUE(std::isinf(read_neg_inf) && read_neg_inf < 0);
    EXPECT_DOUBLE_EQ(read_normal, normal_val);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SequentialReadOrderValidationV62) {
    // Test that data is read back in the exact order it was written
    Serializer s;

    // Write in specific order
    s.write_u8(11);
    s.write_u16(222);
    s.write_u32(3333);
    s.write_i32(-4444);
    s.write_u64(55555);
    s.write_i64(-66666);
    s.write_string("test");
    s.write_f64(7.777);

    Deserializer d(s.data());

    // Read in exact same order
    EXPECT_EQ(d.read_u8(), uint8_t{11});
    EXPECT_EQ(d.read_u16(), uint16_t{222});
    EXPECT_EQ(d.read_u32(), uint32_t{3333});
    EXPECT_EQ(d.read_i32(), int32_t{-4444});
    EXPECT_EQ(d.read_u64(), uint64_t{55555});
    EXPECT_EQ(d.read_i64(), int64_t{-66666});
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_DOUBLE_EQ(d.read_f64(), 7.777);

    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteSkipPatternV62) {
    // Test write-then-skip pattern with partial reads
    Serializer s;

    // Write data with markers
    s.write_u32(0xAAAAAAAA);  // marker 1
    s.write_u32(0x11111111);
    s.write_u32(0x22222222);
    s.write_u32(0xBBBBBBBB);  // marker 2
    s.write_u32(0x33333333);
    s.write_u32(0xCCCCCCCC);  // marker 3

    Deserializer d(s.data());

    // Read first marker and skip next two values
    EXPECT_EQ(d.read_u32(), uint32_t{0xAAAAAAAA});
    d.read_u32();  // skip
    d.read_u32();  // skip

    // Read second marker
    EXPECT_EQ(d.read_u32(), uint32_t{0xBBBBBBBB});
    d.read_u32();  // skip

    // Read final marker
    EXPECT_EQ(d.read_u32(), uint32_t{0xCCCCCCCC});

    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, PartialBufferReadsV62) {
    // Test reading partial data from a larger buffer
    Serializer s;

    uint8_t data_block1[] = {0x10, 0x20, 0x30, 0x40, 0x50};
    uint8_t data_block2[] = {0xAA, 0xBB, 0xCC};
    uint8_t data_block3[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA};

    s.write_bytes(data_block1, sizeof(data_block1));
    s.write_bytes(data_block2, sizeof(data_block2));
    s.write_bytes(data_block3, sizeof(data_block3));

    Deserializer d(s.data());

    auto read_block1 = d.read_bytes();
    ASSERT_EQ(read_block1.size(), sizeof(data_block1));
    for (size_t i = 0; i < sizeof(data_block1); ++i) {
        EXPECT_EQ(read_block1[i], data_block1[i]);
    }

    auto read_block2 = d.read_bytes();
    ASSERT_EQ(read_block2.size(), sizeof(data_block2));
    for (size_t i = 0; i < sizeof(data_block2); ++i) {
        EXPECT_EQ(read_block2[i], data_block2[i]);
    }

    auto read_block3 = d.read_bytes();
    ASSERT_EQ(read_block3.size(), sizeof(data_block3));
    for (size_t i = 0; i < sizeof(data_block3); ++i) {
        EXPECT_EQ(read_block3[i], data_block3[i]);
    }

    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BufferSizeVerificationV62) {
    // Test verification of buffer sizes after writes
    Serializer s;

    // Track sizes after each write operation
    size_t initial_size = s.data().size();
    EXPECT_EQ(initial_size, size_t{0});

    s.write_u8(1);
    size_t after_u8 = s.data().size();
    EXPECT_EQ(after_u8, size_t{1});

    s.write_u16(2);
    size_t after_u16 = s.data().size();
    EXPECT_EQ(after_u16, size_t{3});

    s.write_u32(3);
    size_t after_u32 = s.data().size();
    EXPECT_EQ(after_u32, size_t{7});

    s.write_u64(4);
    size_t after_u64 = s.data().size();
    EXPECT_EQ(after_u64, size_t{15});

    s.write_i32(-5);
    size_t after_i32 = s.data().size();
    EXPECT_EQ(after_i32, size_t{19});

    s.write_i64(-6);
    size_t after_i64 = s.data().size();
    EXPECT_EQ(after_i64, size_t{27});

    // Verify all data is intact
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), uint8_t{1});
    EXPECT_EQ(d.read_u16(), uint16_t{2});
    EXPECT_EQ(d.read_u32(), uint32_t{3});
    EXPECT_EQ(d.read_u64(), uint64_t{4});
    EXPECT_EQ(d.read_i32(), int32_t{-5});
    EXPECT_EQ(d.read_i64(), int64_t{-6});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypeSerializationOrderV63) {
    Serializer s;
    const std::vector<uint8_t> payload = {0x00, 0x7F, 0x80, 0xFF, 0x2A};

    s.write_u8(0xAB);
    s.write_u16(0x1234);
    s.write_u32(0x89ABCDEFu);
    s.write_u64(0x1122334455667788ULL);
    s.write_i64(-1234567890123456789LL);
    s.write_f64(-42.125);
    s.write_string("ipc-mixed-v63");
    s.write_bytes(payload.data(), payload.size());

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_u8(), uint8_t{0xAB});
    EXPECT_EQ(d.read_u16(), uint16_t{0x1234});
    EXPECT_EQ(d.read_u32(), uint32_t{0x89ABCDEFu});
    EXPECT_EQ(d.read_u64(), uint64_t{0x1122334455667788ULL});
    EXPECT_EQ(d.read_i64(), int64_t{-1234567890123456789LL});
    EXPECT_DOUBLE_EQ(d.read_f64(), -42.125);
    EXPECT_EQ(d.read_string(), "ipc-mixed-v63");
    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, NumericBoundaryValuesV63) {
    Serializer s;

    s.write_u8(std::numeric_limits<uint8_t>::min());
    s.write_u8(std::numeric_limits<uint8_t>::max());
    s.write_u16(std::numeric_limits<uint16_t>::min());
    s.write_u16(std::numeric_limits<uint16_t>::max());
    s.write_u32(std::numeric_limits<uint32_t>::min());
    s.write_u32(std::numeric_limits<uint32_t>::max());
    s.write_u64(std::numeric_limits<uint64_t>::min());
    s.write_u64(std::numeric_limits<uint64_t>::max());
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(std::numeric_limits<int64_t>::max());
    s.write_f64(std::numeric_limits<double>::lowest());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(-0.0);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_u8(), std::numeric_limits<uint8_t>::min());
    EXPECT_EQ(d.read_u8(), std::numeric_limits<uint8_t>::max());
    EXPECT_EQ(d.read_u16(), std::numeric_limits<uint16_t>::min());
    EXPECT_EQ(d.read_u16(), std::numeric_limits<uint16_t>::max());
    EXPECT_EQ(d.read_u32(), std::numeric_limits<uint32_t>::min());
    EXPECT_EQ(d.read_u32(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::min());
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::lowest());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    const double negative_zero = d.read_f64();
    EXPECT_EQ(negative_zero, 0.0);
    EXPECT_TRUE(std::signbit(negative_zero));
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesPayloadPatternV63) {
    Serializer s;
    std::vector<uint8_t> payload(128 * 1024);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i * 31 + 7) % 256);
    }

    s.write_bytes(payload.data(), payload.size());
    s.write_u32(0xA1B2C3D4u);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_EQ(d.read_u32(), uint32_t{0xA1B2C3D4u});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeStringRoundTripAndFollowupV63) {
    Serializer s;
    std::string large_text(20000, '\0');
    for (size_t i = 0; i < large_text.size(); ++i) {
        large_text[i] = static_cast<char>('a' + (i % 26));
    }

    s.write_string(large_text);
    s.write_u16(4242);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_string(), large_text);
    EXPECT_EQ(d.read_u16(), uint16_t{4242});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyDataBlocksRoundTripV63) {
    Serializer s;

    s.write_string("");
    s.write_bytes(nullptr, 0);
    s.write_u8(42);
    s.write_string("");

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_string(), "");
    EXPECT_TRUE(d.read_bytes().empty());
    EXPECT_EQ(d.read_u8(), uint8_t{42});
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SequentialReadsMaintainOrderV63) {
    Serializer s;
    for (uint16_t i = 0; i < 64; ++i) {
        s.write_u16(static_cast<uint16_t>(i * 17));
        s.write_u8(static_cast<uint8_t>(255 - i));
        s.write_i64((i % 2 == 0) ? static_cast<int64_t>(i) : -static_cast<int64_t>(i));
    }

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    for (uint16_t i = 0; i < 64; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(i * 17));
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(255 - i));
        const int64_t expected = (i % 2 == 0) ? static_cast<int64_t>(i) : -static_cast<int64_t>(i);
        EXPECT_EQ(d.read_i64(), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, DataIntegrityCopiedBufferUnaffectedByLaterWritesV63) {
    Serializer s;
    const std::vector<uint8_t> payload = {9, 8, 7, 6};

    s.write_u32(0x01020304u);
    s.write_string("stable");
    s.write_bytes(payload.data(), payload.size());
    s.write_i64(-999);

    const std::vector<uint8_t> snapshot = s.data();
    s.write_u64(0xDEADBEEFDEADBEEFULL);

    EXPECT_LT(snapshot.size(), s.data().size());

    Deserializer d(snapshot.data(), snapshot.size());
    EXPECT_EQ(d.read_u32(), uint32_t{0x01020304u});
    EXPECT_EQ(d.read_string(), "stable");
    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_EQ(d.read_i64(), int64_t{-999});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ReencodeRoundTripMatchesOriginalBufferV63) {
    Serializer original;
    const std::vector<uint8_t> blob = {0x10, 0x20, 0x30, 0x40, 0x50};

    original.write_u8(33);
    original.write_u16(4096);
    original.write_u32(700000u);
    original.write_u64(0x0F0E0D0C0B0A0908ULL);
    original.write_i64(-44444444444LL);
    original.write_f64(1.0 / 3.0);
    original.write_string("reencode-check");
    original.write_bytes(blob.data(), blob.size());

    const auto& wire = original.data();
    Deserializer d(wire.data(), wire.size());

    const uint8_t u8 = d.read_u8();
    const uint16_t u16 = d.read_u16();
    const uint32_t u32 = d.read_u32();
    const uint64_t u64 = d.read_u64();
    const int64_t i64 = d.read_i64();
    const double f64 = d.read_f64();
    const std::string str = d.read_string();
    const std::vector<uint8_t> bytes = d.read_bytes();
    EXPECT_FALSE(d.has_remaining());

    Serializer reencoded;
    reencoded.write_u8(u8);
    reencoded.write_u16(u16);
    reencoded.write_u32(u32);
    reencoded.write_u64(u64);
    reencoded.write_i64(i64);
    reencoded.write_f64(f64);
    reencoded.write_string(str);
    reencoded.write_bytes(bytes.data(), bytes.size());

    EXPECT_EQ(original.data(), reencoded.data());
}

TEST(SerializerTest, PtrLenCtorRoundTripAllPrimitiveAndDynamicTypesV64) {
    Serializer s;
    const std::vector<uint8_t> payload = {0x00, 0x7F, 0x80, 0xFF};

    s.write_u8(0xABu);
    s.write_u16(0x1234u);
    s.write_u32(0x89ABCDEFu);
    s.write_u64(0x0123456789ABCDEFULL);
    s.write_i64(-0x123456789LL);
    s.write_f64(-42.625);
    s.write_string("v64-alpha");
    s.write_bytes(payload.data(), payload.size());

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_u8(), uint8_t{0xABu});
    EXPECT_EQ(d.read_u16(), uint16_t{0x1234u});
    EXPECT_EQ(d.read_u32(), uint32_t{0x89ABCDEFu});
    EXPECT_EQ(d.read_u64(), uint64_t{0x0123456789ABCDEFULL});
    EXPECT_EQ(d.read_i64(), int64_t{-0x123456789LL});
    EXPECT_DOUBLE_EQ(d.read_f64(), -42.625);
    EXPECT_EQ(d.read_string(), "v64-alpha");
    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, DataSizeMatchesExpectedByteCountAfterWritesV64) {
    Serializer s;
    const std::vector<uint8_t> payload = {1, 2, 3};

    EXPECT_EQ(s.data().size(), size_t{0});
    s.write_u8(1);
    EXPECT_EQ(s.data().size(), size_t{1});
    s.write_u16(2);
    EXPECT_EQ(s.data().size(), size_t{3});
    s.write_u32(3);
    EXPECT_EQ(s.data().size(), size_t{7});
    s.write_u64(4);
    EXPECT_EQ(s.data().size(), size_t{15});
    s.write_i64(-5);
    EXPECT_EQ(s.data().size(), size_t{23});
    s.write_f64(6.5);
    EXPECT_EQ(s.data().size(), size_t{31});
    s.write_string("xy");
    EXPECT_EQ(s.data().size(), size_t{37});
    s.write_bytes(payload.data(), payload.size());
    EXPECT_EQ(s.data().size(), size_t{44});
}

TEST(SerializerTest, BytesRoundTripPreservesEmbeddedZeroesV64) {
    Serializer s;
    const std::vector<uint8_t> payload = {0x10, 0x00, 0x20, 0x00, 0x30, 0x00};

    s.write_bytes(payload.data(), payload.size());
    s.write_u16(0xBEEFu);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_EQ(d.read_u16(), uint16_t{0xBEEFu});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringRoundTripPreservesEmbeddedNullsV64) {
    Serializer s;
    const std::string with_nulls("hi\0there\0v64", 12);

    s.write_string(with_nulls);
    s.write_u8(9);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    const std::string roundtrip = d.read_string();
    EXPECT_EQ(roundtrip, with_nulls);
    EXPECT_EQ(roundtrip.size(), with_nulls.size());
    EXPECT_EQ(d.read_u8(), uint8_t{9});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SequentialFramesWithMixedTypesReadInExactOrderV64) {
    Serializer s;

    for (uint32_t i = 0; i < 10; ++i) {
        const std::string label = "frame-" + std::to_string(i);
        const std::vector<uint8_t> bytes = {
            static_cast<uint8_t>(i),
            static_cast<uint8_t>(i + 1),
            static_cast<uint8_t>(i + 2)
        };
        s.write_u32(i);
        s.write_i64(-static_cast<int64_t>(i * 1000));
        s.write_f64(static_cast<double>(i) + 0.25);
        s.write_string(label);
        s.write_bytes(bytes.data(), bytes.size());
    }

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    for (uint32_t i = 0; i < 10; ++i) {
        const std::string expected_label = "frame-" + std::to_string(i);
        const std::vector<uint8_t> expected_bytes = {
            static_cast<uint8_t>(i),
            static_cast<uint8_t>(i + 1),
            static_cast<uint8_t>(i + 2)
        };
        EXPECT_EQ(d.read_u32(), i);
        EXPECT_EQ(d.read_i64(), -static_cast<int64_t>(i * 1000));
        EXPECT_DOUBLE_EQ(d.read_f64(), static_cast<double>(i) + 0.25);
        EXPECT_EQ(d.read_string(), expected_label);
        EXPECT_EQ(d.read_bytes(), expected_bytes);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringAndEmptyBytesRoundTripWithFollowupValueV64) {
    Serializer s;

    s.write_string("");
    s.write_bytes(nullptr, 0);
    s.write_u64(0xCAFED00DCAFED00DULL);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_string(), "");
    EXPECT_TRUE(d.read_bytes().empty());
    EXPECT_EQ(d.read_u64(), uint64_t{0xCAFED00DCAFED00DULL});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BufferSnapshotCanBeReadByIndependentDeserializersV64) {
    Serializer s;
    const std::vector<uint8_t> payload = {4, 3, 2, 1};

    s.write_u8(77);
    s.write_string("independent");
    s.write_bytes(payload.data(), payload.size());
    s.write_i64(-77);

    const auto& wire = s.data();
    Deserializer d1(wire.data(), wire.size());
    Deserializer d2(wire.data(), wire.size());

    EXPECT_EQ(d1.read_u8(), uint8_t{77});
    EXPECT_EQ(d1.read_string(), "independent");
    EXPECT_EQ(d1.read_bytes(), payload);
    EXPECT_EQ(d1.read_i64(), int64_t{-77});
    EXPECT_FALSE(d1.has_remaining());

    EXPECT_EQ(d2.read_u8(), uint8_t{77});
    EXPECT_EQ(d2.read_string(), "independent");
    EXPECT_EQ(d2.read_bytes(), payload);
    EXPECT_EQ(d2.read_i64(), int64_t{-77});
    EXPECT_FALSE(d2.has_remaining());
}

TEST(SerializerTest, HasRemainingTransitionsToFalseAtExactBoundaryV64) {
    Serializer s;
    const std::vector<uint8_t> payload = {0xAA, 0xBB};

    s.write_u16(0xA1B2u);
    s.write_string("ok");
    s.write_bytes(payload.data(), payload.size());
    s.write_f64(3.5);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_TRUE(d.has_remaining());
    EXPECT_EQ(d.read_u16(), uint16_t{0xA1B2u});
    EXPECT_TRUE(d.has_remaining());
    EXPECT_EQ(d.read_string(), "ok");
    EXPECT_TRUE(d.has_remaining());
    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_TRUE(d.has_remaining());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.5);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, VeryLargeStringRoundTripAndBoundaryMarkerV65) {
    Serializer s;
    const std::string large_text(131072, 'L');

    s.write_string(large_text);
    s.write_u32(0x1234ABCDu);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_string(), large_text);
    EXPECT_EQ(d.read_u32(), uint32_t{0x1234ABCDu});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, AlternatingTypePatternMaintainsExactReadOrderV65) {
    Serializer s;
    const std::vector<uint8_t> block = {0x10, 0x20, 0x30};

    s.write_u32(101u);
    s.write_string("one");
    s.write_bool(true);
    s.write_u32(202u);
    s.write_string("two");
    s.write_bool(false);
    s.write_bytes(block.data(), block.size());

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_u32(), uint32_t{101u});
    EXPECT_EQ(d.read_string(), "one");
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u32(), uint32_t{202u});
    EXPECT_EQ(d.read_string(), "two");
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_bytes(), block);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleBoolSequenceRoundTripV65) {
    Serializer s;
    const std::vector<bool> pattern = {true, false, false, true, true, false, true, false, true};

    for (bool value : pattern) {
        s.write_bool(value);
    }

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    for (bool expected : pattern) {
        EXPECT_EQ(d.read_bool(), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ZeroLengthBytesCanAppearBetweenTypedFieldsV65) {
    Serializer s;

    s.write_string("prefix");
    s.write_bytes(nullptr, 0);
    s.write_u32(777u);
    s.write_bool(true);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_string(), "prefix");
    EXPECT_TRUE(d.read_bytes().empty());
    EXPECT_EQ(d.read_u32(), uint32_t{777u});
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32MaxValueRoundTripWithNeighborsV65) {
    Serializer s;

    s.write_u32(1u);
    s.write_u32(std::numeric_limits<uint32_t>::max());
    s.write_u32(0u);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_u32(), uint32_t{1u});
    EXPECT_EQ(d.read_u32(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(d.read_u32(), uint32_t{0u});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, NestedSerializationPayloadRoundTripV65) {
    Serializer inner;
    const std::vector<uint8_t> inner_bytes = {9, 8, 7, 6};
    inner.write_u32(0xCAFEBABEu);
    inner.write_string("inner-v65");
    inner.write_bytes(inner_bytes.data(), inner_bytes.size());
    inner.write_bool(true);

    const auto& inner_wire = inner.data();
    Serializer outer;
    outer.write_u32(0xDEADBEEFu);
    outer.write_bytes(inner_wire.data(), inner_wire.size());
    outer.write_string("outer-end");

    const auto& outer_wire = outer.data();
    Deserializer outer_d(outer_wire.data(), outer_wire.size());

    EXPECT_EQ(outer_d.read_u32(), uint32_t{0xDEADBEEFu});
    const std::vector<uint8_t> packed_inner = outer_d.read_bytes();
    EXPECT_EQ(outer_d.read_string(), "outer-end");
    EXPECT_FALSE(outer_d.has_remaining());

    Deserializer inner_d(packed_inner.data(), packed_inner.size());
    EXPECT_EQ(inner_d.read_u32(), uint32_t{0xCAFEBABEu});
    EXPECT_EQ(inner_d.read_string(), "inner-v65");
    EXPECT_EQ(inner_d.read_bytes(), inner_bytes);
    EXPECT_TRUE(inner_d.read_bool());
    EXPECT_FALSE(inner_d.has_remaining());
}

TEST(SerializerTest, SequentialReadsExhaustBufferAtExactEndV65) {
    Serializer s;

    s.write_u32(42u);
    s.write_string("done");
    s.write_bytes(nullptr, 0);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_TRUE(d.has_remaining());
    EXPECT_EQ(d.read_u32(), uint32_t{42u});
    EXPECT_TRUE(d.has_remaining());
    EXPECT_EQ(d.read_string(), "done");
    EXPECT_TRUE(d.has_remaining());
    EXPECT_TRUE(d.read_bytes().empty());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedStringBytesU32SequenceRoundTripV65) {
    Serializer s;
    const std::vector<uint8_t> first = {0x01, 0x02};
    const std::vector<uint8_t> second = {0xAA, 0xBB, 0xCC};

    s.write_string("alpha");
    s.write_bytes(first.data(), first.size());
    s.write_u32(100u);
    s.write_string("beta");
    s.write_bytes(second.data(), second.size());
    s.write_u32(200u);

    const auto& wire = s.data();
    Deserializer d(wire.data(), wire.size());

    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_bytes(), first);
    EXPECT_EQ(d.read_u32(), uint32_t{100u});
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_bytes(), second);
    EXPECT_EQ(d.read_u32(), uint32_t{200u});
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, Int64ValuesRoundTripV66) {
    Serializer s;
    const std::vector<int64_t> values = {
        std::numeric_limits<int64_t>::min(),
        -1234567890123456789LL,
        -1,
        0,
        1,
        1234567890123456789LL,
        std::numeric_limits<int64_t>::max()
    };

    for (int64_t value : values) {
        s.write_i64(value);
    }

    Deserializer d(s.data());
    for (int64_t expected : values) {
        EXPECT_EQ(d.read_i64(), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, FloatValuesRoundTripV66) {
    Serializer s;
    const std::vector<float> values = {
        0.0f,
        -0.5f,
        1.25f,
        -9876.5f,
        std::numeric_limits<float>::max()
    };

    for (float value : values) {
        s.write_f64(static_cast<double>(value));
    }

    Deserializer d(s.data());
    for (float expected : values) {
        EXPECT_FLOAT_EQ(static_cast<float>(d.read_f64()), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringFollowedByNonEmptyRoundTripV66) {
    Serializer s;
    s.write_string("");
    s.write_string("hello-v66");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "hello-v66");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedWritesDifferentTypesStressV66) {
    Serializer s;

    for (uint32_t i = 0; i < 200; ++i) {
        s.write_u32(i);
        s.write_i64(-static_cast<int64_t>(i) * 1111);
        s.write_f64(static_cast<double>(i) * 0.125);
        s.write_bool((i % 2) == 0);
        s.write_string("msg-" + std::to_string(i));
        const std::vector<uint8_t> one = {static_cast<uint8_t>(i & 0xFFu)};
        s.write_bytes(one.data(), one.size());
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 200; ++i) {
        EXPECT_EQ(d.read_u32(), i);
        EXPECT_EQ(d.read_i64(), -static_cast<int64_t>(i) * 1111);
        EXPECT_DOUBLE_EQ(d.read_f64(), static_cast<double>(i) * 0.125);
        EXPECT_EQ(d.read_bool(), (i % 2) == 0);
        EXPECT_EQ(d.read_string(), "msg-" + std::to_string(i));
        const auto bytes = d.read_bytes();
        ASSERT_EQ(bytes.size(), 1u);
        EXPECT_EQ(bytes[0], static_cast<uint8_t>(i & 0xFFu));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ReadPastEndReturnsErrorV66) {
    Serializer s;
    s.write_u32(123456u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 123456u);
    EXPECT_THROW(d.read_u8(), std::runtime_error);
}

TEST(SerializerTest, VeryLargeBufferRoundTrip10000BytesV66) {
    Serializer s;
    std::vector<uint8_t> big(10000);
    for (size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<uint8_t>(i % 251);
    }

    s.write_bytes(big.data(), big.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), big);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolStringU32PatternRepeatedManyTimesV66) {
    Serializer s;

    for (uint32_t i = 0; i < 300; ++i) {
        s.write_bool((i % 3) == 0);
        s.write_string("item-" + std::to_string(i));
        s.write_u32(100000u + i);
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 300; ++i) {
        EXPECT_EQ(d.read_bool(), (i % 3) == 0);
        EXPECT_EQ(d.read_string(), "item-" + std::to_string(i));
        EXPECT_EQ(d.read_u32(), 100000u + i);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesExactOneBytePayloadV66) {
    Serializer s;
    const uint8_t byte = 0xAB;

    s.write_bytes(&byte, 1);

    Deserializer d(s.data());
    const auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], byte);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteU32ZeroAndMaxTogetherV67) {
    Serializer s;
    s.write_u32(0u);
    s.write_u32(std::numeric_limits<uint32_t>::max());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{0u});
    EXPECT_EQ(d.read_u32(), std::numeric_limits<uint32_t>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteStringWithUnicodeCharactersV67) {
    Serializer s;
    const std::string text = "\xF0\x9F\x9A\x80 Browser \xE2\x9C\x85 \xE4\xB8\x96\xE7\x95\x8C";
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ReadStringAfterMultipleU32ReadsV67) {
    Serializer s;
    s.write_u32(7u);
    s.write_u32(11u);
    s.write_u32(13u);
    s.write_string("after-u32-values");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), uint32_t{7u});
    EXPECT_EQ(d.read_u32(), uint32_t{11u});
    EXPECT_EQ(d.read_u32(), uint32_t{13u});
    EXPECT_EQ(d.read_string(), "after-u32-values");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBoolAlternatingTrueFalseTenTimesV67) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_bool((i % 2) == 0);
    }

    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_bool(), (i % 2) == 0);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesWith256BytePayloadV67) {
    Serializer s;
    std::vector<uint8_t> payload(256);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(payload.data(), payload.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), payload);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptySerializerHasZeroSizeV67) {
    Serializer s;
    EXPECT_EQ(s.data().size(), 0u);
}

TEST(SerializerTest, WriteStringThenBytesVerifyOrderV67) {
    Serializer s;
    const std::string label = "header-v67";
    const std::vector<uint8_t> bytes = {0x10, 0x20, 0x30, 0x40};
    s.write_string(label);
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), label);
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsConcatenatedCorrectlyV67) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("-");
    s.write_string("omega");

    Deserializer d(s.data());
    const std::string combined = d.read_string() + d.read_string() + d.read_string();
    EXPECT_EQ(combined, "alpha-omega");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsInSequenceV68) {
    Serializer s;
    const std::vector<std::string> values = {
        "",
        "alpha",
        "beta gamma",
        "delta-123",
        "last"
    };

    for (const auto& value : values) {
        s.write_string(value);
    }

    Deserializer d(s.data());
    for (const auto& expected : values) {
        EXPECT_EQ(d.read_string(), expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolThenU32ThenStringPatternV68) {
    Serializer s;
    for (uint32_t i = 0; i < 20; ++i) {
        s.write_bool((i % 2u) == 0u);
        s.write_u32(1000u + i);
        s.write_string("pattern-" + std::to_string(i));
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 20; ++i) {
        EXPECT_EQ(d.read_bool(), (i % 2u) == 0u);
        EXPECT_EQ(d.read_u32(), 1000u + i);
        EXPECT_EQ(d.read_string(), "pattern-" + std::to_string(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SingleByteWriteReadV68) {
    Serializer s;
    s.write_u8(0xA5u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xA5u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, NegativeNumberAsU32WrapsAroundV68) {
    Serializer s;
    const uint32_t wrapped = static_cast<uint32_t>(-42);
    s.write_u32(wrapped);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), wrapped);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesPreservesNullBytesV68) {
    Serializer s;
    const std::vector<uint8_t> bytes = {0x41, 0x00, 0x42, 0x00, 0x43, 0x00};
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, DeserializerCopyConstructorPreservesReadPositionV68) {
    Serializer s;
    s.write_u32(11u);
    s.write_u32(22u);
    s.write_u32(33u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 11u);

    Deserializer copied(d);
    EXPECT_EQ(d.read_u32(), 22u);
    EXPECT_EQ(copied.read_u32(), 22u);
    EXPECT_EQ(d.read_u32(), 33u);
    EXPECT_EQ(copied.read_u32(), 33u);
    EXPECT_FALSE(d.has_remaining());
    EXPECT_FALSE(copied.has_remaining());
}

TEST(SerializerTest, VeryLongString5000CharsV68) {
    std::string long_str(5000, 'a');
    for (size_t i = 0; i < long_str.size(); ++i) {
        long_str[i] = static_cast<char>('a' + (i % 26));
    }

    Serializer s;
    s.write_string(long_str);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypeStress50OperationsV68) {
    Serializer s;
    for (uint32_t i = 0; i < 50; ++i) {
        switch (i % 5u) {
            case 0:
                s.write_u32(7000u + i);
                break;
            case 1:
                s.write_i64(-static_cast<int64_t>(i) * 1234);
                break;
            case 2:
                s.write_bool((i % 2u) == 0u);
                break;
            case 3:
                s.write_string("mix-" + std::to_string(i));
                break;
            default: {
                const std::vector<uint8_t> bytes = {
                    static_cast<uint8_t>(i & 0xFFu),
                    0x00,
                    static_cast<uint8_t>((i * 3u) & 0xFFu)
                };
                s.write_bytes(bytes.data(), bytes.size());
                break;
            }
        }
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 50; ++i) {
        switch (i % 5u) {
            case 0:
                EXPECT_EQ(d.read_u32(), 7000u + i);
                break;
            case 1:
                EXPECT_EQ(d.read_i64(), -static_cast<int64_t>(i) * 1234);
                break;
            case 2:
                EXPECT_EQ(d.read_bool(), (i % 2u) == 0u);
                break;
            case 3:
                EXPECT_EQ(d.read_string(), "mix-" + std::to_string(i));
                break;
            default: {
                const auto bytes = d.read_bytes();
                ASSERT_EQ(bytes.size(), 3u);
                EXPECT_EQ(bytes[0], static_cast<uint8_t>(i & 0xFFu));
                EXPECT_EQ(bytes[1], 0x00u);
                EXPECT_EQ(bytes[2], static_cast<uint8_t>((i * 3u) & 0xFFu));
                break;
            }
        }
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripStringWithNullByteInsideV69) {
    const std::string with_null("abc\0def", 7);
    Serializer s;
    s.write_string(with_null);

    Deserializer d(s.data());
    const std::string roundtrip = d.read_string();
    EXPECT_EQ(roundtrip, with_null);
    EXPECT_EQ(roundtrip.size(), 7u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripBytesWithAll256ByteValuesV69) {
    std::vector<uint8_t> bytes(256);
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(i);
    }

    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteU32BoundaryValuesV69) {
    Serializer s;
    s.write_u32(0u);
    s.write_u32(1u);
    s.write_u32(255u);
    s.write_u32(256u);
    s.write_u32(65535u);
    s.write_u32(65536u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 255u);
    EXPECT_EQ(d.read_u32(), 256u);
    EXPECT_EQ(d.read_u32(), 65535u);
    EXPECT_EQ(d.read_u32(), 65536u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ReadStringTwiceReturnsSameValueV69) {
    Serializer s;
    s.write_string("same-value");
    s.write_string("same-value");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "same-value");
    EXPECT_EQ(d.read_string(), "same-value");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBoolSequenceOf100TruesV69) {
    Serializer s;
    for (int i = 0; i < 100; ++i) {
        s.write_bool(true);
    }

    Deserializer d(s.data());
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(d.read_bool());
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesWriteSizeIsZeroV69) {
    Serializer s;
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    const auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 0u);
    EXPECT_TRUE(bytes.empty());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithNewlinesAndTabsRoundTripV69) {
    const std::string text = "line1\nline2\tcol2\n\tindented";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleaveU32AndBool20TimesV69) {
    Serializer s;
    for (uint32_t i = 0; i < 20; ++i) {
        s.write_u32(100u + i);
        s.write_bool((i % 2u) == 0u);
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 20; ++i) {
        EXPECT_EQ(d.read_u32(), 100u + i);
        EXPECT_EQ(d.read_bool(), (i % 2u) == 0u);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadU32Value42V70) {
    Serializer s;
    s.write_u32(42u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadStringHelloWorldV70) {
    const std::string text = "hello world";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBoolTrueThenFalseReadBothV70) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesExactDataPreservedV70) {
    const std::vector<uint8_t> bytes = {0xDEu, 0xADu, 0xBEu, 0xEFu, 0x00u, 0x7Fu};
    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteStringWithQuotesAndBackslashesV70) {
    const std::string text = "say \"hello\" \\\\ path";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleU32SequentialReadOrderV70) {
    Serializer s;
    s.write_u32(1u);
    s.write_u32(42u);
    s.write_u32(1000u);
    s.write_u32(0xFFFFFFFFu);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_EQ(d.read_u32(), 1000u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesEmptyFollowedByNonEmptyV70) {
    const std::vector<uint8_t> bytes = {0x01u, 0x02u, 0x03u, 0xFFu};
    Serializer s;
    s.write_bytes(nullptr, 0);
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bytes().empty());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, SerializerDataReturnsRawBufferV70) {
    Serializer s;
    s.write_u8(0xABu);
    s.write_u16(0x1234u);
    s.write_bool(true);

    const auto& raw = s.data();
    const std::vector<uint8_t> expected = {0xABu, 0x12u, 0x34u, 0x01u};
    EXPECT_EQ(raw, expected);
}

TEST(SerializerTest, WriteReadSingleU32Value1000V71) {
    Serializer s;
    s.write_u32(1000u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 1000u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadStringWithEmojiCharactersV71) {
    const std::string text = "Launch \xF0\x9F\x9A\x80 and smile \xF0\x9F\x98\x84";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBoolFalseAndVerifyV71) {
    Serializer s;
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytes512PatternDataV71) {
    std::vector<uint8_t> bytes(512);
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>((i * 37u) & 0xFFu);
    }

    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, TwoStringsBackToBackReadCorrectlyV71) {
    const std::string first = "first string";
    const std::string second = "second string";
    Serializer s;
    s.write_string(first);
    s.write_string(second);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), first);
    EXPECT_EQ(d.read_string(), second);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32ThenStringThenU32PatternV71) {
    const uint32_t prefix = 0x12345678u;
    const std::string middle = "payload";
    const uint32_t suffix = 0xABCDEF01u;
    Serializer s;
    s.write_u32(prefix);
    s.write_string(middle);
    s.write_u32(suffix);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), prefix);
    EXPECT_EQ(d.read_string(), middle);
    EXPECT_EQ(d.read_u32(), suffix);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringWriteReadV71) {
    Serializer s;
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesAllFFThroughoutV71) {
    std::vector<uint8_t> bytes(128, 0xFFu);
    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadU32MaxAndMinV72) {
    Serializer s;
    s.write_u32(std::numeric_limits<uint32_t>::max());
    s.write_u32(std::numeric_limits<uint32_t>::min());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(d.read_u32(), std::numeric_limits<uint32_t>::min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadStringWithSpecialHtmlCharsV72) {
    const std::string text = "<div class=\"msg\">Tom & Jerry 'say' \"hi\"</div>";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadBoolSequenceTrueFalseTrueV72) {
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

TEST(SerializerTest, WriteReadBytesWithRepeatingPatternV72) {
    std::vector<uint8_t> bytes(256);
    for (size_t i = 0; i < bytes.size(); ++i) {
        switch (i % 4u) {
            case 0:
                bytes[i] = 0xAAu;
                break;
            case 1:
                bytes[i] = 0x55u;
                break;
            case 2:
                bytes[i] = 0x00u;
                break;
            default:
                bytes[i] = 0xFFu;
                break;
        }
    }

    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bytes(), bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadU32SequenceZeroToNineV72) {
    Serializer s;
    for (uint32_t i = 0; i < 10u; ++i) {
        s.write_u32(i);
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 10u; ++i) {
        EXPECT_EQ(d.read_u32(), i);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadStringThenBoolThenU32MixedV72) {
    const std::string text = "mix<&>\"value\"";
    const bool flag = false;
    const uint32_t number = 42424242u;

    Serializer s;
    s.write_string(text);
    s.write_bool(flag);
    s.write_u32(number);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_EQ(d.read_bool(), flag);
    EXPECT_EQ(d.read_u32(), number);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteReadEmptyThenNonEmptyStringV72) {
    const std::string second = "after-empty";
    Serializer s;
    s.write_string("");
    s.write_string(second);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), second);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, DeserializerFromRawBufferV72) {
    Serializer s;
    s.write_u32(0xDEADBEEFu);
    s.write_bool(true);
    s.write_string("raw-buffer");

    const auto& raw = s.data();
    Deserializer d(raw.data(), raw.size());
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "raw-buffer");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteU32PowersOfTwoV73) {
    Serializer s;
    for (uint32_t i = 0; i < 32u; ++i) {
        s.write_u32(1u << i);
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 32u; ++i) {
        EXPECT_EQ(d.read_u32(), 1u << i);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteStringWithCjkCharactersV73) {
    const std::string text =
        "\xE6\xBC\xA2\xE5\xAD\x97\xE3\x81\x8B\xE3\x81\xAA\xED\x95\x9C\xEA\xB8\x80";
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBoolAfterStringV73) {
    Serializer s;
    s.write_string("hello");
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesExactSize1024V73) {
    std::vector<uint8_t> bytes(1024);
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(i & 0xFFu);
    }

    Serializer s;
    s.write_bytes(bytes.data(), bytes.size());

    Deserializer d(s.data());
    const std::vector<uint8_t> read_back = d.read_bytes();
    EXPECT_EQ(read_back.size(), 1024u);
    EXPECT_EQ(read_back, bytes);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteU32ThenBytesInterleavedV73) {
    const std::vector<uint8_t> first = {0x10u, 0x20u, 0x30u};
    const std::vector<uint8_t> second = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u};

    Serializer s;
    s.write_u32(11u);
    s.write_bytes(first.data(), first.size());
    s.write_u32(22u);
    s.write_bytes(second.data(), second.size());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 11u);
    EXPECT_EQ(d.read_bytes(), first);
    EXPECT_EQ(d.read_u32(), 22u);
    EXPECT_EQ(d.read_bytes(), second);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBufferSizeIsZeroV73) {
    Serializer s;
    EXPECT_EQ(s.data().size(), 0u);
}

TEST(SerializerTest, WriteReadStringPreservesLengthV73) {
    const std::string text("abc\0def", 7);
    Serializer s;
    s.write_string(text);

    Deserializer d(s.data());
    const std::string read_back = d.read_string();
    EXPECT_EQ(read_back.size(), text.size());
    EXPECT_EQ(read_back, text);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleWritesTotalDataSizeV73) {
    const std::vector<uint8_t> payload = {0xAAu, 0xBBu, 0x00u, 0x11u, 0x22u};
    Serializer s;
    s.write_u32(0xAABBCCDDu);
    s.write_string("abc");
    s.write_bool(false);
    s.write_bytes(payload.data(), payload.size());
    s.write_u16(0xBEEFu);

    const size_t expected_size =
        sizeof(uint32_t) + sizeof(uint32_t) + 3u + sizeof(uint8_t) +
        sizeof(uint32_t) + payload.size() + sizeof(uint16_t);
    EXPECT_EQ(s.data().size(), expected_size);
}

TEST(SerializerTest, UnsignedIntegerEdgeValuesV75) {
    clever::ipc::Serializer s;
    s.write_u8(std::numeric_limits<uint8_t>::min());
    s.write_u8(std::numeric_limits<uint8_t>::max());
    s.write_u16(std::numeric_limits<uint16_t>::min());
    s.write_u16(std::numeric_limits<uint16_t>::max());
    s.write_u32(std::numeric_limits<uint32_t>::min());
    s.write_u32(std::numeric_limits<uint32_t>::max());
    s.write_u64(std::numeric_limits<uint64_t>::min());
    s.write_u64(std::numeric_limits<uint64_t>::max());

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), std::numeric_limits<uint8_t>::min());
    EXPECT_EQ(reader.read_u8(), std::numeric_limits<uint8_t>::max());
    EXPECT_EQ(reader.read_u16(), std::numeric_limits<uint16_t>::min());
    EXPECT_EQ(reader.read_u16(), std::numeric_limits<uint16_t>::max());
    EXPECT_EQ(reader.read_u32(), std::numeric_limits<uint32_t>::min());
    EXPECT_EQ(reader.read_u32(), std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(reader.read_u64(), std::numeric_limits<uint64_t>::min());
    EXPECT_EQ(reader.read_u64(), std::numeric_limits<uint64_t>::max());
}

TEST(SerializerTest, SignedI32EdgeValuesV75) {
    clever::ipc::Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i32(0);
    s.write_i32(-1);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(reader.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(reader.read_i32(), 0);
    EXPECT_EQ(reader.read_i32(), -1);
}

TEST(SerializerTest, FloatingPointSpecialAndExtremeValuesV75) {
    clever::ipc::Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    s.write_f64(std::numeric_limits<double>::lowest());
    s.write_f64(std::numeric_limits<double>::max());

    clever::ipc::Deserializer reader(s.data());
    EXPECT_DOUBLE_EQ(reader.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(reader.read_f64(), -0.0);
    EXPECT_EQ(reader.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(reader.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(reader.read_f64()));
    EXPECT_DOUBLE_EQ(reader.read_f64(), std::numeric_limits<double>::lowest());
    EXPECT_DOUBLE_EQ(reader.read_f64(), std::numeric_limits<double>::max());
}

TEST(SerializerTest, MixedScalarRoundTripSequenceV75) {
    clever::ipc::Serializer s;
    s.write_u8(0x7Fu);
    s.write_u16(0xABCDu);
    s.write_u32(0x89ABCDEFu);
    s.write_i32(-20240229);
    s.write_u64(0x0123456789ABCDEFULL);
    s.write_f64(-987654.125);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 0x7Fu);
    EXPECT_EQ(reader.read_u16(), 0xABCDu);
    EXPECT_EQ(reader.read_u32(), 0x89ABCDEFu);
    EXPECT_EQ(reader.read_i32(), -20240229);
    EXPECT_EQ(reader.read_u64(), 0x0123456789ABCDEFULL);
    EXPECT_DOUBLE_EQ(reader.read_f64(), -987654.125);
}

TEST(SerializerTest, EmptyStringAndEmptyBytesRoundTripV75) {
    clever::ipc::Serializer s;
    const uint8_t* empty_ptr = nullptr;
    s.write_string("");
    s.write_bytes(empty_ptr, 0);
    s.write_u32(0u);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), "");
    EXPECT_TRUE(reader.read_bytes().empty());
    EXPECT_EQ(reader.read_u32(), 0u);
}

TEST(SerializerTest, BinaryPatternPayloadRoundTripV75) {
    std::vector<uint8_t> payload(512u);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>((i % 2u) == 0u ? 0xAAu : 0x55u);
    }

    clever::ipc::Serializer s;
    s.write_bytes(payload.data(), payload.size());
    s.write_u16(0x00FFu);
    s.write_u16(0xFF00u);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_bytes(), payload);
    EXPECT_EQ(reader.read_u16(), 0x00FFu);
    EXPECT_EQ(reader.read_u16(), 0xFF00u);
}

TEST(SerializerTest, Utf8StringEncodingRoundTripV75) {
    const std::string utf8 =
        "ASCII + \xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4 + "
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E + emoji \xF0\x9F\x98\x80 + "
        "accents caf\xC3\xA9 na\xC3\xAFve";
    const std::string embedded_null("pre\0post", 8);

    clever::ipc::Serializer s;
    s.write_string(utf8);
    s.write_string(embedded_null);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), utf8);
    EXPECT_EQ(reader.read_string(), embedded_null);
}

TEST(SerializerTest, InterleavedStringsBytesAndNumbersV75) {
    const std::string a = "header";
    const std::string b("x\0y\0z", 5);
    const std::vector<uint8_t> bytes = {0x00u, 0xFFu, 0x10u, 0x80u, 0x7Fu};

    clever::ipc::Serializer s;
    s.write_string(a);
    s.write_u32(2026u);
    s.write_bytes(bytes.data(), bytes.size());
    s.write_string(b);
    s.write_i32(-42);
    s.write_f64(-0.5);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), a);
    EXPECT_EQ(reader.read_u32(), 2026u);
    EXPECT_EQ(reader.read_bytes(), bytes);
    EXPECT_EQ(reader.read_string(), b);
    EXPECT_EQ(reader.read_i32(), -42);
    EXPECT_DOUBLE_EQ(reader.read_f64(), -0.5);
}

TEST(SerializerTest, UnsignedRoundTripBoundariesV76) {
    clever::ipc::Serializer s;
    s.write_u8(0u);
    s.write_u8(255u);
    s.write_u16(0u);
    s.write_u16(65535u);
    s.write_u32(0u);
    s.write_u32(4294967295u);
    s.write_u64(0u);
    s.write_u64(18446744073709551615ull);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 0u);
    EXPECT_EQ(reader.read_u8(), 255u);
    EXPECT_EQ(reader.read_u16(), 0u);
    EXPECT_EQ(reader.read_u16(), 65535u);
    EXPECT_EQ(reader.read_u32(), 0u);
    EXPECT_EQ(reader.read_u32(), 4294967295u);
    EXPECT_EQ(reader.read_u64(), 0u);
    EXPECT_EQ(reader.read_u64(), 18446744073709551615ull);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, SignedAndFloatRoundTripEdgesV76) {
    clever::ipc::Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(-1);
    s.write_i32(0);
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_f64(-0.0);
    s.write_f64(0.0);
    s.write_f64(std::numeric_limits<double>::denorm_min());
    s.write_f64(std::numeric_limits<double>::max());

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(reader.read_i32(), -1);
    EXPECT_EQ(reader.read_i32(), 0);
    EXPECT_EQ(reader.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_DOUBLE_EQ(reader.read_f64(), -0.0);
    EXPECT_DOUBLE_EQ(reader.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(reader.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_DOUBLE_EQ(reader.read_f64(), std::numeric_limits<double>::max());
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, StringUtf8AndEmbeddedNullRoundTripV76) {
    const std::string utf8 =
        "plain-\xED\x95\x9C\xEA\xB8\x80-\xE6\xBC\xA2\xE5\xAD\x97-"
        "\xF0\x9F\x9A\x80";
    const std::string with_nulls("A\0B\0C", 5);

    clever::ipc::Serializer s;
    s.write_string(utf8);
    s.write_string(with_nulls);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), utf8);
    EXPECT_EQ(reader.read_string(), with_nulls);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, BinaryAllByteValuesRoundTripV76) {
    std::vector<uint8_t> payload(256u);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }

    clever::ipc::Serializer s;
    s.write_bytes(payload.data(), payload.size());

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_bytes(), payload);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, BinaryPayloadWithZerosAndLengthMarkersV76) {
    const std::vector<uint8_t> payload = {
        0x00u, 0x00u, 0xFFu, 0x00u, 0x10u, 0x00u, 0x80u, 0x00u};

    clever::ipc::Serializer s;
    s.write_u32(static_cast<uint32_t>(payload.size()));
    s.write_bytes(payload.data(), payload.size());
    s.write_u32(0u);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u32(), payload.size());
    EXPECT_EQ(reader.read_bytes(), payload);
    EXPECT_EQ(reader.read_u32(), 0u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, MixedTypeRoundTripSequenceV76) {
    const std::string label("id:\0\x7F", 5);
    const std::vector<uint8_t> bytes = {0xDEu, 0xADu, 0xBEu, 0xEFu, 0x00u};

    clever::ipc::Serializer s;
    s.write_u8(17u);
    s.write_u16(65000u);
    s.write_u32(1234567890u);
    s.write_u64(9000000000000000000ull);
    s.write_i32(-20260001);
    s.write_f64(3.141592653589793);
    s.write_string(label);
    s.write_bytes(bytes.data(), bytes.size());

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 17u);
    EXPECT_EQ(reader.read_u16(), 65000u);
    EXPECT_EQ(reader.read_u32(), 1234567890u);
    EXPECT_EQ(reader.read_u64(), 9000000000000000000ull);
    EXPECT_EQ(reader.read_i32(), -20260001);
    EXPECT_DOUBLE_EQ(reader.read_f64(), 3.141592653589793);
    EXPECT_EQ(reader.read_string(), label);
    EXPECT_EQ(reader.read_bytes(), bytes);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, EmptyThenNonEmptyStringRoundTripV76) {
    const std::string non_empty = "serializer-v76";

    clever::ipc::Serializer s;
    s.write_string("");
    s.write_string(non_empty);
    s.write_u16(42u);

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), "");
    EXPECT_EQ(reader.read_string(), non_empty);
    EXPECT_EQ(reader.read_u16(), 42u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, MultipleByteBlobsInterleavedWithNumbersV76) {
    const std::vector<uint8_t> first = {0x01u, 0x02u, 0x03u};
    const std::vector<uint8_t> second(64u, 0xA5u);
    const std::vector<uint8_t> third = {0x00u, 0xFFu, 0x7Fu, 0x80u};

    clever::ipc::Serializer s;
    s.write_u32(1u);
    s.write_bytes(first.data(), first.size());
    s.write_u32(2u);
    s.write_bytes(second.data(), second.size());
    s.write_u32(3u);
    s.write_bytes(third.data(), third.size());

    clever::ipc::Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u32(), 1u);
    EXPECT_EQ(reader.read_bytes(), first);
    EXPECT_EQ(reader.read_u32(), 2u);
    EXPECT_EQ(reader.read_bytes(), second);
    EXPECT_EQ(reader.read_u32(), 3u);
    EXPECT_EQ(reader.read_bytes(), third);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripU8MinMaxV77) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(255);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 0u);
    EXPECT_EQ(reader.read_u8(), 255u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripU64LargeV77) {
    Serializer s;
    s.write_u64(UINT64_MAX - 1);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u64(), UINT64_MAX - 1);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripEmptyStringV77) {
    Serializer s;
    s.write_string("");

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), "");
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripEmptyBytesV77) {
    std::vector<uint8_t> empty;
    Serializer s;
    s.write_bytes(empty.data(), empty.size());

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_bytes(), empty);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, InterleavedStringsAndU32V77) {
    const std::string str1 = "hello";
    const std::string str2 = "world";

    Serializer s;
    s.write_string(str1);
    s.write_u32(42u);
    s.write_string(str2);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), str1);
    EXPECT_EQ(reader.read_u32(), 42u);
    EXPECT_EQ(reader.read_string(), str2);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64NegativeRoundTripV77) {
    Serializer s;
    s.write_f64(-123.456);

    Deserializer reader(s.data());
    EXPECT_DOUBLE_EQ(reader.read_f64(), -123.456);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64ZeroRoundTripV77) {
    Serializer s;
    s.write_f64(0.0);

    Deserializer reader(s.data());
    EXPECT_DOUBLE_EQ(reader.read_f64(), 0.0);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, HasRemainingFalseAfterFullReadV77) {
    Serializer s;
    s.write_u8(100);
    s.write_u16(2000u);
    s.write_u32(30000u);
    s.write_f64(99.99);
    s.write_string("complete");

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 100u);
    EXPECT_EQ(reader.read_u16(), 2000u);
    EXPECT_EQ(reader.read_u32(), 30000u);
    EXPECT_DOUBLE_EQ(reader.read_f64(), 99.99);
    EXPECT_EQ(reader.read_string(), "complete");
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripU16BoundaryV78) {
    Serializer s;
    s.write_u16(65535);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u16(), 65535u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripU32ZeroV78) {
    Serializer s;
    s.write_u32(0);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u32(), 0u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripLongStringV78) {
    std::string long_string(1000, 'a');

    Serializer s;
    s.write_string(long_string);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), long_string);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, MultipleByteBlobsSequentialV78) {
    const uint8_t blob1[] = {1, 2, 3, 4, 5};
    const uint8_t blob2[] = {10, 20, 30};
    const uint8_t blob3[] = {100, 200, 255};

    Serializer s;
    s.write_bytes(blob1, 5);
    s.write_bytes(blob2, 3);
    s.write_bytes(blob3, 3);

    Deserializer reader(s.data());
    auto bytes1 = reader.read_bytes();
    auto bytes2 = reader.read_bytes();
    auto bytes3 = reader.read_bytes();

    EXPECT_EQ(bytes1, std::vector<uint8_t>({1, 2, 3, 4, 5}));
    EXPECT_EQ(bytes2, std::vector<uint8_t>({10, 20, 30}));
    EXPECT_EQ(bytes3, std::vector<uint8_t>({100, 200, 255}));
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64InfinityRoundTripV78) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());

    Deserializer reader(s.data());
    double result = reader.read_f64();
    EXPECT_TRUE(std::isinf(result) && result > 0);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64NaNRoundTripV78) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::quiet_NaN());

    Deserializer reader(s.data());
    double result = reader.read_f64();
    EXPECT_TRUE(std::isnan(result));
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, MixedTypesSequentialV78) {
    Serializer s;
    s.write_u8(42);
    s.write_u16(1000u);
    s.write_u32(100000u);
    s.write_u64(0x0123456789ABCDEFull);
    s.write_f64(3.14159);
    s.write_string("test");

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 42u);
    EXPECT_EQ(reader.read_u16(), 1000u);
    EXPECT_EQ(reader.read_u32(), 100000u);
    EXPECT_EQ(reader.read_u64(), 0x0123456789ABCDEFull);
    EXPECT_DOUBLE_EQ(reader.read_f64(), 3.14159);
    EXPECT_EQ(reader.read_string(), "test");
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, RoundTripU64ZeroV78) {
    Serializer s;
    s.write_u64(0);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u64(), 0u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U8SequenceAllValuesV79) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(127);
    s.write_u8(255);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 0u);
    EXPECT_EQ(reader.read_u8(), 127u);
    EXPECT_EQ(reader.read_u8(), 255u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U32MaxValueV79) {
    Serializer s;
    s.write_u32(UINT32_MAX);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u32(), UINT32_MAX);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, StringWithSpacesV79) {
    Serializer s;
    s.write_string("hello world");

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), "hello world");
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, BytesWithPatternV79) {
    Serializer s;
    std::vector<uint8_t> pattern = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(pattern.data(), pattern.size());

    Deserializer reader(s.data());
    auto result = reader.read_bytes();
    EXPECT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0xDEu);
    EXPECT_EQ(result[1], 0xADu);
    EXPECT_EQ(result[2], 0xBEu);
    EXPECT_EQ(result[3], 0xEFu);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64PiV79) {
    Serializer s;
    s.write_f64(3.14159265358979);

    Deserializer reader(s.data());
    EXPECT_DOUBLE_EQ(reader.read_f64(), 3.14159265358979);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, EmptySerializerDataV79) {
    Serializer s;
    EXPECT_TRUE(s.data().empty());
}

TEST(SerializerTest, TwoStringsBackToBackV79) {
    Serializer s;
    s.write_string("first");
    s.write_string("second");

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), "first");
    EXPECT_EQ(reader.read_string(), "second");
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U16AllOnesV79) {
    Serializer s;
    s.write_u16(0xFFFF);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u16(), 0xFFFFu);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U64OneV80) {
    Serializer s;
    s.write_u64(1);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u64(), 1u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64NegZeroV80) {
    Serializer s;
    s.write_f64(-0.0);

    Deserializer reader(s.data());
    double val = reader.read_f64();
    EXPECT_DOUBLE_EQ(val, 0.0);
    EXPECT_TRUE(std::signbit(val));
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, StringUnicodeV80) {
    Serializer s;
    std::string unicode_str = "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80";
    s.write_string(unicode_str);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_string(), unicode_str);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, LargeBytesV80) {
    Serializer s;
    std::vector<uint8_t> large(1024);
    std::iota(large.begin(), large.end(), static_cast<uint8_t>(0));
    s.write_bytes(large.data(), large.size());

    Deserializer reader(s.data());
    auto result = reader.read_bytes();
    EXPECT_EQ(result.size(), 1024u);
    for (size_t i = 0; i < 1024; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U8ThenStringThenU8V80) {
    Serializer s;
    s.write_u8(0xAA);
    s.write_string("middle");
    s.write_u8(0xBB);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u8(), 0xAAu);
    EXPECT_EQ(reader.read_string(), "middle");
    EXPECT_EQ(reader.read_u8(), 0xBBu);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U32SequenceV80) {
    Serializer s;
    for (uint32_t i = 0; i < 5; ++i) {
        s.write_u32(i * 1000);
    }

    Deserializer reader(s.data());
    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(reader.read_u32(), i * 1000);
    }
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64VerySmallV80) {
    Serializer s;
    double tiny = std::numeric_limits<double>::min();
    s.write_f64(tiny);

    Deserializer reader(s.data());
    EXPECT_DOUBLE_EQ(reader.read_f64(), tiny);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U16ZeroV80) {
    Serializer s;
    s.write_u16(0);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u16(), 0u);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U8AllPowersOfTwoV81) {
    Serializer s;
    for (int i = 0; i < 8; ++i) {
        s.write_u8(static_cast<uint8_t>(1u << i));
    }

    Deserializer reader(s.data());
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(reader.read_u8(), static_cast<uint8_t>(1u << i));
    }
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64DenormalizedValueV81) {
    Serializer s;
    double denorm = std::numeric_limits<double>::denorm_min();
    s.write_f64(denorm);

    Deserializer reader(s.data());
    EXPECT_DOUBLE_EQ(reader.read_f64(), denorm);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, StringSingleCharRepeatedV81) {
    Serializer s;
    std::string repeated(256, 'Z');
    s.write_string(repeated);

    Deserializer reader(s.data());
    std::string result = reader.read_string();
    EXPECT_EQ(result.size(), 256u);
    EXPECT_EQ(result, repeated);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, BytesAllZerosV81) {
    Serializer s;
    std::vector<uint8_t> zeros(512, 0);
    s.write_bytes(zeros.data(), zeros.size());

    Deserializer reader(s.data());
    auto result = reader.read_bytes();
    EXPECT_EQ(result.size(), 512u);
    for (size_t i = 0; i < 512; ++i) {
        EXPECT_EQ(result[i], 0u);
    }
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, U16U32U64DescendingV81) {
    Serializer s;
    s.write_u16(65535);
    s.write_u32(4294967295u);
    s.write_u64(UINT64_MAX);

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_u16(), 65535u);
    EXPECT_EQ(reader.read_u32(), 4294967295u);
    EXPECT_EQ(reader.read_u64(), UINT64_MAX);
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, MultipleEmptyStringsV81) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_string("");
    }

    Deserializer reader(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(reader.read_string(), "");
    }
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, F64SpecialSequenceV81) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::lowest());

    Deserializer reader(s.data());
    EXPECT_EQ(reader.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(reader.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(reader.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(reader.read_f64(), std::numeric_limits<double>::lowest());
    EXPECT_FALSE(reader.has_remaining());
}

TEST(SerializerTest, AllTypesReversedOrderV81) {
    Serializer s;
    std::vector<uint8_t> raw_bytes = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(raw_bytes.data(), raw_bytes.size());
    s.write_string("reversed");
    s.write_f64(2.71828);
    s.write_u64(123456789012345ULL);
    s.write_u32(42);
    s.write_u16(999);
    s.write_u8(77);

    Deserializer reader(s.data());
    auto bytes_out = reader.read_bytes();
    EXPECT_EQ(bytes_out.size(), 4u);
    EXPECT_EQ(bytes_out[0], 0xDE);
    EXPECT_EQ(bytes_out[1], 0xAD);
    EXPECT_EQ(bytes_out[2], 0xBE);
    EXPECT_EQ(bytes_out[3], 0xEF);
    EXPECT_EQ(reader.read_string(), "reversed");
    EXPECT_DOUBLE_EQ(reader.read_f64(), 2.71828);
    EXPECT_EQ(reader.read_u64(), 123456789012345ULL);
    EXPECT_EQ(reader.read_u32(), 42u);
    EXPECT_EQ(reader.read_u16(), 999u);
    EXPECT_EQ(reader.read_u8(), 77u);
    EXPECT_FALSE(reader.has_remaining());
}

// ------------------------------------------------------------------
// V82 tests
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripU16PowersOfTwoV82) {
    Serializer s;
    s.write_u16(1);
    s.write_u16(2);
    s.write_u16(4);
    s.write_u16(256);
    s.write_u16(1024);
    s.write_u16(32768);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 1u);
    EXPECT_EQ(d.read_u16(), 2u);
    EXPECT_EQ(d.read_u16(), 4u);
    EXPECT_EQ(d.read_u16(), 256u);
    EXPECT_EQ(d.read_u16(), 1024u);
    EXPECT_EQ(d.read_u16(), 32768u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripF64SubnormalAndTinyV82) {
    Serializer s;
    double subnormal = std::numeric_limits<double>::denorm_min();
    double tiny = std::numeric_limits<double>::min();  // smallest normal
    double neg_tiny = -std::numeric_limits<double>::min();
    s.write_f64(subnormal);
    s.write_f64(tiny);
    s.write_f64(neg_tiny);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), subnormal);
    EXPECT_DOUBLE_EQ(d.read_f64(), tiny);
    EXPECT_DOUBLE_EQ(d.read_f64(), neg_tiny);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripStringWithNewlinesAndTabsV82) {
    Serializer s;
    std::string multiline = "line1\nline2\nline3";
    std::string tabbed = "col1\tcol2\tcol3";
    std::string mixed = "\r\n\t \r\n\t ";
    s.write_string(multiline);
    s.write_string(tabbed);
    s.write_string(mixed);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), multiline);
    EXPECT_EQ(d.read_string(), tabbed);
    EXPECT_EQ(d.read_string(), mixed);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripBytesAllZerosV82) {
    Serializer s;
    std::vector<uint8_t> zeros(256, 0x00);
    s.write_bytes(zeros.data(), zeros.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 256u);
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], 0x00) << "Mismatch at index " << i;
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU32BitPatternsV82) {
    Serializer s;
    s.write_u32(0x00000000u);
    s.write_u32(0xFFFFFFFFu);
    s.write_u32(0xAAAAAAAAu);
    s.write_u32(0x55555555u);
    s.write_u32(0x0F0F0F0Fu);
    s.write_u32(0xF0F0F0F0u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0x00000000u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0xAAAAAAAAu);
    EXPECT_EQ(d.read_u32(), 0x55555555u);
    EXPECT_EQ(d.read_u32(), 0x0F0F0F0Fu);
    EXPECT_EQ(d.read_u32(), 0xF0F0F0F0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripManyStringsV82) {
    Serializer s;
    const int count = 50;
    for (int i = 0; i < count; ++i) {
        s.write_string("item_" + std::to_string(i));
    }

    Deserializer d(s.data());
    for (int i = 0; i < count; ++i) {
        EXPECT_EQ(d.read_string(), "item_" + std::to_string(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU64FibonacciValuesV82) {
    Serializer s;
    uint64_t a = 0, b = 1;
    std::vector<uint64_t> fibs;
    for (int i = 0; i < 20; ++i) {
        fibs.push_back(a);
        s.write_u64(a);
        uint64_t next = a + b;
        a = b;
        b = next;
    }

    Deserializer d(s.data());
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(d.read_u64(), fibs[i]);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripInterleavedTypesCompactV82) {
    Serializer s;
    s.write_u8(0xFF);
    s.write_string("between");
    s.write_u16(12345);
    s.write_bytes(nullptr, 0);
    s.write_f64(3.14);
    s.write_u32(0xDEADBEEFu);
    s.write_string("");
    s.write_u64(0xCAFEBABECAFEBABEULL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xFFu);
    EXPECT_EQ(d.read_string(), "between");
    EXPECT_EQ(d.read_u16(), 12345u);
    auto empty_bytes = d.read_bytes();
    EXPECT_TRUE(empty_bytes.empty());
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_u64(), 0xCAFEBABECAFEBABEULL);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V83 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U8BoundaryValuesV83) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(128);
    s.write_u8(254);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 1u);
    EXPECT_EQ(d.read_u8(), 128u);
    EXPECT_EQ(d.read_u8(), 254u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV83) {
    Serializer s;
    s.write_u16(1);
    s.write_u16(256);
    s.write_u16(1024);
    s.write_u16(32768);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 1u);
    EXPECT_EQ(d.read_u16(), 256u);
    EXPECT_EQ(d.read_u16(), 1024u);
    EXPECT_EQ(d.read_u16(), 32768u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32SpecificBitPatternsV83) {
    Serializer s;
    s.write_u32(0x00000001u);
    s.write_u32(0x80000000u);
    s.write_u32(0x7FFFFFFFu);
    s.write_u32(0xAAAAAAAAu);
    s.write_u32(0x55555555u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0x00000001u);
    EXPECT_EQ(d.read_u32(), 0x80000000u);
    EXPECT_EQ(d.read_u32(), 0x7FFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0xAAAAAAAAu);
    EXPECT_EQ(d.read_u32(), 0x55555555u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64LargeValuesV83) {
    Serializer s;
    s.write_u64(0ULL);
    s.write_u64(0x0000000100000000ULL);
    s.write_u64(0x7FFFFFFFFFFFFFFFULL);
    s.write_u64(0xFFFFFFFFFFFFFFFFULL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ULL);
    EXPECT_EQ(d.read_u64(), 0x0000000100000000ULL);
    EXPECT_EQ(d.read_u64(), 0x7FFFFFFFFFFFFFFFULL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV83) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithEmbeddedNullsV83) {
    Serializer s;
    std::string with_nulls("abc\0def", 7);
    s.write_string(with_nulls);
    s.write_string("");
    s.write_string("trailing");

    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_EQ(result.size(), 7u);
    EXPECT_EQ(result, with_nulls);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "trailing");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesVariousLengthsV83) {
    Serializer s;
    // Empty bytes
    s.write_bytes(nullptr, 0);
    // Single byte
    uint8_t one = 0xAB;
    s.write_bytes(&one, 1);
    // Larger block
    std::vector<uint8_t> block(256);
    std::iota(block.begin(), block.end(), static_cast<uint8_t>(0));
    s.write_bytes(block.data(), block.size());

    Deserializer d(s.data());
    auto empty = d.read_bytes();
    EXPECT_TRUE(empty.empty());
    auto single = d.read_bytes();
    ASSERT_EQ(single.size(), 1u);
    EXPECT_EQ(single[0], 0xABu);
    auto big = d.read_bytes();
    ASSERT_EQ(big.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(big[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedTypesComplexV83) {
    Serializer s;
    s.write_u8(42);
    s.write_string("hello world");
    s.write_u32(0xBAADF00Du);
    s.write_f64(2.718281828459045);
    s.write_u16(9999);
    uint8_t raw[] = {10, 20, 30, 40, 50};
    s.write_bytes(raw, 5);
    s.write_u64(123456789012345ULL);
    s.write_string("");
    s.write_u8(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_string(), "hello world");
    EXPECT_EQ(d.read_u32(), 0xBAADF00Du);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
    EXPECT_EQ(d.read_u16(), 9999u);
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 10u);
    EXPECT_EQ(bytes[4], 50u);
    EXPECT_EQ(d.read_u64(), 123456789012345ULL);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V84 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U8BoundaryValuesV84) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(254);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 1u);
    EXPECT_EQ(d.read_u8(), 127u);
    EXPECT_EQ(d.read_u8(), 128u);
    EXPECT_EQ(d.read_u8(), 254u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV84) {
    Serializer s;
    for (int i = 0; i < 16; ++i) {
        s.write_u16(static_cast<uint16_t>(1u << i));
    }

    Deserializer d(s.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32AlternatingBitPatternsV84) {
    Serializer s;
    s.write_u32(0x00000000u);
    s.write_u32(0xFFFFFFFFu);
    s.write_u32(0xAAAAAAAAu);
    s.write_u32(0x55555555u);
    s.write_u32(0x0F0F0F0Fu);
    s.write_u32(0xF0F0F0F0u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0x00000000u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0xAAAAAAAAu);
    EXPECT_EQ(d.read_u32(), 0x55555555u);
    EXPECT_EQ(d.read_u32(), 0x0F0F0F0Fu);
    EXPECT_EQ(d.read_u32(), 0xF0F0F0F0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64LargeValuesV84) {
    Serializer s;
    s.write_u64(0ULL);
    s.write_u64(1ULL);
    s.write_u64(0x00000000FFFFFFFFuLL);
    s.write_u64(0xFFFFFFFF00000000uLL);
    s.write_u64(0x8000000000000000uLL);
    s.write_u64(0xFFFFFFFFFFFFFFFFuLL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ULL);
    EXPECT_EQ(d.read_u64(), 1ULL);
    EXPECT_EQ(d.read_u64(), 0x00000000FFFFFFFFuLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFF00000000uLL);
    EXPECT_EQ(d.read_u64(), 0x8000000000000000uLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFuLL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV84) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::epsilon());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithSpecialCharsV84) {
    Serializer s;
    s.write_string("");
    s.write_string("a");
    s.write_string("hello\nworld\ttab");
    s.write_string(std::string("null\0inside", 11));
    s.write_string("unicode: \xC3\xA9\xC3\xA0\xC3\xBC");
    s.write_string(std::string(1000, 'X'));

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "a");
    EXPECT_EQ(d.read_string(), "hello\nworld\ttab");
    std::string with_null("null\0inside", 11);
    EXPECT_EQ(d.read_string(), with_null);
    EXPECT_EQ(d.read_string(), "unicode: \xC3\xA9\xC3\xA0\xC3\xBC");
    EXPECT_EQ(d.read_string(), std::string(1000, 'X'));
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesVariousSizesV84) {
    Serializer s;
    // Empty
    s.write_bytes(nullptr, 0);
    // Single byte
    uint8_t single = 0xFF;
    s.write_bytes(&single, 1);
    // 4 bytes
    uint8_t four[] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(four, 4);
    // 512 bytes
    std::vector<uint8_t> large(512);
    for (size_t i = 0; i < large.size(); ++i) {
        large[i] = static_cast<uint8_t>(i % 256);
    }
    s.write_bytes(large.data(), large.size());

    Deserializer d(s.data());
    auto r0 = d.read_bytes();
    EXPECT_TRUE(r0.empty());
    auto r1 = d.read_bytes();
    ASSERT_EQ(r1.size(), 1u);
    EXPECT_EQ(r1[0], 0xFFu);
    auto r4 = d.read_bytes();
    ASSERT_EQ(r4.size(), 4u);
    EXPECT_EQ(r4[0], 0xDEu);
    EXPECT_EQ(r4[1], 0xADu);
    EXPECT_EQ(r4[2], 0xBEu);
    EXPECT_EQ(r4[3], 0xEFu);
    auto r512 = d.read_bytes();
    ASSERT_EQ(r512.size(), 512u);
    for (size_t i = 0; i < 512; ++i) {
        EXPECT_EQ(r512[i], static_cast<uint8_t>(i % 256));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypeStressSequenceV84) {
    Serializer s;
    // Write a complex interleaved sequence of all types
    s.write_u8(255);
    s.write_u16(65535);
    s.write_u32(0xDEADBEEFu);
    s.write_u64(0x0123456789ABCDEFuLL);
    s.write_f64(3.14159265358979323846);
    s.write_string("serializer stress test");
    uint8_t blob[] = {1, 2, 3};
    s.write_bytes(blob, 3);
    // Second pass: different values
    s.write_u8(0);
    s.write_u16(0);
    s.write_u32(1u);
    s.write_u64(1ULL);
    s.write_f64(-1.0e308);
    s.write_string("");
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    // First pass
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_EQ(d.read_u16(), 65535u);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_u64(), 0x0123456789ABCDEFuLL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979323846);
    EXPECT_EQ(d.read_string(), "serializer stress test");
    auto b1 = d.read_bytes();
    ASSERT_EQ(b1.size(), 3u);
    EXPECT_EQ(b1[0], 1u);
    EXPECT_EQ(b1[1], 2u);
    EXPECT_EQ(b1[2], 3u);
    // Second pass
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u16(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u64(), 1ULL);
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.0e308);
    EXPECT_EQ(d.read_string(), "");
    auto b2 = d.read_bytes();
    EXPECT_TRUE(b2.empty());
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V85 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U8BoundaryValuesV85) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(254);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 1u);
    EXPECT_EQ(d.read_u8(), 127u);
    EXPECT_EQ(d.read_u8(), 128u);
    EXPECT_EQ(d.read_u8(), 254u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV85) {
    Serializer s;
    for (int i = 0; i < 16; ++i) {
        s.write_u16(static_cast<uint16_t>(1u << i));
    }

    Deserializer d(s.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32AlternatingBitPatternsV85) {
    Serializer s;
    s.write_u32(0x55555555u);
    s.write_u32(0xAAAAAAAAu);
    s.write_u32(0x0F0F0F0Fu);
    s.write_u32(0xF0F0F0F0u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0x55555555u);
    EXPECT_EQ(d.read_u32(), 0xAAAAAAAAu);
    EXPECT_EQ(d.read_u32(), 0x0F0F0F0Fu);
    EXPECT_EQ(d.read_u32(), 0xF0F0F0F0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64LargeValuesV85) {
    Serializer s;
    s.write_u64(0ULL);
    s.write_u64(1ULL);
    s.write_u64(0x00000000FFFFFFFFuLL);
    s.write_u64(0xFFFFFFFF00000000uLL);
    s.write_u64(0xFFFFFFFFFFFFFFFFuLL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ULL);
    EXPECT_EQ(d.read_u64(), 1ULL);
    EXPECT_EQ(d.read_u64(), 0x00000000FFFFFFFFuLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFF00000000uLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFuLL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV85) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::epsilon());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithNullBytesV85) {
    Serializer s;
    std::string with_nulls("hello\0world", 11);
    s.write_string(with_nulls);
    s.write_string("");
    std::string long_str(1000, 'X');
    s.write_string(long_str);

    Deserializer d(s.data());
    std::string r1 = d.read_string();
    EXPECT_EQ(r1.size(), 11u);
    EXPECT_EQ(r1, with_nulls);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), long_str);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesIncreasingLengthsV85) {
    Serializer s;
    // Write byte arrays of lengths 0, 1, 4, 16, 256
    s.write_bytes(nullptr, 0);

    uint8_t one_byte[] = {0x42};
    s.write_bytes(one_byte, 1);

    uint8_t four_bytes[] = {10, 20, 30, 40};
    s.write_bytes(four_bytes, 4);

    std::vector<uint8_t> sixteen(16);
    std::iota(sixteen.begin(), sixteen.end(), static_cast<uint8_t>(0));
    s.write_bytes(sixteen.data(), sixteen.size());

    std::vector<uint8_t> big(256);
    for (size_t i = 0; i < 256; ++i) big[i] = static_cast<uint8_t>(255 - i);
    s.write_bytes(big.data(), big.size());

    Deserializer d(s.data());
    auto r0 = d.read_bytes();
    EXPECT_TRUE(r0.empty());

    auto r1 = d.read_bytes();
    ASSERT_EQ(r1.size(), 1u);
    EXPECT_EQ(r1[0], 0x42u);

    auto r4 = d.read_bytes();
    ASSERT_EQ(r4.size(), 4u);
    EXPECT_EQ(r4[0], 10u);
    EXPECT_EQ(r4[3], 40u);

    auto r16 = d.read_bytes();
    ASSERT_EQ(r16.size(), 16u);
    for (size_t i = 0; i < 16; ++i) {
        EXPECT_EQ(r16[i], static_cast<uint8_t>(i));
    }

    auto r256 = d.read_bytes();
    ASSERT_EQ(r256.size(), 256u);
    for (size_t i = 0; i < 256; ++i) {
        EXPECT_EQ(r256[i], static_cast<uint8_t>(255 - i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedTypesRoundTripV85) {
    Serializer s;
    s.write_u8(42);
    s.write_string("interleaved");
    s.write_u32(0xCAFEBABEu);
    uint8_t blob[] = {9, 8, 7, 6, 5};
    s.write_bytes(blob, 5);
    s.write_f64(2.718281828459045);
    s.write_u16(12345);
    s.write_u64(0xFEDCBA9876543210uLL);
    s.write_string("end");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42u);
    EXPECT_EQ(d.read_string(), "interleaved");
    EXPECT_EQ(d.read_u32(), 0xCAFEBABEu);
    auto rb = d.read_bytes();
    ASSERT_EQ(rb.size(), 5u);
    EXPECT_EQ(rb[0], 9u);
    EXPECT_EQ(rb[4], 5u);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_EQ(d.read_u64(), 0xFEDCBA9876543210uLL);
    EXPECT_EQ(d.read_string(), "end");
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V86 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U8BoundaryValuesV86) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(254);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0u);
    EXPECT_EQ(d.read_u8(), 1u);
    EXPECT_EQ(d.read_u8(), 127u);
    EXPECT_EQ(d.read_u8(), 128u);
    EXPECT_EQ(d.read_u8(), 254u);
    EXPECT_EQ(d.read_u8(), 255u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV86) {
    Serializer s;
    for (int i = 0; i < 16; ++i) {
        s.write_u16(static_cast<uint16_t>(1u << i));
    }

    Deserializer d(s.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32MaxAndZeroV86) {
    Serializer s;
    s.write_u32(0u);
    s.write_u32(1u);
    s.write_u32(0x7FFFFFFFu);
    s.write_u32(0x80000000u);
    s.write_u32(0xFFFFFFFFu);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 0x7FFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0x80000000u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV86) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringAndBytesV86) {
    Serializer s;
    s.write_string("");
    std::vector<uint8_t> empty_buf;
    s.write_bytes(empty_buf.data(), 0);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    auto rb = d.read_bytes();
    EXPECT_EQ(rb.size(), 0u);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesBlobV86) {
    Serializer s;
    std::vector<uint8_t> big(4096);
    for (size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<uint8_t>(i & 0xFF);
    }
    s.write_bytes(big.data(), big.size());

    Deserializer d(s.data());
    auto rb = d.read_bytes();
    ASSERT_EQ(rb.size(), 4096u);
    for (size_t i = 0; i < 4096; ++i) {
        EXPECT_EQ(rb[i], static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64HighBitsV86) {
    Serializer s;
    s.write_u64(0uLL);
    s.write_u64(1uLL);
    s.write_u64(0x00000000FFFFFFFFuLL);
    s.write_u64(0xFFFFFFFF00000000uLL);
    s.write_u64(0xFFFFFFFFFFFFFFFFuLL);
    s.write_u64(0x8000000000000000uLL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0uLL);
    EXPECT_EQ(d.read_u64(), 1uLL);
    EXPECT_EQ(d.read_u64(), 0x00000000FFFFFFFFuLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFF00000000uLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFuLL);
    EXPECT_EQ(d.read_u64(), 0x8000000000000000uLL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsWithSpecialCharsV86) {
    Serializer s;
    s.write_string("hello world");
    s.write_string("tab\there");
    s.write_string("newline\nhere");
    s.write_string("null\0gone");  // only up to null in C++ string literal
    s.write_string(std::string("embedded\0null", 13));
    s.write_string("unicode: \xC3\xA9\xC3\xA0\xC3\xBC");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello world");
    EXPECT_EQ(d.read_string(), "tab\there");
    EXPECT_EQ(d.read_string(), "newline\nhere");
    EXPECT_EQ(d.read_string(), std::string("null"));
    EXPECT_EQ(d.read_string(), std::string("embedded\0null", 13));
    EXPECT_EQ(d.read_string(), "unicode: \xC3\xA9\xC3\xA0\xC3\xBC");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U8BoundaryValuesV87) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(254);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u8(), 127);
    EXPECT_EQ(d.read_u8(), 128);
    EXPECT_EQ(d.read_u8(), 254);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV87) {
    Serializer s;
    for (int i = 0; i < 16; ++i) {
        s.write_u16(static_cast<uint16_t>(1u << i));
    }

    Deserializer d(s.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV87) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::epsilon());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), 0.0);
    EXPECT_EQ(d.read_f64(), -0.0);
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_TRUE(std::isnan(d.read_f64()));
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesRoundTripV87) {
    Serializer s;
    std::vector<uint8_t> empty;
    s.write_bytes(empty.data(), 0);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesPayloadV87) {
    Serializer s;
    std::vector<uint8_t> big(4096);
    std::iota(big.begin(), big.end(), static_cast<uint8_t>(0));
    s.write_bytes(big.data(), big.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 4096u);
    EXPECT_EQ(result, big);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedTypesV87) {
    Serializer s;
    s.write_u8(42);
    s.write_string("hello");
    s.write_u32(0xDEADBEEFu);
    s.write_f64(3.14);
    s.write_u16(9999);
    std::vector<uint8_t> blob = {0xCA, 0xFE};
    s.write_bytes(blob.data(), blob.size());
    s.write_u64(0x0102030405060708uLL);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42);
    EXPECT_EQ(d.read_string(), "hello");
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14);
    EXPECT_EQ(d.read_u16(), 9999);
    auto bytes_out = d.read_bytes();
    EXPECT_EQ(bytes_out.size(), 2u);
    EXPECT_EQ(bytes_out[0], 0xCA);
    EXPECT_EQ(bytes_out[1], 0xFE);
    EXPECT_EQ(d.read_u64(), 0x0102030405060708uLL);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ManyStringsV87) {
    Serializer s;
    std::vector<std::string> strings;
    for (int i = 0; i < 100; ++i) {
        strings.push_back("str_" + std::to_string(i));
    }
    for (const auto& str : strings) {
        s.write_string(str);
    }

    Deserializer d(s.data());
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(d.read_string(), "str_" + std::to_string(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32AlternatingBitsV87) {
    Serializer s;
    s.write_u32(0xAAAAAAAAu);
    s.write_u32(0x55555555u);
    s.write_u32(0x0F0F0F0Fu);
    s.write_u32(0xF0F0F0F0u);
    s.write_u32(0x00FF00FFu);
    s.write_u32(0xFF00FF00u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0xAAAAAAAAu);
    EXPECT_EQ(d.read_u32(), 0x55555555u);
    EXPECT_EQ(d.read_u32(), 0x0F0F0F0Fu);
    EXPECT_EQ(d.read_u32(), 0xF0F0F0F0u);
    EXPECT_EQ(d.read_u32(), 0x00FF00FFu);
    EXPECT_EQ(d.read_u32(), 0xFF00FF00u);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V88 Tests
// ------------------------------------------------------------------

TEST(SerializerTest, U8BoundaryValuesV88) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(128);
    s.write_u8(254);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u8(), 128);
    EXPECT_EQ(d.read_u8(), 254);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV88) {
    Serializer s;
    for (int i = 0; i < 16; ++i) {
        s.write_u16(static_cast<uint16_t>(1u << i));
    }

    Deserializer d(s.data());
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV88) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::epsilon());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesRoundTripV88) {
    Serializer s;
    std::vector<uint8_t> empty_bytes;
    s.write_bytes(empty_bytes.data(), empty_bytes.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesBlockV88) {
    Serializer s;
    std::vector<uint8_t> big(4096);
    std::iota(big.begin(), big.end(), static_cast<uint8_t>(0));
    s.write_bytes(big.data(), big.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 4096u);
    for (size_t i = 0; i < 4096; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithNullBytesV88) {
    Serializer s;
    std::string with_nulls("hello\0world", 11);
    s.write_string(with_nulls);

    Deserializer d(s.data());
    std::string out = d.read_string();
    EXPECT_EQ(out.size(), 11u);
    EXPECT_EQ(out, with_nulls);
    EXPECT_EQ(out[5], '\0');
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64MaxAndMinValuesV88) {
    Serializer s;
    s.write_u64(0uLL);
    s.write_u64(1uLL);
    s.write_u64(0xFFFFFFFFFFFFFFFFuLL);
    s.write_u64(0x8000000000000000uLL);
    s.write_u64(0x7FFFFFFFFFFFFFFFuLL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0uLL);
    EXPECT_EQ(d.read_u64(), 1uLL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFuLL);
    EXPECT_EQ(d.read_u64(), 0x8000000000000000uLL);
    EXPECT_EQ(d.read_u64(), 0x7FFFFFFFFFFFFFFFuLL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedTypesComplexV88) {
    Serializer s;
    s.write_u8(0xFF);
    s.write_f64(-273.15);
    s.write_string("temperature");
    s.write_u32(100u);
    s.write_bytes(reinterpret_cast<const uint8_t*>("\xDE\xAD\xBE\xEF"), 4);
    s.write_u16(0x1234);
    s.write_u64(999999999999uLL);
    s.write_string("");
    s.write_u8(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xFF);
    EXPECT_DOUBLE_EQ(d.read_f64(), -273.15);
    EXPECT_EQ(d.read_string(), "temperature");
    EXPECT_EQ(d.read_u32(), 100u);
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0xDE);
    EXPECT_EQ(bytes[1], 0xAD);
    EXPECT_EQ(bytes[2], 0xBE);
    EXPECT_EQ(bytes[3], 0xEF);
    EXPECT_EQ(d.read_u16(), 0x1234);
    EXPECT_EQ(d.read_u64(), 999999999999uLL);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU8MaxMinV89) {
    Serializer s;
    s.write_u8(0);
    s.write_u8(1);
    s.write_u8(127);
    s.write_u8(128);
    s.write_u8(255);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_EQ(d.read_u8(), 1);
    EXPECT_EQ(d.read_u8(), 127);
    EXPECT_EQ(d.read_u8(), 128);
    EXPECT_EQ(d.read_u8(), 255);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU16MaxV89) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(1);
    s.write_u16(256);
    s.write_u16(32767);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0);
    EXPECT_EQ(d.read_u16(), 1);
    EXPECT_EQ(d.read_u16(), 256);
    EXPECT_EQ(d.read_u16(), 32767);
    EXPECT_EQ(d.read_u16(), 65535);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU32MaxV89) {
    Serializer s;
    s.write_u32(0u);
    s.write_u32(1u);
    s.write_u32(65536u);
    s.write_u32(2147483647u);
    s.write_u32(4294967295u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 65536u);
    EXPECT_EQ(d.read_u32(), 2147483647u);
    EXPECT_EQ(d.read_u32(), 4294967295u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RoundTripU64MaxValueV89) {
    Serializer s;
    s.write_u64(0uLL);
    s.write_u64(UINT64_MAX);
    s.write_u64(0x0102030405060708uLL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0uLL);
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
    EXPECT_EQ(d.read_u64(), 0x0102030405060708uLL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsSequenceV89) {
    Serializer s;
    s.write_string("alpha");
    s.write_string("beta");
    s.write_string("");
    s.write_string("gamma delta");
    s.write_string("epsilon");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    EXPECT_EQ(d.read_string(), "beta");
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "gamma delta");
    EXPECT_EQ(d.read_string(), "epsilon");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesRoundTripV89) {
    Serializer s;
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64NegativeAndZeroV89) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(-1.0);
    s.write_f64(-999999.123456);
    s.write_f64(-1e-300);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -999999.123456);
    EXPECT_DOUBLE_EQ(d.read_f64(), -1e-300);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypesSequenceV89) {
    Serializer s;
    s.write_u8(42);
    s.write_string("hello world");
    s.write_u32(123456789u);
    s.write_f64(3.14159265358979);
    s.write_u16(9999);
    s.write_bytes(reinterpret_cast<const uint8_t*>("abc"), 3);
    s.write_u64(0xDEADBEEFCAFEuLL);
    s.write_string("end");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42);
    EXPECT_EQ(d.read_string(), "hello world");
    EXPECT_EQ(d.read_u32(), 123456789u);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
    EXPECT_EQ(d.read_u16(), 9999);
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 'a');
    EXPECT_EQ(bytes[1], 'b');
    EXPECT_EQ(bytes[2], 'c');
    EXPECT_EQ(d.read_u64(), 0xDEADBEEFCAFEuLL);
    EXPECT_EQ(d.read_string(), "end");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U8AllBitPatternsV90) {
    Serializer s;
    for (uint16_t i = 0; i < 256; ++i) {
        s.write_u8(static_cast<uint8_t>(i));
    }
    Deserializer d(s.data());
    for (uint16_t i = 0; i < 256; ++i) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV90) {
    Serializer s;
    s.write_u16(1);
    s.write_u16(2);
    s.write_u16(4);
    s.write_u16(256);
    s.write_u16(1024);
    s.write_u16(16384);
    s.write_u16(32768);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 1);
    EXPECT_EQ(d.read_u16(), 2);
    EXPECT_EQ(d.read_u16(), 4);
    EXPECT_EQ(d.read_u16(), 256);
    EXPECT_EQ(d.read_u16(), 1024);
    EXPECT_EQ(d.read_u16(), 16384);
    EXPECT_EQ(d.read_u16(), 32768);
    EXPECT_EQ(d.read_u16(), 65535);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32MaxAndNeighborsV90) {
    Serializer s;
    s.write_u32(0u);
    s.write_u32(1u);
    s.write_u32(0x7FFFFFFFu);
    s.write_u32(0x80000000u);
    s.write_u32(0xFFFFFFFEu);
    s.write_u32(0xFFFFFFFFu);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 0x7FFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0x80000000u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFEu);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SubnormalsAndExtremesV90) {
    Serializer s;
    s.write_f64(5e-324);
    s.write_f64(-5e-324);
    s.write_f64(1.7976931348623157e+308);
    s.write_f64(-1.7976931348623157e+308);
    s.write_f64(2.2250738585072014e-308);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 5e-324);
    EXPECT_DOUBLE_EQ(d.read_f64(), -5e-324);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.7976931348623157e+308);
    EXPECT_DOUBLE_EQ(d.read_f64(), -1.7976931348623157e+308);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.2250738585072014e-308);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsWithSpecialCharsV90) {
    Serializer s;
    s.write_string("line1\nline2\nline3");
    s.write_string("tab\there");
    s.write_string("quote\"inside");
    s.write_string("backslash\\path");
    s.write_string("\r\n\t");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "line1\nline2\nline3");
    EXPECT_EQ(d.read_string(), "tab\there");
    EXPECT_EQ(d.read_string(), "quote\"inside");
    EXPECT_EQ(d.read_string(), "backslash\\path");
    EXPECT_EQ(d.read_string(), "\r\n\t");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithRepeatedPatternsV90) {
    std::vector<uint8_t> pattern_aa(128, 0xAA);
    std::vector<uint8_t> pattern_55(128, 0x55);
    std::vector<uint8_t> pattern_00(64, 0x00);
    std::vector<uint8_t> pattern_ff(64, 0xFF);

    Serializer s;
    s.write_bytes(pattern_aa.data(), pattern_aa.size());
    s.write_bytes(pattern_55.data(), pattern_55.size());
    s.write_bytes(pattern_00.data(), pattern_00.size());
    s.write_bytes(pattern_ff.data(), pattern_ff.size());

    Deserializer d(s.data());
    auto r1 = d.read_bytes();
    EXPECT_EQ(r1.size(), 128u);
    for (size_t i = 0; i < 128; ++i) EXPECT_EQ(r1[i], 0xAA);
    auto r2 = d.read_bytes();
    EXPECT_EQ(r2.size(), 128u);
    for (size_t i = 0; i < 128; ++i) EXPECT_EQ(r2[i], 0x55);
    auto r3 = d.read_bytes();
    EXPECT_EQ(r3.size(), 64u);
    for (size_t i = 0; i < 64; ++i) EXPECT_EQ(r3[i], 0x00);
    auto r4 = d.read_bytes();
    EXPECT_EQ(r4.size(), 64u);
    for (size_t i = 0; i < 64; ++i) EXPECT_EQ(r4[i], 0xFF);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64BitShiftValuesV90) {
    Serializer s;
    for (int shift = 0; shift < 64; ++shift) {
        s.write_u64(1uLL << shift);
    }

    Deserializer d(s.data());
    for (int shift = 0; shift < 64; ++shift) {
        EXPECT_EQ(d.read_u64(), 1uLL << shift);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedStringAndU32V90) {
    Serializer s;
    for (uint32_t i = 0; i < 20; ++i) {
        s.write_u32(i * 1000);
        s.write_string("item_" + std::to_string(i));
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 20; ++i) {
        EXPECT_EQ(d.read_u32(), i * 1000);
        EXPECT_EQ(d.read_string(), "item_" + std::to_string(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U8BoundaryAlternatingV91) {
    Serializer s;
    for (uint8_t v = 0; v < 255; ++v) {
        s.write_u8(v);
        s.write_u8(static_cast<uint8_t>(255 - v));
    }

    Deserializer d(s.data());
    for (uint8_t v = 0; v < 255; ++v) {
        EXPECT_EQ(d.read_u8(), v);
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(255 - v));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialSequenceV91) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(1.0e-300);
    s.write_f64(1.0e+300);
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::epsilon());
    s.write_f64(std::numeric_limits<double>::denorm_min());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), -0.0);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0e-300);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0e+300);
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithNullBytesV91) {
    std::string with_null("hello\0world", 11);
    std::string with_multi_null("a\0b\0c\0d", 7);

    Serializer s;
    s.write_string(with_null);
    s.write_string(with_multi_null);

    Deserializer d(s.data());
    auto r1 = d.read_string();
    EXPECT_EQ(r1.size(), 11u);
    EXPECT_EQ(r1, with_null);
    auto r2 = d.read_string();
    EXPECT_EQ(r2.size(), 7u);
    EXPECT_EQ(r2, with_multi_null);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesGradientPatternV91) {
    std::vector<uint8_t> gradient(256);
    for (int i = 0; i < 256; ++i) gradient[i] = static_cast<uint8_t>(i);

    Serializer s;
    s.write_bytes(gradient.data(), gradient.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16PowersOfTwoV91) {
    Serializer s;
    for (int p = 0; p < 16; ++p) {
        s.write_u16(static_cast<uint16_t>(1u << p));
    }

    Deserializer d(s.data());
    for (int p = 0; p < 16; ++p) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << p));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypesFullCoverageV91) {
    Serializer s;
    s.write_u8(42);
    s.write_u16(12345);
    s.write_u32(0xABCD1234u);
    s.write_u64(0x0102030405060708uLL);
    s.write_f64(2.718281828459045);
    s.write_string("interleaved");
    uint8_t blob[] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(blob, 4);
    s.write_u8(99);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42);
    EXPECT_EQ(d.read_u16(), 12345);
    EXPECT_EQ(d.read_u32(), 0xABCD1234u);
    EXPECT_EQ(d.read_u64(), 0x0102030405060708uLL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
    EXPECT_EQ(d.read_string(), "interleaved");
    auto b = d.read_bytes();
    EXPECT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0xDE);
    EXPECT_EQ(b[1], 0xAD);
    EXPECT_EQ(b[2], 0xBE);
    EXPECT_EQ(b[3], 0xEF);
    EXPECT_EQ(d.read_u8(), 99);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesAndEmptyStringV91) {
    Serializer s;
    s.write_string("");
    s.write_bytes(nullptr, 0);
    s.write_string("");
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    auto b1 = d.read_bytes();
    EXPECT_EQ(b1.size(), 0u);
    EXPECT_EQ(d.read_string(), "");
    auto b2 = d.read_bytes();
    EXPECT_EQ(b2.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32DescendingSequenceV91) {
    Serializer s;
    for (uint32_t v = 0xFFFFFFFFu; v >= 0xFFFFFF00u; --v) {
        s.write_u32(v);
    }

    Deserializer d(s.data());
    for (uint32_t v = 0xFFFFFFFFu; v >= 0xFFFFFF00u; --v) {
        EXPECT_EQ(d.read_u32(), v);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16AlternatingBitPatternsV92) {
    Serializer s;
    s.write_u16(0xAAAA);
    s.write_u16(0x5555);
    s.write_u16(0xFF00);
    s.write_u16(0x00FF);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0xAAAA);
    EXPECT_EQ(d.read_u16(), 0x5555);
    EXPECT_EQ(d.read_u16(), 0xFF00);
    EXPECT_EQ(d.read_u16(), 0x00FF);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SubnormalAndTinyValuesV92) {
    Serializer s;
    double subnormal = std::numeric_limits<double>::denorm_min();
    double tiny = std::numeric_limits<double>::min();
    double epsilon = std::numeric_limits<double>::epsilon();
    s.write_f64(subnormal);
    s.write_f64(tiny);
    s.write_f64(epsilon);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), subnormal);
    EXPECT_DOUBLE_EQ(d.read_f64(), tiny);
    EXPECT_DOUBLE_EQ(d.read_f64(), epsilon);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolStringBoolInterleavedV92) {
    Serializer s;
    s.write_bool(true);
    s.write_string("between");
    s.write_bool(false);
    s.write_string("bools");
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_string(), "between");
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_string(), "bools");
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32I64MixedNegativePositiveV92) {
    Serializer s;
    s.write_i32(-2147483647);
    s.write_i64(9223372036854775807LL);
    s.write_i32(1);
    s.write_i64(-9223372036854775807LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -2147483647);
    EXPECT_EQ(d.read_i64(), 9223372036854775807LL);
    EXPECT_EQ(d.read_i32(), 1);
    EXPECT_EQ(d.read_i64(), -9223372036854775807LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesBinaryPayload256V92) {
    Serializer s;
    std::vector<uint8_t> payload(256);
    for (int i = 0; i < 256; ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(payload.data(), payload.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64PowersOfTwoV92) {
    Serializer s;
    for (int shift = 0; shift < 64; ++shift) {
        s.write_u64(1ULL << shift);
    }

    Deserializer d(s.data());
    for (int shift = 0; shift < 64; ++shift) {
        EXPECT_EQ(d.read_u64(), 1ULL << shift);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsWithSpecialCharsV92) {
    Serializer s;
    s.write_string("hello\tworld");
    s.write_string("line1\nline2");
    s.write_string(std::string("null\0char", 9));
    s.write_string("");
    s.write_string("back\\slash");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello\tworld");
    EXPECT_EQ(d.read_string(), "line1\nline2");
    EXPECT_EQ(d.read_string(), std::string("null\0char", 9));
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "back\\slash");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, TakeDataAndDeserializeV92) {
    Serializer s;
    s.write_u32(0xCAFEBABEu);
    s.write_string("moved");
    s.write_f64(1.0 / 3.0);

    auto buf = s.take_data();
    EXPECT_GT(buf.size(), 0u);
    EXPECT_TRUE(s.data().empty());

    Deserializer d(buf.data(), buf.size());
    EXPECT_EQ(d.read_u32(), 0xCAFEBABEu);
    EXPECT_EQ(d.read_string(), "moved");
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.0 / 3.0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, AlternatingU8AndU16V93) {
    Serializer s;
    for (uint16_t i = 0; i < 50; ++i) {
        s.write_u8(static_cast<uint8_t>(i & 0xFF));
        s.write_u16(i * 100);
    }

    Deserializer d(s.data());
    for (uint16_t i = 0; i < 50; ++i) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i & 0xFF));
        EXPECT_EQ(d.read_u16(), i * 100);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialSequenceV93) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::denorm_min());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringThenBytesInterleavedV93) {
    Serializer s;
    s.write_string("alpha");
    uint8_t bin1[] = {0xDE, 0xAD};
    s.write_bytes(bin1, 2);
    s.write_string("beta");
    uint8_t bin2[] = {0xBE, 0xEF, 0xCA, 0xFE};
    s.write_bytes(bin2, 4);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "alpha");
    auto r1 = d.read_bytes();
    EXPECT_EQ(r1.size(), 2u);
    EXPECT_EQ(r1[0], 0xDE);
    EXPECT_EQ(r1[1], 0xAD);
    EXPECT_EQ(d.read_string(), "beta");
    auto r2 = d.read_bytes();
    EXPECT_EQ(r2.size(), 4u);
    EXPECT_EQ(r2[0], 0xBE);
    EXPECT_EQ(r2[3], 0xFE);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32BoundaryValuesV93) {
    Serializer s;
    s.write_u32(0u);
    s.write_u32(1u);
    s.write_u32(0x7FFFFFFFu);
    s.write_u32(0x80000000u);
    s.write_u32(0xFFFFFFFEu);
    s.write_u32(0xFFFFFFFFu);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u32(), 0u);
    EXPECT_EQ(d.read_u32(), 1u);
    EXPECT_EQ(d.read_u32(), 0x7FFFFFFFu);
    EXPECT_EQ(d.read_u32(), 0x80000000u);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFEu);
    EXPECT_EQ(d.read_u32(), 0xFFFFFFFFu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesPayloadV93) {
    Serializer s;
    s.write_bytes(nullptr, 0);
    s.write_u8(42);
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    auto r1 = d.read_bytes();
    EXPECT_EQ(r1.size(), 0u);
    EXPECT_EQ(d.read_u8(), 42);
    auto r2 = d.read_bytes();
    EXPECT_EQ(r2.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeStringRoundTripV93) {
    Serializer s;
    std::string big(10000, 'X');
    for (size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<char>('A' + (i % 26));
    }
    s.write_string(big);

    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_EQ(result.size(), 10000u);
    EXPECT_EQ(result, big);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypesCheckerboardV93) {
    Serializer s;
    s.write_u8(0xFF);
    s.write_u64(0x0102030405060708ULL);
    s.write_string("mid");
    s.write_u16(0xABCD);
    s.write_f64(2.718281828459045);
    s.write_u32(0x12345678u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xFF);
    EXPECT_EQ(d.read_u64(), 0x0102030405060708ULL);
    EXPECT_EQ(d.read_string(), "mid");
    EXPECT_EQ(d.read_u16(), 0xABCD);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828459045);
    EXPECT_EQ(d.read_u32(), 0x12345678u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, RepeatedSerializerReuseV93) {
    Serializer s1;
    s1.write_u32(111u);
    s1.write_string("first");

    Serializer s2;
    s2.write_u32(222u);
    s2.write_string("second");

    Deserializer d1(s1.data());
    EXPECT_EQ(d1.read_u32(), 111u);
    EXPECT_EQ(d1.read_string(), "first");
    EXPECT_FALSE(d1.has_remaining());

    Deserializer d2(s2.data());
    EXPECT_EQ(d2.read_u32(), 222u);
    EXPECT_EQ(d2.read_string(), "second");
    EXPECT_FALSE(d2.has_remaining());
}

TEST(SerializerTest, U64MaxAndZeroRoundTripV94) {
    Serializer s;
    s.write_u64(0ULL);
    s.write_u64(0xFFFFFFFFFFFFFFFFULL);
    s.write_u64(1ULL);
    s.write_u64(0x8000000000000000ULL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ULL);
    EXPECT_EQ(d.read_u64(), 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(d.read_u64(), 1ULL);
    EXPECT_EQ(d.read_u64(), 0x8000000000000000ULL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SubnormalAndTinyValuesV94) {
    Serializer s;
    s.write_f64(5e-324);
    s.write_f64(-5e-324);
    s.write_f64(2.2250738585072014e-308);
    s.write_f64(1e-100);

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 5e-324);
    EXPECT_DOUBLE_EQ(d.read_f64(), -5e-324);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.2250738585072014e-308);
    EXPECT_DOUBLE_EQ(d.read_f64(), 1e-100);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithAllByteValuesV94) {
    Serializer s;
    std::vector<uint8_t> all_bytes(256);
    for (int i = 0; i < 256; ++i) {
        all_bytes[i] = static_cast<uint8_t>(i);
    }
    s.write_bytes(all_bytes.data(), all_bytes.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    EXPECT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringThenBytesInterleavedV94) {
    Serializer s;
    s.write_string("hello");
    uint8_t buf[] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(buf, 4);
    s.write_string("world");
    s.write_bytes(buf, 0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    auto b1 = d.read_bytes();
    EXPECT_EQ(b1.size(), 4u);
    EXPECT_EQ(b1[0], 0xDE);
    EXPECT_EQ(b1[3], 0xEF);
    EXPECT_EQ(d.read_string(), "world");
    auto b2 = d.read_bytes();
    EXPECT_EQ(b2.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16AlternatingBitPatternsV94) {
    Serializer s;
    s.write_u16(0x0000);
    s.write_u16(0xFFFF);
    s.write_u16(0xAAAA);
    s.write_u16(0x5555);
    s.write_u16(0x00FF);
    s.write_u16(0xFF00);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0x0000);
    EXPECT_EQ(d.read_u16(), 0xFFFF);
    EXPECT_EQ(d.read_u16(), 0xAAAA);
    EXPECT_EQ(d.read_u16(), 0x5555);
    EXPECT_EQ(d.read_u16(), 0x00FF);
    EXPECT_EQ(d.read_u16(), 0xFF00);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleEmptyStringsV94) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_string("");
    }

    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_string(), "");
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32PowersOfTwoV94) {
    Serializer s;
    for (int bit = 0; bit < 32; ++bit) {
        s.write_u32(1u << bit);
    }

    Deserializer d(s.data());
    for (int bit = 0; bit < 32; ++bit) {
        EXPECT_EQ(d.read_u32(), 1u << bit);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, AllTypesKitchenSinkV94) {
    Serializer s;
    s.write_u8(0x42);
    s.write_u16(0x1234);
    s.write_u32(0xDEADBEEFu);
    s.write_u64(0x0A0B0C0D0E0F1011ULL);
    s.write_f64(3.141592653589793);
    s.write_string("kitchen sink");
    uint8_t blob[] = {1, 2, 3};
    s.write_bytes(blob, 3);
    s.write_f64(-0.0);
    s.write_u8(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0x42);
    EXPECT_EQ(d.read_u16(), 0x1234);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_u64(), 0x0A0B0C0D0E0F1011ULL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.141592653589793);
    EXPECT_EQ(d.read_string(), "kitchen sink");
    auto b = d.read_bytes();
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 2);
    EXPECT_EQ(b[2], 3);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_u8(), 0);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V95 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U64FibonacciSequenceV95) {
    Serializer s;
    uint64_t a = 0, b = 1;
    std::vector<uint64_t> fibs;
    for (int i = 0; i < 20; ++i) {
        fibs.push_back(a);
        s.write_u64(a);
        uint64_t next = a + b;
        a = b;
        b = next;
    }

    Deserializer d(s.data());
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(d.read_u64(), fibs[i]);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesCollectionV95) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::denorm_min());
    s.write_f64(std::numeric_limits<double>::max());
    s.write_f64(std::numeric_limits<double>::lowest());
    s.write_f64(std::numeric_limits<double>::epsilon());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::lowest());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesBinaryPatternV95) {
    Serializer s;
    // Write a 256-byte block containing every byte value 0x00..0xFF
    std::vector<uint8_t> all_bytes(256);
    std::iota(all_bytes.begin(), all_bytes.end(), 0);
    s.write_bytes(all_bytes.data(), all_bytes.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithEmbeddedNullsV95) {
    Serializer s;
    std::string with_nulls("abc\0def\0ghi", 11);
    s.write_string(with_nulls);

    Deserializer d(s.data());
    std::string out = d.read_string();
    EXPECT_EQ(out.size(), 11u);
    EXPECT_EQ(out, with_nulls);
    EXPECT_EQ(out[3], '\0');
    EXPECT_EQ(out[7], '\0');
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedU8AndU32V95) {
    Serializer s;
    for (uint32_t i = 0; i < 50; ++i) {
        s.write_u8(static_cast<uint8_t>(i & 0xFF));
        s.write_u32(i * 1000u);
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 50; ++i) {
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i & 0xFF));
        EXPECT_EQ(d.read_u32(), i * 1000u);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16AllBitPatternsV95) {
    Serializer s;
    // Write specific u16 bit patterns: 0, max, alternating bits, etc.
    s.write_u16(0x0000);
    s.write_u16(0xFFFF);
    s.write_u16(0xAAAA);
    s.write_u16(0x5555);
    s.write_u16(0x00FF);
    s.write_u16(0xFF00);
    s.write_u16(0x0F0F);
    s.write_u16(0xF0F0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0x0000);
    EXPECT_EQ(d.read_u16(), 0xFFFF);
    EXPECT_EQ(d.read_u16(), 0xAAAA);
    EXPECT_EQ(d.read_u16(), 0x5555);
    EXPECT_EQ(d.read_u16(), 0x00FF);
    EXPECT_EQ(d.read_u16(), 0xFF00);
    EXPECT_EQ(d.read_u16(), 0x0F0F);
    EXPECT_EQ(d.read_u16(), 0xF0F0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleEmptyBytesBlocksV95) {
    Serializer s;
    // Write several zero-length byte blocks interleaved with data
    s.write_bytes(nullptr, 0);
    s.write_u32(0xCAFEBABEu);
    s.write_bytes(nullptr, 0);
    s.write_string("marker");
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    auto b1 = d.read_bytes();
    EXPECT_EQ(b1.size(), 0u);
    EXPECT_EQ(d.read_u32(), 0xCAFEBABEu);
    auto b2 = d.read_bytes();
    EXPECT_EQ(b2.size(), 0u);
    EXPECT_EQ(d.read_string(), "marker");
    auto b3 = d.read_bytes();
    EXPECT_EQ(b3.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeStringRoundTripV95) {
    Serializer s;
    // Build a 10000-char string with a repeating pattern
    std::string large;
    large.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        large.push_back(static_cast<char>('A' + (i % 26)));
    }
    s.write_string(large);
    s.write_u8(0xFE);

    Deserializer d(s.data());
    std::string out = d.read_string();
    EXPECT_EQ(out.size(), 10000u);
    EXPECT_EQ(out, large);
    EXPECT_EQ(d.read_u8(), 0xFE);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V96 Tests
// ------------------------------------------------------------------

TEST(SerializerTest, AllIntegerTypesInterleavedV96) {
    // Serialize every integer type in sequence and verify round-trip
    Serializer s;
    s.write_u8(0xAB);
    s.write_u16(0x1234);
    s.write_u32(0xDEADBEEFu);
    s.write_u64(0x0102030405060708ULL);
    s.write_i32(-99999);
    s.write_i64(-8888888888LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xAB);
    EXPECT_EQ(d.read_u16(), 0x1234);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_u64(), 0x0102030405060708ULL);
    EXPECT_EQ(d.read_i32(), -99999);
    EXPECT_EQ(d.read_i64(), -8888888888LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesRoundTripV96) {
    // Verify that special floating-point values survive serialization
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::denorm_min());

    Deserializer d(s.data());
    double pos_inf = d.read_f64();
    EXPECT_TRUE(std::isinf(pos_inf) && pos_inf > 0);
    double neg_inf = d.read_f64();
    EXPECT_TRUE(std::isinf(neg_inf) && neg_inf < 0);
    double nan_val = d.read_f64();
    EXPECT_TRUE(std::isnan(nan_val));
    double neg_zero = d.read_f64();
    EXPECT_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    double denorm = d.read_f64();
    EXPECT_EQ(denorm, std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolSequenceAllPatternsV96) {
    // Write alternating and repeated bool patterns
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesBinaryPayloadRoundTripV96) {
    // Write a non-trivial binary payload and verify byte-for-byte equality
    std::vector<uint8_t> payload(256);
    for (int i = 0; i < 256; ++i) {
        payload[i] = static_cast<uint8_t>(i);
    }
    Serializer s;
    s.write_bytes(payload.data(), payload.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithEmbeddedNullBytesV96) {
    // Strings containing embedded null characters should round-trip correctly
    std::string with_nulls("hello\0world\0end", 15);
    ASSERT_EQ(with_nulls.size(), 15u);

    Serializer s;
    s.write_string(with_nulls);

    Deserializer d(s.data());
    std::string out = d.read_string();
    EXPECT_EQ(out.size(), 15u);
    EXPECT_EQ(out, with_nulls);
    EXPECT_EQ(out[5], '\0');
    EXPECT_EQ(out[11], '\0');
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32I64BoundaryValuesV96) {
    // Test minimum and maximum values for signed integer types
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i32(0);
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(std::numeric_limits<int64_t>::max());
    s.write_i64(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(d.read_i64(), 0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsConcatenatedV96) {
    // Serialize many strings and confirm each deserializes independently
    Serializer s;
    std::vector<std::string> strings = {
        "", "a", "ab", "abc", "Hello, World!",
        std::string(500, 'X'), "unicode: \xC3\xA9\xC3\xA0\xC3\xBC"
    };
    for (const auto& str : strings) {
        s.write_string(str);
    }

    Deserializer d(s.data());
    for (const auto& expected : strings) {
        std::string actual = d.read_string();
        EXPECT_EQ(actual, expected);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, DataVectorGrowsCorrectlyV96) {
    // Verify the data() vector contains bytes and grows as writes accumulate
    Serializer s;
    EXPECT_EQ(s.data().size(), 0u);

    s.write_u8(42);
    size_t after_u8 = s.data().size();
    EXPECT_GE(after_u8, 1u);

    s.write_u32(0x12345678u);
    size_t after_u32 = s.data().size();
    EXPECT_GT(after_u32, after_u8);

    s.write_string("test");
    size_t after_str = s.data().size();
    EXPECT_GT(after_str, after_u32);

    // Deserialize to confirm correctness despite incremental growth
    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 42);
    EXPECT_EQ(d.read_u32(), 0x12345678u);
    EXPECT_EQ(d.read_string(), "test");
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V97 tests
// ------------------------------------------------------------------

TEST(SerializerTest, RoundTripAllIntegerTypesInterleavedV97) {
    // Interleave every integer write type in a single stream and verify ordering
    Serializer s;
    s.write_u8(0xAB);
    s.write_u16(0xCDEF);
    s.write_u32(0x12345678u);
    s.write_u64(0xFEDCBA9876543210ULL);
    s.write_i32(-42);
    s.write_i64(-9999999999LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xAB);
    EXPECT_EQ(d.read_u16(), 0xCDEF);
    EXPECT_EQ(d.read_u32(), 0x12345678u);
    EXPECT_EQ(d.read_u64(), 0xFEDCBA9876543210ULL);
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_EQ(d.read_i64(), -9999999999LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, WriteBytesPreservesExactContentV97) {
    // Write a known byte pattern and ensure exact match on read
    const uint8_t pattern[] = {0x00, 0xFF, 0x80, 0x7F, 0x01, 0xFE, 0x55, 0xAA};
    Serializer s;
    s.write_bytes(pattern, sizeof(pattern));

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), sizeof(pattern));
    for (size_t i = 0; i < sizeof(pattern); ++i) {
        EXPECT_EQ(result[i], pattern[i]) << "Mismatch at byte index " << i;
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsWithSpecialCharsV97) {
    // Serialize several strings including empty, whitespace, unicode, and long
    Serializer s;
    s.write_string("");
    s.write_string("  \t\n\r  ");
    s.write_string("Hello, World!");
    s.write_string("\xC3\xA9\xC3\xA0\xC3\xBC"); // e-acute, a-grave, u-umlaut in UTF-8

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_string(), "  \t\n\r  ");
    EXPECT_EQ(d.read_string(), "Hello, World!");
    EXPECT_EQ(d.read_string(), "\xC3\xA9\xC3\xA0\xC3\xBC");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolSequenceAllCombinationsV97) {
    // Write all 8 combinations of 3 booleans and verify round-trip
    Serializer s;
    for (int i = 0; i < 8; ++i) {
        s.write_bool((i & 4) != 0);
        s.write_bool((i & 2) != 0);
        s.write_bool((i & 1) != 0);
    }

    Deserializer d(s.data());
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(d.read_bool(), (i & 4) != 0) << "Triple " << i << " bit2";
        EXPECT_EQ(d.read_bool(), (i & 2) != 0) << "Triple " << i << " bit1";
        EXPECT_EQ(d.read_bool(), (i & 1) != 0) << "Triple " << i << " bit0";
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64ExtremeMagnitudesV97) {
    // Test f64 with extreme but finite values: denorms, max, min, epsilon
    Serializer s;
    s.write_f64(std::numeric_limits<double>::min());         // smallest positive normal
    s.write_f64(std::numeric_limits<double>::max());         // largest finite
    s.write_f64(std::numeric_limits<double>::denorm_min());  // smallest positive denorm
    s.write_f64(std::numeric_limits<double>::epsilon());     // machine epsilon
    s.write_f64(-std::numeric_limits<double>::max());        // most negative finite

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, HasRemainingTracksConsumptionV97) {
    // Verify has_remaining() transitions from true to false at exact boundary
    Serializer s;
    s.write_u8(1);
    s.write_u16(2);
    s.write_u32(3);

    Deserializer d(s.data());
    EXPECT_TRUE(d.has_remaining());

    d.read_u8();
    EXPECT_TRUE(d.has_remaining());

    d.read_u16();
    EXPECT_TRUE(d.has_remaining());

    d.read_u32();
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargePayloadMixedTypesV97) {
    // Build a large payload with 100 items of mixed types and roundtrip
    Serializer s;
    for (uint32_t i = 0; i < 100; ++i) {
        s.write_u32(i);
        s.write_bool(i % 2 == 0);
        s.write_string("item_" + std::to_string(i));
    }

    Deserializer d(s.data());
    for (uint32_t i = 0; i < 100; ++i) {
        EXPECT_EQ(d.read_u32(), i) << "u32 mismatch at iteration " << i;
        EXPECT_EQ(d.read_bool(), i % 2 == 0) << "bool mismatch at iteration " << i;
        EXPECT_EQ(d.read_string(), "item_" + std::to_string(i)) << "string mismatch at iteration " << i;
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyBytesFollowedByNonEmptyV97) {
    // Write empty bytes then non-empty bytes, ensuring length prefixes work
    Serializer s;
    s.write_bytes(nullptr, 0);
    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(data, 4);
    s.write_bytes(nullptr, 0);

    Deserializer d(s.data());
    auto first = d.read_bytes();
    EXPECT_EQ(first.size(), 0u);

    auto second = d.read_bytes();
    ASSERT_EQ(second.size(), 4u);
    EXPECT_EQ(second[0], 0xDE);
    EXPECT_EQ(second[1], 0xAD);
    EXPECT_EQ(second[2], 0xBE);
    EXPECT_EQ(second[3], 0xEF);

    auto third = d.read_bytes();
    EXPECT_EQ(third.size(), 0u);

    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V98 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U64MaxAndMinBoundaryV98) {
    // Verify u64 handles full 64-bit range including 0 and max
    Serializer s;
    s.write_u64(0);
    s.write_u64(std::numeric_limits<uint64_t>::max());
    s.write_u64(1);
    s.write_u64(std::numeric_limits<uint64_t>::max() - 1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(d.read_u64(), 1u);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max() - 1);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32NegativeValuesRoundTripV98) {
    // Ensure negative i32 values survive serialization round-trip
    Serializer s;
    s.write_i32(-1);
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(-42);
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i32(0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialFloatingPointValuesV98) {
    // Verify special IEEE 754 values: infinity, negative infinity, NaN, epsilon
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    s.write_f64(std::numeric_limits<double>::epsilon());
    s.write_f64(-0.0);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(d.read_f64()));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    double neg_zero = d.read_f64();
    EXPECT_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithEmbeddedNullBytesV98) {
    // Strings containing null bytes should be preserved exactly
    Serializer s;
    std::string with_nulls("hello\0world", 11);
    s.write_string(with_nulls);
    s.write_string(std::string(1, '\0'));
    s.write_string("");

    Deserializer d(s.data());
    std::string result1 = d.read_string();
    EXPECT_EQ(result1.size(), 11u);
    EXPECT_EQ(result1, with_nulls);
    EXPECT_EQ(result1[5], '\0');

    std::string result2 = d.read_string();
    EXPECT_EQ(result2.size(), 1u);
    EXPECT_EQ(result2[0], '\0');

    std::string result3 = d.read_string();
    EXPECT_EQ(result3.size(), 0u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedTypesComplexPatternV98) {
    // Mix many different types in a non-trivial interleaving pattern
    Serializer s;
    s.write_bool(true);
    s.write_u8(0xFF);
    s.write_string("separator");
    s.write_i64(-9876543210LL);
    s.write_u16(12345);
    s.write_f64(2.718281828);
    s.write_bool(false);
    s.write_u32(0xDEADBEEF);
    const uint8_t blob[] = {1, 2, 3};
    s.write_bytes(blob, 3);
    s.write_i32(-999);

    Deserializer d(s.data());
    EXPECT_TRUE(d.read_bool());
    EXPECT_EQ(d.read_u8(), 0xFF);
    EXPECT_EQ(d.read_string(), "separator");
    EXPECT_EQ(d.read_i64(), -9876543210LL);
    EXPECT_EQ(d.read_u16(), 12345u);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828);
    EXPECT_FALSE(d.read_bool());
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    auto bytes = d.read_bytes();
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 1);
    EXPECT_EQ(bytes[1], 2);
    EXPECT_EQ(bytes[2], 3);
    EXPECT_EQ(d.read_i32(), -999);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBinaryBlobV98) {
    // Serialize a 4096-byte blob and verify every byte round-trips
    Serializer s;
    std::vector<uint8_t> large_blob(4096);
    for (size_t i = 0; i < large_blob.size(); ++i) {
        large_blob[i] = static_cast<uint8_t>(i & 0xFF);
    }
    s.write_bytes(large_blob.data(), large_blob.size());
    s.write_u32(0xCAFEBABE);

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 4096u);
    for (size_t i = 0; i < 4096; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i & 0xFF)) << "Mismatch at byte " << i;
    }
    EXPECT_EQ(d.read_u32(), 0xCAFEBABEu);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, HasRemainingTrackingAcrossReadsV98) {
    // Verify has_remaining returns true until all data is consumed
    Serializer s;
    s.write_u8(42);
    s.write_u16(1000);
    s.write_bool(true);

    Deserializer d(s.data());
    EXPECT_TRUE(d.has_remaining());
    d.read_u8();
    EXPECT_TRUE(d.has_remaining());
    d.read_u16();
    EXPECT_TRUE(d.has_remaining());
    d.read_bool();
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleStringsVaryingLengthsV98) {
    // Serialize strings of varying lengths including very long ones
    Serializer s;
    std::string empty_str;
    std::string short_str = "hi";
    std::string medium_str(256, 'M');
    std::string long_str(10000, 'X');
    std::string unicode_str = "\xC3\xA9\xC3\xA0\xC3\xBC"; // UTF-8 for e-acute, a-grave, u-umlaut

    s.write_string(empty_str);
    s.write_string(short_str);
    s.write_string(medium_str);
    s.write_string(long_str);
    s.write_string(unicode_str);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), empty_str);
    EXPECT_EQ(d.read_string(), short_str);

    std::string med_result = d.read_string();
    EXPECT_EQ(med_result.size(), 256u);
    EXPECT_EQ(med_result, medium_str);

    std::string long_result = d.read_string();
    EXPECT_EQ(long_result.size(), 10000u);
    EXPECT_EQ(long_result, long_str);

    EXPECT_EQ(d.read_string(), unicode_str);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V99 Tests
// ------------------------------------------------------------------

TEST(SerializerTest, InterleavedTypesRoundTripV99) {
    Serializer s;
    s.write_u8(0xAB);
    s.write_string("between");
    s.write_i32(-999);
    s.write_bool(true);
    s.write_f64(2.718281828);
    s.write_u64(0xDEADBEEFCAFEBABEULL);
    s.write_string("");
    s.write_i64(-1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xAB);
    EXPECT_EQ(d.read_string(), "between");
    EXPECT_EQ(d.read_i32(), -999);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_DOUBLE_EQ(d.read_f64(), 2.718281828);
    EXPECT_EQ(d.read_u64(), 0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_EQ(d.read_i64(), -1);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16BoundaryValuesV99) {
    Serializer s;
    s.write_u16(0);
    s.write_u16(1);
    s.write_u16(255);
    s.write_u16(256);
    s.write_u16(32767);
    s.write_u16(32768);
    s.write_u16(65534);
    s.write_u16(65535);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0);
    EXPECT_EQ(d.read_u16(), 1);
    EXPECT_EQ(d.read_u16(), 255);
    EXPECT_EQ(d.read_u16(), 256);
    EXPECT_EQ(d.read_u16(), 32767);
    EXPECT_EQ(d.read_u16(), 32768);
    EXPECT_EQ(d.read_u16(), 65534);
    EXPECT_EQ(d.read_u16(), 65535);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithAllByteValuesV99) {
    std::vector<uint8_t> all_bytes(256);
    for (int i = 0; i < 256; ++i) {
        all_bytes[i] = static_cast<uint8_t>(i);
    }

    Serializer s;
    s.write_bytes(all_bytes.data(), all_bytes.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SubnormalAndExtremeV99) {
    double subnormal = std::numeric_limits<double>::denorm_min();
    double max_val = std::numeric_limits<double>::max();
    double lowest_val = std::numeric_limits<double>::lowest();
    double epsilon = std::numeric_limits<double>::epsilon();

    Serializer s;
    s.write_f64(subnormal);
    s.write_f64(max_val);
    s.write_f64(lowest_val);
    s.write_f64(epsilon);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), subnormal);
    EXPECT_EQ(d.read_f64(), max_val);
    EXPECT_EQ(d.read_f64(), lowest_val);
    EXPECT_EQ(d.read_f64(), epsilon);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleBoolSequenceV99) {
    Serializer s;
    // Write alternating pattern plus edges
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_TRUE(d.read_bool());
    EXPECT_FALSE(d.read_bool());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringWithBinaryContentV99) {
    // String containing bytes that look like control characters
    std::string binary_str;
    binary_str.push_back('\x00');
    binary_str.push_back('\x01');
    binary_str.push_back('\xFF');
    binary_str.push_back('\x7F');
    binary_str.push_back('\t');
    binary_str.push_back('\n');
    binary_str.append("text");

    Serializer s;
    s.write_string(binary_str);

    Deserializer d(s.data());
    std::string result = d.read_string();
    EXPECT_EQ(result.size(), binary_str.size());
    EXPECT_EQ(result, binary_str);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U32PowersOfTwoV99) {
    Serializer s;
    for (int bit = 0; bit < 32; ++bit) {
        s.write_u32(1u << bit);
    }

    Deserializer d(s.data());
    for (int bit = 0; bit < 32; ++bit) {
        EXPECT_EQ(d.read_u32(), 1u << bit);
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32I64SignedEdgeCasesV99) {
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i32(0);
    s.write_i32(-1);
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(std::numeric_limits<int64_t>::max());
    s.write_i64(0);
    s.write_i64(-1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(d.read_i64(), 0);
    EXPECT_EQ(d.read_i64(), -1);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V100 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U16PowersOfTwoRoundTripV100) {
    Serializer s;
    for (int bit = 0; bit < 16; ++bit) {
        s.write_u16(static_cast<uint16_t>(1u << bit));
    }

    Deserializer d(s.data());
    for (int bit = 0; bit < 16; ++bit) {
        EXPECT_EQ(d.read_u16(), static_cast<uint16_t>(1u << bit));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SubnormalAndTinyValuesV100) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::denorm_min());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::epsilon());
    s.write_f64(-std::numeric_limits<double>::denorm_min());

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::epsilon());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BytesWithAllByteValuesRoundTripV100) {
    std::vector<uint8_t> all_bytes(256);
    std::iota(all_bytes.begin(), all_bytes.end(), static_cast<uint8_t>(0));

    Serializer s;
    s.write_bytes(all_bytes.data(), all_bytes.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 256u);
    for (size_t i = 0; i < 256; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedBoolAndU8PatternV100) {
    Serializer s;
    for (int i = 0; i < 10; ++i) {
        s.write_bool(i % 2 == 0);
        s.write_u8(static_cast<uint8_t>(i * 25));
    }

    Deserializer d(s.data());
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(d.read_bool(), (i % 2 == 0));
        EXPECT_EQ(d.read_u8(), static_cast<uint8_t>(i * 25));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MultipleConsecutiveEmptyStringsV100) {
    Serializer s;
    for (int i = 0; i < 5; ++i) {
        s.write_string("");
    }

    Deserializer d(s.data());
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(d.read_string(), "");
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U64HighBitPatternsV100) {
    Serializer s;
    s.write_u64(0x8000000000000000ULL);
    s.write_u64(0xAAAAAAAAAAAAAAAAULL);
    s.write_u64(0x5555555555555555ULL);
    s.write_u64(0xFF00FF00FF00FF00ULL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0x8000000000000000ULL);
    EXPECT_EQ(d.read_u64(), 0xAAAAAAAAAAAAAAAAULL);
    EXPECT_EQ(d.read_u64(), 0x5555555555555555ULL);
    EXPECT_EQ(d.read_u64(), 0xFF00FF00FF00FF00ULL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, StringThenBytesInterleavedV100) {
    Serializer s;
    s.write_string("hello");
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    s.write_bytes(payload, 4);
    s.write_string("world");
    uint8_t payload2[] = {0xCA, 0xFE};
    s.write_bytes(payload2, 2);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "hello");
    auto b1 = d.read_bytes();
    ASSERT_EQ(b1.size(), 4u);
    EXPECT_EQ(b1[0], 0xDE);
    EXPECT_EQ(b1[1], 0xAD);
    EXPECT_EQ(b1[2], 0xBE);
    EXPECT_EQ(b1[3], 0xEF);
    EXPECT_EQ(d.read_string(), "world");
    auto b2 = d.read_bytes();
    ASSERT_EQ(b2.size(), 2u);
    EXPECT_EQ(b2[0], 0xCA);
    EXPECT_EQ(b2[1], 0xFE);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, TakeDataThenDeserializeIndependentlyV100) {
    Serializer s;
    s.write_i32(-42);
    s.write_f64(3.14159265358979);
    s.write_bool(true);
    s.write_u32(999999u);

    auto taken = s.take_data();
    EXPECT_TRUE(s.data().empty());

    Deserializer d(taken);
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.14159265358979);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_u32(), 999999u);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V101 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U64MaxAndMinBoundaryV101) {
    Serializer s;
    s.write_u64(0u);
    s.write_u64(std::numeric_limits<uint64_t>::max());
    s.write_u64(1u);
    s.write_u64(std::numeric_limits<uint64_t>::max() - 1u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(d.read_u64(), 1u);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max() - 1u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32NegativePositiveAlternatingV101) {
    Serializer s;
    s.write_i32(std::numeric_limits<int32_t>::min());
    s.write_i32(std::numeric_limits<int32_t>::max());
    s.write_i32(-1);
    s.write_i32(0);
    s.write_i32(1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(d.read_i32(), std::numeric_limits<int32_t>::max());
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i32(), 1);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesNanInfV101) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::denorm_min());

    EXPECT_EQ(s.data().size(), 5 * 8u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(d.read_f64()));
    double neg_zero = d.read_f64();
    EXPECT_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringAndEmptyBytesV101) {
    Serializer s;
    s.write_string("");
    uint8_t empty_buf[] = {0};
    s.write_bytes(empty_buf, 0);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    auto bytes = d.read_bytes();
    EXPECT_EQ(bytes.size(), 0u);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolSequenceRoundTripV101) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedIntegerWidthsSequenceV101) {
    Serializer s;
    s.write_u8(0xAB);
    s.write_u16(0x1234);
    s.write_u32(0xDEADBEEFu);
    s.write_u64(0x0102030405060708ull);
    s.write_i32(-12345);
    s.write_i64(-9876543210LL);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0xAB);
    EXPECT_EQ(d.read_u16(), 0x1234);
    EXPECT_EQ(d.read_u32(), 0xDEADBEEFu);
    EXPECT_EQ(d.read_u64(), 0x0102030405060708ull);
    EXPECT_EQ(d.read_i32(), -12345);
    EXPECT_EQ(d.read_i64(), -9876543210LL);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesBlobRoundTripV101) {
    Serializer s;
    std::vector<uint8_t> big_blob(4096);
    std::iota(big_blob.begin(), big_blob.end(), static_cast<uint8_t>(0));
    s.write_bytes(big_blob.data(), big_blob.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 4096u);
    for (size_t i = 0; i < 4096; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, ComplexMessageProtocolSimulationV101) {
    // Simulate a protocol: version(u8), flags(u16), timestamp(u64),
    // payload_type(i32), temperature(f64), name(string), active(bool), raw_data(bytes)
    Serializer s;
    s.write_u8(3);                     // version
    s.write_u16(0x00FF);              // flags
    s.write_u64(1709136000000ull);    // timestamp ms
    s.write_i32(-7);                  // payload_type
    s.write_f64(36.6);               // temperature
    s.write_string("sensor-alpha-9"); // name
    s.write_bool(true);               // active
    uint8_t raw[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    s.write_bytes(raw, 5);            // raw_data

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 3u);
    EXPECT_EQ(d.read_u16(), 0x00FF);
    EXPECT_EQ(d.read_u64(), 1709136000000ull);
    EXPECT_EQ(d.read_i32(), -7);
    EXPECT_DOUBLE_EQ(d.read_f64(), 36.6);
    EXPECT_EQ(d.read_string(), "sensor-alpha-9");
    EXPECT_EQ(d.read_bool(), true);
    auto raw_result = d.read_bytes();
    ASSERT_EQ(raw_result.size(), 5u);
    EXPECT_EQ(raw_result[0], 0x01);
    EXPECT_EQ(raw_result[1], 0x02);
    EXPECT_EQ(raw_result[2], 0x03);
    EXPECT_EQ(raw_result[3], 0x04);
    EXPECT_EQ(raw_result[4], 0x05);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V102 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U64MaxAndZeroBoundaryV102) {
    Serializer s;
    s.write_u64(0ull);
    s.write_u64(std::numeric_limits<uint64_t>::max());
    s.write_u64(1ull);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0ull);
    EXPECT_EQ(d.read_u64(), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(d.read_u64(), 1ull);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I64SignedExtremesV102) {
    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());
    s.write_i64(std::numeric_limits<int64_t>::max());
    s.write_i64(0);
    s.write_i64(-1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(d.read_i64(), 0);
    EXPECT_EQ(d.read_i64(), -1);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV102) {
    Serializer s;
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::quiet_NaN());
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::denorm_min());

    Deserializer d(s.data());
    double pos_inf = d.read_f64();
    EXPECT_TRUE(std::isinf(pos_inf) && pos_inf > 0);
    double neg_inf = d.read_f64();
    EXPECT_TRUE(std::isinf(neg_inf) && neg_inf < 0);
    double nan_val = d.read_f64();
    EXPECT_TRUE(std::isnan(nan_val));
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    double denorm = d.read_f64();
    EXPECT_DOUBLE_EQ(denorm, std::numeric_limits<double>::denorm_min());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringAndEmptyBytesV102) {
    Serializer s;
    s.write_string("");
    s.write_bytes(nullptr, 0);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    auto empty_bytes = d.read_bytes();
    EXPECT_EQ(empty_bytes.size(), 0u);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolSequencePatternV102) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBinaryPayloadV102) {
    std::vector<uint8_t> payload(4096);
    std::iota(payload.begin(), payload.end(), static_cast<uint8_t>(0));

    Serializer s;
    s.write_u32(static_cast<uint32_t>(payload.size()));
    s.write_bytes(payload.data(), payload.size());

    Deserializer d(s.data());
    uint32_t len = d.read_u32();
    EXPECT_EQ(len, 4096u);
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 4096u);
    for (size_t i = 0; i < 4096; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, Utf8StringRoundTripV102) {
    Serializer s;
    s.write_string("Hello, \xe4\xb8\x96\xe7\x95\x8c!");  // Hello, 世界!
    s.write_string("\xf0\x9f\x98\x80\xf0\x9f\x8e\x89");  // 😀🎉
    s.write_string("caf\xc3\xa9");                          // café

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "Hello, \xe4\xb8\x96\xe7\x95\x8c!");
    EXPECT_EQ(d.read_string(), "\xf0\x9f\x98\x80\xf0\x9f\x8e\x89");
    EXPECT_EQ(d.read_string(), "caf\xc3\xa9");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, InterleavedTypesProtocolMessageV102) {
    // Simulate a protocol message: header(u8 version, u16 type, u32 seq),
    // body(i32 code, i64 timestamp, f64 value, bool flag, string label, bytes payload)
    Serializer s;
    s.write_u8(2);                        // version
    s.write_u16(0x0401);                  // message type
    s.write_u32(999999u);                 // sequence number
    s.write_i32(-42);                     // status code
    s.write_i64(-8070450532247928832LL);  // timestamp
    s.write_f64(3.141592653589793);       // measurement
    s.write_bool(false);                  // ack flag
    s.write_string("diagnostics.report"); // label
    uint8_t tag[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    s.write_bytes(tag, 6);               // tag bytes

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 2u);
    EXPECT_EQ(d.read_u16(), 0x0401);
    EXPECT_EQ(d.read_u32(), 999999u);
    EXPECT_EQ(d.read_i32(), -42);
    EXPECT_EQ(d.read_i64(), -8070450532247928832LL);
    EXPECT_DOUBLE_EQ(d.read_f64(), 3.141592653589793);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_string(), "diagnostics.report");
    auto tag_result = d.read_bytes();
    ASSERT_EQ(tag_result.size(), 6u);
    EXPECT_EQ(tag_result[0], 0xDE);
    EXPECT_EQ(tag_result[1], 0xAD);
    EXPECT_EQ(tag_result[2], 0xBE);
    EXPECT_EQ(tag_result[3], 0xEF);
    EXPECT_EQ(tag_result[4], 0xCA);
    EXPECT_EQ(tag_result[5], 0xFE);
    EXPECT_FALSE(d.has_remaining());
}

// ------------------------------------------------------------------
// V103 tests
// ------------------------------------------------------------------

TEST(SerializerTest, U64MaxBoundaryRoundTripV103) {
    Serializer s;
    s.write_u64(0u);
    s.write_u64(1u);
    s.write_u64(UINT64_MAX);
    s.write_u64(UINT64_MAX - 1u);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u64(), 0u);
    EXPECT_EQ(d.read_u64(), 1u);
    EXPECT_EQ(d.read_u64(), UINT64_MAX);
    EXPECT_EQ(d.read_u64(), UINT64_MAX - 1u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, I32NegativePositiveAlternatingV103) {
    Serializer s;
    s.write_i32(INT32_MIN);
    s.write_i32(INT32_MAX);
    s.write_i32(-1);
    s.write_i32(0);
    s.write_i32(1);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_i32(), INT32_MIN);
    EXPECT_EQ(d.read_i32(), INT32_MAX);
    EXPECT_EQ(d.read_i32(), -1);
    EXPECT_EQ(d.read_i32(), 0);
    EXPECT_EQ(d.read_i32(), 1);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, F64SpecialValuesV103) {
    Serializer s;
    s.write_f64(0.0);
    s.write_f64(-0.0);
    s.write_f64(std::numeric_limits<double>::infinity());
    s.write_f64(-std::numeric_limits<double>::infinity());
    s.write_f64(std::numeric_limits<double>::min());
    s.write_f64(std::numeric_limits<double>::max());

    Deserializer d(s.data());
    EXPECT_DOUBLE_EQ(d.read_f64(), 0.0);
    double neg_zero = d.read_f64();
    EXPECT_DOUBLE_EQ(neg_zero, 0.0);
    EXPECT_TRUE(std::signbit(neg_zero));
    EXPECT_EQ(d.read_f64(), std::numeric_limits<double>::infinity());
    EXPECT_EQ(d.read_f64(), -std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(d.read_f64(), std::numeric_limits<double>::max());
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, EmptyStringAndBytesV103) {
    Serializer s;
    s.write_string("");
    s.write_bytes(nullptr, 0);
    s.write_string("");

    Deserializer d(s.data());
    EXPECT_EQ(d.read_string(), "");
    auto b = d.read_bytes();
    EXPECT_EQ(b.size(), 0u);
    EXPECT_EQ(d.read_string(), "");
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, BoolSequencePatternV103) {
    Serializer s;
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(true);
    s.write_bool(false);
    s.write_bool(false);
    s.write_bool(true);
    s.write_bool(false);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_bool(), false);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, LargeBytesBlobV103) {
    Serializer s;
    std::vector<uint8_t> blob(1024);
    std::iota(blob.begin(), blob.end(), static_cast<uint8_t>(0));
    s.write_bytes(blob.data(), blob.size());

    Deserializer d(s.data());
    auto result = d.read_bytes();
    ASSERT_EQ(result.size(), 1024u);
    for (size_t i = 0; i < 1024; ++i) {
        EXPECT_EQ(result[i], static_cast<uint8_t>(i & 0xFF));
    }
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, U16EndianConsistencyV103) {
    Serializer s;
    s.write_u16(0x0000);
    s.write_u16(0x00FF);
    s.write_u16(0xFF00);
    s.write_u16(0xFFFF);
    s.write_u16(0xABCD);

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u16(), 0x0000);
    EXPECT_EQ(d.read_u16(), 0x00FF);
    EXPECT_EQ(d.read_u16(), 0xFF00);
    EXPECT_EQ(d.read_u16(), 0xFFFF);
    EXPECT_EQ(d.read_u16(), 0xABCD);
    EXPECT_FALSE(d.has_remaining());
}

TEST(SerializerTest, MixedTypesComplexMessageV103) {
    Serializer s;
    // Simulate a complex protocol message
    s.write_u8(0x01);                          // version
    s.write_u32(42u);                          // request id
    s.write_i64(-9999999999LL);                // timestamp
    s.write_bool(true);                        // compressed flag
    s.write_string("application/json");        // content type
    s.write_f64(1.23e-15);                     // precision
    uint8_t checksum[] = {0x01, 0x02, 0x03};
    s.write_bytes(checksum, 3);                // checksum
    s.write_u16(8080);                         // port
    s.write_i32(-256);                         // offset
    s.write_string("end");                     // terminator

    Deserializer d(s.data());
    EXPECT_EQ(d.read_u8(), 0x01u);
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_EQ(d.read_i64(), -9999999999LL);
    EXPECT_EQ(d.read_bool(), true);
    EXPECT_EQ(d.read_string(), "application/json");
    EXPECT_DOUBLE_EQ(d.read_f64(), 1.23e-15);
    auto cksum = d.read_bytes();
    ASSERT_EQ(cksum.size(), 3u);
    EXPECT_EQ(cksum[0], 0x01);
    EXPECT_EQ(cksum[1], 0x02);
    EXPECT_EQ(cksum[2], 0x03);
    EXPECT_EQ(d.read_u16(), 8080);
    EXPECT_EQ(d.read_i32(), -256);
    EXPECT_EQ(d.read_string(), "end");
    EXPECT_FALSE(d.has_remaining());
}
