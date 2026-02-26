#include <gtest/gtest.h>
#include <clever/url/percent_encoding.h>
#include <string>

using namespace clever::url;

// =============================================================================
// percent_encode tests
// =============================================================================
TEST(PercentEncoding, NoEncodingNeeded) {
    EXPECT_EQ(percent_encode("hello"), "hello");
}

TEST(PercentEncoding, SpaceEncoding) {
    EXPECT_EQ(percent_encode("hello world"), "hello%20world");
}

TEST(PercentEncoding, MultipleSpecialChars) {
    EXPECT_EQ(percent_encode("a b<c>d"), "a%20b%3Cc%3Ed");
}

TEST(PercentEncoding, AlreadyEncodedPassthrough) {
    // Percent signs that are part of valid percent-encoded sequences should
    // still get double-encoded (we encode the input literally)
    EXPECT_EQ(percent_encode("%20"), "%2520");
}

TEST(PercentEncoding, EmptyString) {
    EXPECT_EQ(percent_encode(""), "");
}

TEST(PercentEncoding, AllAsciiLettersUnchanged) {
    std::string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    EXPECT_EQ(percent_encode(letters), letters);
}

TEST(PercentEncoding, DigitsUnchanged) {
    EXPECT_EQ(percent_encode("0123456789"), "0123456789");
}

TEST(PercentEncoding, HyphenDotUnderscoreTildeUnchanged) {
    EXPECT_EQ(percent_encode("-._~"), "-._~");
}

TEST(PercentEncoding, PathCharsNotEncodedByDefault) {
    // By default, path chars like / : @ are NOT encoded
    EXPECT_EQ(percent_encode("/path"), "/path");
}

TEST(PercentEncoding, PathCharsEncodedWhenFlagged) {
    EXPECT_EQ(percent_encode("/path", true), "%2Fpath");
}

TEST(PercentEncoding, HighByteEncoding) {
    // Non-ASCII byte 0xC3 0xA9 (UTF-8 for 'e' with accent)
    std::string input = "\xC3\xA9";
    EXPECT_EQ(percent_encode(input), "%C3%A9");
}

// =============================================================================
// percent_decode tests
// =============================================================================
TEST(PercentDecoding, NoDecodingNeeded) {
    EXPECT_EQ(percent_decode("hello"), "hello");
}

TEST(PercentDecoding, DecodeSpace) {
    EXPECT_EQ(percent_decode("hello%20world"), "hello world");
}

TEST(PercentDecoding, DecodeMultiple) {
    EXPECT_EQ(percent_decode("%48%65%6C%6C%6F"), "Hello");
}

TEST(PercentDecoding, DecodeLowerHex) {
    EXPECT_EQ(percent_decode("%2f"), "/");
}

TEST(PercentDecoding, IncompletePercentSequence) {
    // Incomplete percent encoding should be left as-is
    EXPECT_EQ(percent_decode("hello%2"), "hello%2");
}

TEST(PercentDecoding, PercentAtEnd) {
    EXPECT_EQ(percent_decode("hello%"), "hello%");
}

TEST(PercentDecoding, InvalidHexDigit) {
    EXPECT_EQ(percent_decode("%GG"), "%GG");
}

TEST(PercentDecoding, EmptyString) {
    EXPECT_EQ(percent_decode(""), "");
}

TEST(PercentDecoding, MixedEncodedAndPlain) {
    EXPECT_EQ(percent_decode("a%20b%20c"), "a b c");
}

// =============================================================================
// is_url_code_point tests
// =============================================================================
TEST(IsURLCodePoint, AsciiLetters) {
    EXPECT_TRUE(is_url_code_point('a'));
    EXPECT_TRUE(is_url_code_point('z'));
    EXPECT_TRUE(is_url_code_point('A'));
    EXPECT_TRUE(is_url_code_point('Z'));
}

TEST(IsURLCodePoint, Digits) {
    EXPECT_TRUE(is_url_code_point('0'));
    EXPECT_TRUE(is_url_code_point('9'));
}

TEST(IsURLCodePoint, SpecialAllowedChars) {
    // These are URL code points per the WHATWG spec
    EXPECT_TRUE(is_url_code_point('!'));
    EXPECT_TRUE(is_url_code_point('$'));
    EXPECT_TRUE(is_url_code_point('&'));
    EXPECT_TRUE(is_url_code_point('\''));
    EXPECT_TRUE(is_url_code_point('('));
    EXPECT_TRUE(is_url_code_point(')'));
    EXPECT_TRUE(is_url_code_point('*'));
    EXPECT_TRUE(is_url_code_point('+'));
    EXPECT_TRUE(is_url_code_point(','));
    EXPECT_TRUE(is_url_code_point('-'));
    EXPECT_TRUE(is_url_code_point('.'));
    EXPECT_TRUE(is_url_code_point('/'));
    EXPECT_TRUE(is_url_code_point(':'));
    EXPECT_TRUE(is_url_code_point(';'));
    EXPECT_TRUE(is_url_code_point('='));
    EXPECT_TRUE(is_url_code_point('@'));
    EXPECT_TRUE(is_url_code_point('_'));
    EXPECT_TRUE(is_url_code_point('~'));
}

TEST(IsURLCodePoint, ControlCharsNotURLCodePoints) {
    EXPECT_FALSE(is_url_code_point('\0'));
    EXPECT_FALSE(is_url_code_point('\t'));
    EXPECT_FALSE(is_url_code_point('\n'));
    EXPECT_FALSE(is_url_code_point(' '));
}

TEST(IsURLCodePoint, ForbiddenPrintableCharsNotURLCodePoints) {
    // These printable ASCII chars are NOT URL code points per WHATWG URL spec
    EXPECT_FALSE(is_url_code_point('"'));
    EXPECT_FALSE(is_url_code_point('<'));
    EXPECT_FALSE(is_url_code_point('>'));
    EXPECT_FALSE(is_url_code_point('\\'));
    EXPECT_FALSE(is_url_code_point('^'));
    EXPECT_FALSE(is_url_code_point('`'));
    EXPECT_FALSE(is_url_code_point('{'));
    EXPECT_FALSE(is_url_code_point('|'));
    EXPECT_FALSE(is_url_code_point('}'));
}

TEST(IsURLCodePoint, PercentSignNotURLCodePoint) {
    // '%' is not itself a URL code point (it's the escape introducer)
    EXPECT_FALSE(is_url_code_point('%'));
}

TEST(IsURLCodePoint, QuestionMarkIsURLCodePoint) {
    // '?' (U+003F) is listed as a URL code point in WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('?'));
}

TEST(IsURLCodePoint, HashIsNotURLCodePoint) {
    // '#' (U+0023) is a URL syntax delimiter, not a URL code point
    EXPECT_FALSE(is_url_code_point('#'));
}

TEST(PercentDecoding, DecodeNULByte) {
    std::string result = percent_decode("%00");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(result[0]), 0x00);
}

TEST(PercentDecoding, DecodeDELByte) {
    std::string result = percent_decode("%7F");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(result[0]), 0x7F);
}

TEST(PercentDecoding, DecodeUTF8MultiByteSequence) {
    // %C3%A4 is UTF-8 encoding of ä (U+00E4)
    std::string result = percent_decode("%C3%A4");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(result[0]), 0xC3);
    EXPECT_EQ(static_cast<unsigned char>(result[1]), 0xA4);
}
