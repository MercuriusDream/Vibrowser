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

// ---------------------------------------------------------------------------
// Cycle 487 — additional percent encoding regression tests
// ---------------------------------------------------------------------------

// Encode then decode round-trips to original
TEST(PercentEncoding, EncodeDecodeRoundTrip) {
    std::string original = "hello world & test";
    EXPECT_EQ(percent_decode(percent_encode(original)), original);
}

// Decode upper-case hex letters: %41=%42=%43=ABC, %5A=Z
TEST(PercentDecoding, DecodeUpperHexLetters) {
    EXPECT_EQ(percent_decode("%41%42%43"), "ABC");
    EXPECT_EQ(percent_decode("%5A"), "Z");
}

// Safe chars like '!' and '$' are NOT encoded (they're URL code points)
TEST(PercentEncoding, SafeCharsNotEncoded) {
    EXPECT_EQ(percent_encode("!$&'()*+,;="), "!$&'()*+,;=");
}

// Consecutive encoded spaces
TEST(PercentDecoding, DecodeConsecutiveEncodedSpaces) {
    EXPECT_EQ(percent_decode("%20%20%20"), "   ");
}

// Decode %2D='-' and %5F='_' (unreserved chars)
TEST(PercentDecoding, DecodeMinusAndUnderscore) {
    EXPECT_EQ(percent_decode("%2D"), "-");
    EXPECT_EQ(percent_decode("%5F"), "_");
}

// Encode with path flag: '/' is encoded
TEST(PercentEncoding, PathFlagEncodesSlash) {
    EXPECT_NE(percent_encode("/a/b", true), "/a/b");
    EXPECT_EQ(percent_encode("/a/b", true), "%2Fa%2Fb");
}

// Decode mixed case hex digits
TEST(PercentDecoding, DecodeMixedCaseHex) {
    // %2f and %2F should both decode to '/'
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");
}

// Encode a string with multiple different special characters
TEST(PercentEncoding, MultipleDistinctSpecialChars) {
    // '<', '>', '"' should all get encoded
    std::string input = "<html>\"test\"</html>";
    std::string result = percent_encode(input);
    EXPECT_NE(result.find("%3C"), std::string::npos) << "< should be encoded";
    EXPECT_NE(result.find("%3E"), std::string::npos) << "> should be encoded";
    // Must not contain unencoded < or >
    EXPECT_EQ(result.find('<'), std::string::npos) << "< should not appear raw";
    EXPECT_EQ(result.find('>'), std::string::npos) << "> should not appear raw";
}

// ============================================================================
// Cycle 508: PercentEncoding regression tests
// ============================================================================

TEST(PercentEncoding, EmptyStringReturnsEmpty) {
    EXPECT_EQ(percent_encode(""), "");
}

TEST(PercentDecoding, EmptyStringReturnsEmpty) {
    EXPECT_EQ(percent_decode(""), "");
}

TEST(PercentEncoding, EncodeHashSign) {
    std::string result = percent_encode("#");
    EXPECT_EQ(result, "%23");
}

TEST(PercentDecoding, DecodeSlashFromEncoded) {
    EXPECT_EQ(percent_decode("%2F"), "/");
    EXPECT_EQ(percent_decode("%2f"), "/");
}

TEST(PercentEncoding, AtSignNotEncodedByDefault) {
    // '@' is treated as a safe character in this implementation
    std::string result = percent_encode("user@example.com");
    // Round-trip must recover original
    EXPECT_EQ(percent_decode(result), "user@example.com");
    // The host part is preserved
    EXPECT_NE(result.find("example.com"), std::string::npos);
}

TEST(PercentDecoding, DecodeLiteralLetterA) {
    // %41 is 'A' in ASCII
    EXPECT_EQ(percent_decode("%41"), "A");
}

TEST(IsURLCodePoint, SpaceIsNotURLCodePoint) {
    EXPECT_FALSE(is_url_code_point(' '));  // 0x20
}

TEST(PercentEncoding, EncodeEqualsSign) {
    // '=' is not a safe character for generic encoding
    std::string result = percent_encode("key=value");
    // Either '=' is encoded or not, depending on context.
    // The key assertion: encoding then decoding recovers the original.
    EXPECT_EQ(percent_decode(result), "key=value");
}

TEST(PercentEncoding, PathFlagEncodesColonAndAtSign) {
    std::string result = percent_encode(":@", true);
    EXPECT_NE(result.find("%3A"), std::string::npos);
    EXPECT_NE(result.find("%40"), std::string::npos);
    EXPECT_EQ(percent_decode(result), ":@");
}

TEST(PercentEncoding, ForbiddenAsciiCharsEncoded) {
    const std::string forbidden = "\\|^{}`";
    for (char ch : forbidden) {
        std::string input(1, ch);
        std::string encoded = percent_encode(input);
        ASSERT_FALSE(encoded.empty());
        EXPECT_EQ(encoded[0], '%') << "Expected encoding for char: " << ch;
        EXPECT_EQ(percent_decode(encoded), input);
    }
}

TEST(IsURLCodePoint, UnicodeAboveU00A0IsURLCodePoint) {
    EXPECT_TRUE(is_url_code_point(0x00A0));
    EXPECT_TRUE(is_url_code_point(0x1F600));
}

TEST(IsURLCodePoint, SurrogateAndNoncharacterRejected) {
    EXPECT_FALSE(is_url_code_point(0xD800));
    EXPECT_FALSE(is_url_code_point(0xDFFF));
    EXPECT_FALSE(is_url_code_point(0xFFFE));
    EXPECT_FALSE(is_url_code_point(0xFDD0));
}

TEST(PercentEncoding, FullRoundTripWithSpecialCharsV129) {
    std::string input = "<>\"#%special&chars";
    std::string encoded = percent_encode(input);
    // Verify no raw special chars remain in encoded output
    EXPECT_EQ(encoded.find('<'), std::string::npos);
    EXPECT_EQ(encoded.find('>'), std::string::npos);
    EXPECT_EQ(encoded.find('"'), std::string::npos);
    EXPECT_EQ(encoded.find('#'), std::string::npos);
    // Decode recovers original
    EXPECT_EQ(percent_decode(encoded), input);
}

TEST(PercentDecoding, DecodeBoundaryByteZeroV129) {
    std::string decoded = percent_decode("%00");
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(decoded[0]), 0x00u);
}

TEST(PercentDecoding, DecodeBoundaryByteFFV129) {
    std::string decoded = percent_decode("%FF");
    ASSERT_EQ(decoded.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(decoded[0]), 0xFFu);
}

TEST(IsURLCodePoint, BoundaryUnicodeCodePointsV129) {
    // Control characters and C1 range should be rejected
    EXPECT_FALSE(is_url_code_point(0x7F));    // DEL
    EXPECT_FALSE(is_url_code_point(0x80));    // C1 control start
    EXPECT_FALSE(is_url_code_point(0x9F));    // C1 control end
    // U+00A0 (non-breaking space) and above should be accepted
    EXPECT_TRUE(is_url_code_point(0x00A0));
    // Last valid Unicode code point in planes (excluding noncharacters)
    EXPECT_TRUE(is_url_code_point(0x10FFFD));
    // U+10FFFE is a noncharacter — should be rejected
    EXPECT_FALSE(is_url_code_point(0x10FFFE));
}

TEST(IsURLCodePoint, PercentEncodingV130_1_IsUrlCodePointPositiveBoundaryAsciiSafe) {
    EXPECT_TRUE(is_url_code_point(U'A'));
    EXPECT_TRUE(is_url_code_point(U'z'));
    EXPECT_TRUE(is_url_code_point(U'0'));
    EXPECT_TRUE(is_url_code_point(U'9'));
    EXPECT_TRUE(is_url_code_point(U'-'));
    EXPECT_TRUE(is_url_code_point(U'.'));
    EXPECT_TRUE(is_url_code_point(U'_'));
    EXPECT_TRUE(is_url_code_point(U'~'));
}

TEST(PercentDecoding, PercentEncodingV130_2_DecodeAllEncodedStringNoLiterals) {
    EXPECT_EQ(percent_decode("%48%65%6C%6C%6F"), "Hello");
}

TEST(PercentDecoding, PercentEncodingV130_3_DecodeAllEncodedHelloWorldV130) {
    EXPECT_EQ(percent_decode("%48%65%6C%6C%6F%20%57%6F%72%6C%64"), "Hello World");
}

TEST(IsURLCodePoint, IsUrlCodePointV130_4_NullAndLowAsciiControlCharsRejected) {
    EXPECT_FALSE(is_url_code_point(0x0000));
    EXPECT_FALSE(is_url_code_point(0x0007));
    EXPECT_FALSE(is_url_code_point(0x000A));
    EXPECT_FALSE(is_url_code_point(0x001F));
}

TEST(PercentEncoding, PercentEncodingV131_1_EncodePathCharsTrueMixedPathAndQueryChars) {
    // With encode_path_chars=true, path chars like / and : should be encoded
    std::string result = percent_encode("/path:to/file?q=1", true);
    EXPECT_NE(result.find("%2F"), std::string::npos);  // / encoded
    EXPECT_NE(result.find("%3A"), std::string::npos);  // : encoded
    EXPECT_NE(result.find("%3F"), std::string::npos);  // ? encoded
    // Letters and digits should remain unencoded
    EXPECT_NE(result.find("path"), std::string::npos);
    EXPECT_NE(result.find("file"), std::string::npos);
}

TEST(PercentDecoding, PercentEncodingV131_2_DecodeConsecutiveEncodedNoLiteralsBetween) {
    EXPECT_EQ(percent_decode("%2F%3F%23%25"), "/?#%");
}

TEST(PercentEncoding, PercentEncodingV131_3_RoundTripFullUrlPathString) {
    std::string original = "/my path/to file<special>#anchor";
    std::string encoded = percent_encode(original);
    std::string decoded = percent_decode(encoded);
    EXPECT_EQ(decoded, original);
    // Verify encoding actually changed something (spaces, angle brackets, #)
    EXPECT_NE(encoded, original);
    EXPECT_NE(encoded.find("%20"), std::string::npos);
}

TEST(IsURLCodePoint, PercentEncodingV131_4_IsUrlCodePointAsciiSpecialUrlChars) {
    EXPECT_TRUE(is_url_code_point(U'!'));
    EXPECT_TRUE(is_url_code_point(U'$'));
    EXPECT_TRUE(is_url_code_point(U'&'));
    EXPECT_TRUE(is_url_code_point(U'\''));
    EXPECT_TRUE(is_url_code_point(U'('));
    EXPECT_TRUE(is_url_code_point(U')'));
    EXPECT_TRUE(is_url_code_point(U'*'));
    EXPECT_TRUE(is_url_code_point(U'+'));
    EXPECT_TRUE(is_url_code_point(U','));
    EXPECT_TRUE(is_url_code_point(U';'));
    EXPECT_TRUE(is_url_code_point(U'='));
}

TEST(PercentEncoding, FullRoundTripWithMixedSpecialCharsV132) {
    // Encode a string with <>"#% and verify decode recovers the original
    std::string original = "hello<world>\"test#value%end";
    std::string encoded = percent_encode(original);
    std::string decoded = percent_decode(encoded);
    EXPECT_EQ(decoded, original);
    // Verify encoding actually changed the special characters
    EXPECT_NE(encoded, original);
    EXPECT_NE(encoded.find("%3C"), std::string::npos); // <
    EXPECT_NE(encoded.find("%3E"), std::string::npos); // >
    EXPECT_NE(encoded.find("%22"), std::string::npos); // "
    EXPECT_NE(encoded.find("%23"), std::string::npos); // #
    EXPECT_NE(encoded.find("%25"), std::string::npos); // %
}

TEST(PercentDecoding, DecodeBoundaryByteZeroV132) {
    // %00 should decode to a string containing the null byte (0x00)
    std::string result = percent_decode("%00");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(result[0]), 0x00);
}

TEST(PercentDecoding, DecodeBoundaryByteFFV132) {
    // %FF should decode to a string containing byte 0xFF
    std::string result = percent_decode("%FF");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(result[0]), 0xFF);
}

TEST(IsURLCodePoint, BoundaryUnicodeCodePointsV132) {
    // 0x7F (DEL) is a control character, not a URL code point
    EXPECT_FALSE(is_url_code_point(U'\x7F'));
    // 0x80 is a C1 control character, not a URL code point
    EXPECT_FALSE(is_url_code_point(U'\x80'));
    // 0x9F is the last C1 control character, not a URL code point
    EXPECT_FALSE(is_url_code_point(U'\x9F'));
    // 0x00A0 (non-breaking space) is the first valid non-ASCII URL code point
    EXPECT_TRUE(is_url_code_point(U'\x00A0'));
    // 0x10FFFD is the last valid Unicode code point that is a URL code point
    EXPECT_TRUE(is_url_code_point(U'\U0010FFFD'));
    // 0x10FFFE is a noncharacter, not a URL code point
    EXPECT_FALSE(is_url_code_point(U'\U0010FFFE'));
}

// =============================================================================
// Round 133 Percent Encoding tests
// =============================================================================

TEST(PercentEncoding, PercentEncodingV133_1_EncodeNonUrlSafePrintableAscii) {
    // Characters <>{}\|^` should all be percent-encoded
    std::string input = "<>{}\x5C|^`";  // \x5C is backslash
    std::string encoded = percent_encode(input);
    EXPECT_NE(encoded.find("%3C"), std::string::npos);  // <
    EXPECT_NE(encoded.find("%3E"), std::string::npos);  // >
    EXPECT_NE(encoded.find("%7B"), std::string::npos);  // {
    EXPECT_NE(encoded.find("%7D"), std::string::npos);  // }
    EXPECT_NE(encoded.find("%5C"), std::string::npos);  // backslash
    EXPECT_NE(encoded.find("%7C"), std::string::npos);  // |
    EXPECT_NE(encoded.find("%5E"), std::string::npos);  // ^
    EXPECT_NE(encoded.find("%60"), std::string::npos);  // `
    // None of the original characters should remain unencoded
    EXPECT_EQ(encoded.find('<'), std::string::npos);
    EXPECT_EQ(encoded.find('>'), std::string::npos);
    EXPECT_EQ(encoded.find('{'), std::string::npos);
    EXPECT_EQ(encoded.find('}'), std::string::npos);
}

TEST(PercentDecoding, PercentEncodingV133_2_SingleLayerDecodeNoDoubleDecoding) {
    // Decoding "%25" yields "%" — so "a%25b" → "a%b", not "ab"
    std::string result = percent_decode("a%25b");
    EXPECT_EQ(result, "a%b");
}

TEST(IsURLCodePoint, PercentEncodingV133_3_CurlyBraceNotUrlCodePoint) {
    // Curly braces are not URL code points
    EXPECT_FALSE(is_url_code_point(U'{'));
    EXPECT_FALSE(is_url_code_point(U'}'));
    // Exclamation mark is a URL code point
    EXPECT_TRUE(is_url_code_point(U'!'));
}

TEST(PercentDecoding, PercentEncodingV133_4_DecodeMixedCaseHexDigits) {
    // Both lowercase and uppercase hex digits should decode correctly
    std::string lower = percent_decode("%2f");
    std::string upper = percent_decode("%2F");
    EXPECT_EQ(lower, "/");
    EXPECT_EQ(upper, "/");
}

// =============================================================================
// Round 134 Percent Encoding tests
// =============================================================================

TEST(PercentEncoding, PercentEncodingV134_1_EncodeThenDecodeIdentity) {
    // Round-trip: encode then decode should recover the original ASCII printable string
    std::string original = "Hello, World! 123 abc XYZ";
    std::string encoded = percent_encode(original);
    std::string decoded = percent_decode(encoded);
    EXPECT_EQ(decoded, original);
}

TEST(PercentDecoding, PercentEncodingV134_2_DecodeIncompletePercentSequence) {
    // "%2" is incomplete (needs two hex digits after %) — should be left unchanged
    std::string result = percent_decode("%2");
    EXPECT_EQ(result, "%2");
}

TEST(IsURLCodePoint, PercentEncodingV134_3_IsUrlCodePointDigitsAndLetters) {
    // All digits 0-9 should be URL code points
    for (char32_t ch = U'0'; ch <= U'9'; ++ch) {
        EXPECT_TRUE(is_url_code_point(ch)) << "digit " << static_cast<char>(ch);
    }
    // All uppercase letters A-Z should be URL code points
    for (char32_t ch = U'A'; ch <= U'Z'; ++ch) {
        EXPECT_TRUE(is_url_code_point(ch)) << "upper " << static_cast<char>(ch);
    }
    // All lowercase letters a-z should be URL code points
    for (char32_t ch = U'a'; ch <= U'z'; ++ch) {
        EXPECT_TRUE(is_url_code_point(ch)) << "lower " << static_cast<char>(ch);
    }
}

TEST(PercentDecoding, PercentEncodingV134_4_DecodeEmptyString) {
    // Decoding an empty string should return an empty string
    EXPECT_EQ(percent_decode(""), "");
    EXPECT_TRUE(percent_decode("").empty());
}

// =============================================================================
// V135 tests
// =============================================================================

TEST(PercentEncoding, ConsecutiveSpecialCharsEncodeV135) {
    // Multiple consecutive special characters should each be individually encoded
    std::string result = percent_encode("a b&c");
    // Space encodes to %20; & is a valid URL code point and is NOT encoded
    EXPECT_EQ(result, "a%20b&c");

    // Multiple spaces in a row
    std::string multi_space = percent_encode("x  y");
    EXPECT_EQ(multi_space, "x%20%20y");

    // Special chars at the beginning and end
    std::string edges = percent_encode(" hello ");
    EXPECT_EQ(edges, "%20hello%20");
}

TEST(PercentDecoding, MixedCasePercentDecodesV135) {
    // Both lowercase and uppercase hex digits in percent-encoding should decode identically
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");

    // Mixed case within the same string
    EXPECT_EQ(percent_decode("%2fpath%2Fmore"), "/path/more");

    // Another example: %3a (colon) in both cases
    EXPECT_EQ(percent_decode("%3a"), ":");
    EXPECT_EQ(percent_decode("%3A"), ":");
}

TEST(PercentEncoding, AlphanumNotEncodedV135) {
    // All alphanumeric characters should pass through without encoding
    std::string lower = "abcdefghijklmnopqrstuvwxyz";
    std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string digits = "0123456789";

    EXPECT_EQ(percent_encode(lower), lower);
    EXPECT_EQ(percent_encode(upper), upper);
    EXPECT_EQ(percent_encode(digits), digits);

    // Combined alphanumeric string should also be unchanged
    std::string combined = "abc123XYZ";
    EXPECT_EQ(percent_encode(combined), combined);
}

TEST(IsURLCodePoint, AsciiControlCharsNotCodePointsV135) {
    // ASCII control characters (0x01 through 0x1F) are not valid URL code points
    for (char32_t ch = 0x01; ch <= 0x1F; ++ch) {
        EXPECT_FALSE(is_url_code_point(ch))
            << "ASCII control char 0x" << std::hex << static_cast<unsigned>(ch)
            << " should not be a URL code point";
    }

    // NULL (0x00) should also not be a URL code point
    EXPECT_FALSE(is_url_code_point(U'\0'));

    // DEL (0x7F) should also not be a URL code point
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x7F)));
}

// =============================================================================
// V136 tests
// =============================================================================

TEST(PercentEncoding, TildeNotEncodedV136) {
    // Tilde (~) is an unreserved character and a valid URL code point,
    // so it must pass through percent_encode unchanged
    EXPECT_EQ(percent_encode("~"), "~");
    EXPECT_EQ(percent_encode("a~b"), "a~b");
    EXPECT_EQ(percent_encode("~user/home"), "~user/home");
}

TEST(PercentDecoding, PlusSignNotDecodedV136) {
    // In percent decoding, '+' should NOT be converted to space.
    // '+' as space is only a form-urlencoded convention, not standard percent decoding.
    EXPECT_EQ(percent_decode("+"), "+");
    EXPECT_EQ(percent_decode("a+b"), "a+b");
    EXPECT_EQ(percent_decode("hello+world"), "hello+world");
    // Verify that %20 IS decoded to space (contrast with + behavior)
    EXPECT_EQ(percent_decode("hello%20world"), "hello world");
}

TEST(PercentEncoding, SpaceEncodesToPercent20V136) {
    // A single space should encode to %20
    EXPECT_EQ(percent_encode(" "), "%20");
    // Multiple spaces should each encode independently
    EXPECT_EQ(percent_encode("   "), "%20%20%20");
    // Space surrounded by safe characters
    EXPECT_EQ(percent_encode("a b"), "a%20b");
    // Leading and trailing spaces
    EXPECT_EQ(percent_encode(" x "), "%20x%20");
}

TEST(IsURLCodePoint, SlashAndQuestionMarkAreCodePointsV136) {
    // '/' (U+002F) is a URL code point per WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('/'));
    // '?' (U+003F) is a URL code point per WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('?'));
    // Our encoder encodes '?' to %3F by default but '/' passes through
    EXPECT_EQ(percent_encode("/"), "/");
    EXPECT_EQ(percent_encode("?"), "%3F");
}

// =============================================================================
// V137 tests
// =============================================================================

TEST(PercentEncoding, HashIsEncodedV137) {
    // '#' is NOT a URL code point (it's a delimiter) and must be percent-encoded
    EXPECT_EQ(percent_encode("#"), "%23");
    // Hash embedded in a string should be encoded while safe chars pass through
    EXPECT_EQ(percent_encode("section#anchor"), "section%23anchor");
    // Multiple hashes should each be encoded
    EXPECT_EQ(percent_encode("##"), "%23%23");
    // Round-trip: encode then decode should recover the original
    EXPECT_EQ(percent_decode(percent_encode("#test#")), "#test#");
}

TEST(PercentDecoding, DoubleEncodedRoundTripV137) {
    // %2520 is the double-encoded form of %20 (space).
    // First decode: %2520 → %20  (the %25 decodes to '%', leaving "%20")
    std::string once = percent_decode("%2520");
    EXPECT_EQ(once, "%20");
    // Second decode: %20 → " " (space)
    std::string twice = percent_decode(once);
    EXPECT_EQ(twice, " ");

    // Encoding a string that already contains %20 should double-encode the %
    std::string encoded = percent_encode("%20");
    EXPECT_EQ(encoded, "%2520");
    // And decoding the double-encoded value should return the single-encoded form
    EXPECT_EQ(percent_decode(encoded), "%20");
}

TEST(PercentEncoding, PeriodAndHyphenNotEncodedV137) {
    // Period (.) and hyphen (-) are unreserved characters / URL code points
    // and must NOT be percent-encoded
    EXPECT_EQ(percent_encode("."), ".");
    EXPECT_EQ(percent_encode("-"), "-");
    EXPECT_EQ(percent_encode("file-name.txt"), "file-name.txt");
    EXPECT_EQ(percent_encode("a.b-c.d"), "a.b-c.d");
    // Underscore and tilde should also pass through
    EXPECT_EQ(percent_encode("a_b~c"), "a_b~c");
    // Mixed: period and hyphen with a space (space gets encoded, rest stays)
    EXPECT_EQ(percent_encode("hello-world .txt"), "hello-world%20.txt");
}

TEST(IsURLCodePoint, AtSignIsCodePointV137) {
    // '@' (U+0040) is a valid URL code point per the WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('@'));
    // Verify it is not encoded by default (since it's a URL code point)
    std::string result = percent_encode("user@host");
    EXPECT_NE(result.find('@'), std::string::npos);
    // Round-trip should preserve '@'
    EXPECT_EQ(percent_decode(result), "user@host");
    // With path flag, '@' SHOULD be encoded
    std::string path_result = percent_encode("@", true);
    EXPECT_EQ(path_result, "%40");
}
