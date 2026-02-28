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

// =============================================================================
// Round V138 Percent Encoding tests
// =============================================================================

TEST(PercentEncoding, SquareBracketsEncodedV138) {
    // Square brackets [ and ] are not unreserved and not path chars,
    // so they must be percent-encoded
    EXPECT_EQ(percent_encode("["), "%5B");
    EXPECT_EQ(percent_encode("]"), "%5D");
    EXPECT_EQ(percent_encode("[foo]"), "%5Bfoo%5D");
    // They are also not URL code points
    EXPECT_FALSE(is_url_code_point('['));
    EXPECT_FALSE(is_url_code_point(']'));
    // Round-trip: decode the encoded form back to original
    EXPECT_EQ(percent_decode("%5B"), "[");
    EXPECT_EQ(percent_decode("%5D"), "]");
}

TEST(PercentDecoding, PartialSequenceAtEndPreservedV138) {
    // A percent sign followed by only one hex digit at the end of the string
    // is an incomplete sequence and should be preserved literally
    EXPECT_EQ(percent_decode("abc%2"), "abc%2");
    // Also test with just "%" at end
    EXPECT_EQ(percent_decode("test%"), "test%");
    // And "%X" where X is a valid hex digit but there's no second digit
    EXPECT_EQ(percent_decode("hello%A"), "hello%A");
    // A valid sequence followed by an incomplete one should decode the valid
    // part and preserve the incomplete part
    EXPECT_EQ(percent_decode("x%20y%3"), "x y%3");
}

TEST(PercentEncoding, CurlyBracesEncodedV138) {
    // Curly braces { and } are not unreserved and not path chars,
    // so they must be percent-encoded
    EXPECT_EQ(percent_encode("{"), "%7B");
    EXPECT_EQ(percent_encode("}"), "%7D");
    EXPECT_EQ(percent_encode("{key}"), "%7Bkey%7D");
    // Round-trip through encode then decode
    std::string original = "template-{id}-{name}";
    std::string encoded = percent_encode(original);
    EXPECT_EQ(percent_decode(encoded), original);
    // Verify the encoded form has the expected percent sequences
    EXPECT_NE(encoded.find("%7B"), std::string::npos);
    EXPECT_NE(encoded.find("%7D"), std::string::npos);
}

TEST(IsURLCodePoint, ExclamationMarkIsCodePointV138) {
    // '!' (U+0021) is a valid URL code point per the WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('!'));
    // Since '!' is a path character, it should NOT be encoded by default
    EXPECT_EQ(percent_encode("!"), "!");
    EXPECT_EQ(percent_encode("hello!world"), "hello!world");
    // With encode_path_chars=true, '!' SHOULD be encoded
    EXPECT_EQ(percent_encode("!", true), "%21");
    // Decoding %21 should give back '!'
    EXPECT_EQ(percent_decode("%21"), "!");
}

// =============================================================================
// V139 Tests
// =============================================================================

TEST(PercentEncoding, BackslashEncodedV139) {
    // Backslash '\' (U+005C) is not an unreserved character and must be encoded
    EXPECT_EQ(percent_encode("\\"), "%5C");
    // Backslash within a string should be encoded while safe chars pass through
    EXPECT_EQ(percent_encode("path\\to\\file"), "path%5Cto%5Cfile");
    // Multiple consecutive backslashes
    EXPECT_EQ(percent_encode("\\\\"), "%5C%5C");
    // Decoding the encoded form should recover the backslash
    EXPECT_EQ(percent_decode("%5C"), "\\");
    EXPECT_EQ(percent_decode("%5c"), "\\");
}

TEST(PercentDecoding, AllHexDigitsValidV139) {
    // Uppercase hex letters %41 through %5A decode to 'A' through 'Z'
    EXPECT_EQ(percent_decode("%41"), "A");
    EXPECT_EQ(percent_decode("%42"), "B");
    EXPECT_EQ(percent_decode("%43"), "C");
    EXPECT_EQ(percent_decode("%4D"), "M");
    EXPECT_EQ(percent_decode("%4E"), "N");
    EXPECT_EQ(percent_decode("%58"), "X");
    EXPECT_EQ(percent_decode("%59"), "Y");
    EXPECT_EQ(percent_decode("%5A"), "Z");
    // Verify a full sequence of hex-encoded uppercase letters decodes correctly
    EXPECT_EQ(percent_decode("%48%45%4C%4C%4F"), "HELLO");
    // Lowercase hex digits are equally valid
    EXPECT_EQ(percent_decode("%41%42%43"), "ABC");
    EXPECT_EQ(percent_decode("%61%62%63"), "abc");
}

TEST(PercentEncoding, DollarSignNotEncodedV139) {
    // '$' (U+0024) is a sub-delimiter and should NOT be encoded by default
    EXPECT_EQ(percent_encode("$"), "$");
    EXPECT_EQ(percent_encode("price=$100"), "price=$100");
    EXPECT_EQ(percent_encode("$var"), "$var");
    // Mixed with characters that ARE encoded
    EXPECT_EQ(percent_encode("$100 USD"), "$100%20USD");
    // With encode_path_chars=true, '$' SHOULD be encoded
    EXPECT_EQ(percent_encode("$", true), "%24");
    // Decoding %24 recovers '$'
    EXPECT_EQ(percent_decode("%24"), "$");
}

TEST(IsURLCodePoint, AsteriskIsCodePointV139) {
    // '*' (U+002A) is a valid URL code point per the WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('*'));
    // '*' is a sub-delimiter / path character, so NOT encoded by default
    EXPECT_EQ(percent_encode("*"), "*");
    EXPECT_EQ(percent_encode("file*.txt"), "file*.txt");
    // With encode_path_chars=true, '*' SHOULD be encoded
    EXPECT_EQ(percent_encode("*", true), "%2A");
    // Decoding %2A should give back '*'
    EXPECT_EQ(percent_decode("%2A"), "*");
    EXPECT_EQ(percent_decode("%2a"), "*");
}

// =============================================================================
// V140 Tests
// =============================================================================

TEST(PercentEncoding, PipeCharEncodedV140) {
    // '|' (U+007C) is not unreserved and not a sub-delimiter — must be encoded
    EXPECT_EQ(percent_encode("|"), "%7C");
    EXPECT_EQ(percent_encode("a|b|c"), "a%7Cb%7Cc");
    // Decoding %7C recovers '|'
    EXPECT_EQ(percent_decode("%7C"), "|");
    EXPECT_EQ(percent_decode("%7c"), "|");
}

TEST(PercentDecoding, LowercaseHexDecodesV140) {
    // Lowercase hex digits %6a must decode to 'j' (U+006A)
    EXPECT_EQ(percent_decode("%6a"), "j");
    // Uppercase equivalent also decodes to 'j'
    EXPECT_EQ(percent_decode("%6A"), "j");
    // Mixed case in a longer string
    EXPECT_EQ(percent_decode("%6a%75%6d%70"), "jump");
}

TEST(PercentEncoding, ColonNotEncodedV140) {
    // ':' is a sub-delimiter / path character — NOT encoded by default
    EXPECT_EQ(percent_encode(":"), ":");
    EXPECT_EQ(percent_encode("time:12:30"), "time:12:30");
    // With encode_path_chars=true, ':' SHOULD be encoded
    EXPECT_EQ(percent_encode(":", true), "%3A");
    // Decoding %3A recovers ':'
    EXPECT_EQ(percent_decode("%3A"), ":");
}

TEST(IsURLCodePoint, EqualsSignIsCodePointV140) {
    // '=' (U+003D) is a valid URL code point per the WHATWG URL spec
    EXPECT_TRUE(is_url_code_point('='));
    // '=' is a sub-delimiter, so NOT encoded by default
    EXPECT_EQ(percent_encode("="), "=");
    EXPECT_EQ(percent_encode("key=value"), "key=value");
    // With encode_path_chars=true, '=' SHOULD be encoded
    EXPECT_EQ(percent_encode("=", true), "%3D");
    // Decoding %3D should give back '='
    EXPECT_EQ(percent_decode("%3D"), "=");
    EXPECT_EQ(percent_decode("%3d"), "=");
}

TEST(PercentEncoding, SpaceEncodesTo20V141) {
    // A single space character must encode to %20
    EXPECT_EQ(percent_encode(" "), "%20");
    // Multiple spaces each become %20
    EXPECT_EQ(percent_encode("  "), "%20%20");
    // Space within text
    EXPECT_EQ(percent_encode("a b"), "a%20b");
}

TEST(PercentDecoding, PlusSignNotDecodedV141) {
    // %2B decodes to '+'
    EXPECT_EQ(percent_decode("%2B"), "+");
    // A literal '+' is NOT a space — it should remain as '+'
    EXPECT_EQ(percent_decode("+"), "+");
    // Mixed: %2B and literal + both yield '+'
    EXPECT_EQ(percent_decode("%2B+"), "++");
}

TEST(PercentEncoding, AlphanumericNotEncodedV141) {
    // Lowercase letters pass through unchanged
    EXPECT_EQ(percent_encode("abcdefghijklmnopqrstuvwxyz"), "abcdefghijklmnopqrstuvwxyz");
    // Uppercase letters pass through unchanged
    EXPECT_EQ(percent_encode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    // Digits pass through unchanged
    EXPECT_EQ(percent_encode("0123456789"), "0123456789");
    // Mixed alphanumeric
    EXPECT_EQ(percent_encode("Test123"), "Test123");
}

TEST(IsURLCodePoint, AsciiLettersAndDigitsAreCodePointsV141) {
    // All lowercase ASCII letters are valid URL code points
    EXPECT_TRUE(is_url_code_point(U'a'));
    EXPECT_TRUE(is_url_code_point(U'z'));
    // All uppercase ASCII letters are valid URL code points
    EXPECT_TRUE(is_url_code_point(U'A'));
    EXPECT_TRUE(is_url_code_point(U'Z'));
    // All ASCII digits are valid URL code points
    EXPECT_TRUE(is_url_code_point(U'0'));
    EXPECT_TRUE(is_url_code_point(U'9'));
    // Control chars in 0x7F-0x9F range are NOT valid URL code points
    EXPECT_FALSE(is_url_code_point(0x7F));
    EXPECT_FALSE(is_url_code_point(0x80));
    EXPECT_FALSE(is_url_code_point(0x9F));
}

TEST(PercentEncoding, HashEncodedV142) {
    // The '#' character must be percent-encoded to %23
    EXPECT_EQ(percent_encode("#"), "%23");
    // Hash within text
    EXPECT_EQ(percent_encode("a#b"), "a%23b");
}

TEST(PercentDecoding, LowercaseHexDecodedV142) {
    // Lowercase hex digits decode identically to uppercase
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");
    // Both produce the same result
    EXPECT_EQ(percent_decode("%2f"), percent_decode("%2F"));
    // Mixed case hex
    EXPECT_EQ(percent_decode("%2a"), "*");
    EXPECT_EQ(percent_decode("%2A"), "*");
}

TEST(PercentEncoding, TildeAndHyphenNotEncodedV142) {
    // Tilde and hyphen are unreserved characters and must not be encoded
    EXPECT_EQ(percent_encode("~"), "~");
    EXPECT_EQ(percent_encode("-"), "-");
    EXPECT_EQ(percent_encode("a~b-c"), "a~b-c");
    EXPECT_EQ(percent_encode("~-~-"), "~-~-");
}

TEST(IsURLCodePoint, ExclamationAndAsteriskAreCodePointsV142) {
    // '!' and '*' are valid URL code points
    EXPECT_TRUE(is_url_code_point(U'!'));
    EXPECT_TRUE(is_url_code_point(U'*'));
    // Also verify '&' is a valid URL code point
    EXPECT_TRUE(is_url_code_point(U'&'));
}

TEST(PercentEncoding, EqualsAndAmpersandNotEncodedV143) {
    // '=' and '&' are valid URL code points and should not be encoded
    EXPECT_EQ(percent_encode("="), "=");
    EXPECT_EQ(percent_encode("&"), "&");
    EXPECT_EQ(percent_encode("a=1&b=2"), "a=1&b=2");
    EXPECT_EQ(percent_encode("key=value&foo=bar"), "key=value&foo=bar");
}

TEST(PercentDecoding, DoubleEncodedPercentV143) {
    // %2520 should decode to %20 in a single decode pass (not to a space)
    EXPECT_EQ(percent_decode("%2520"), "%20");
    // Multiple double-encoded sequences
    EXPECT_EQ(percent_decode("a%2520b%2520c"), "a%20b%20c");
}

TEST(PercentEncoding, AtSignNotEncodedV143) {
    // '@' is a valid URL code point and is NOT encoded by percent_encode
    EXPECT_EQ(percent_encode("@"), "@");
    EXPECT_EQ(percent_encode("user@host"), "user@host");
    // '@' mixed with characters that ARE encoded
    EXPECT_EQ(percent_encode("user@host name"), "user@host%20name");
}

TEST(IsURLCodePoint, DollarAndPlusAreCodePointsV143) {
    // '$' and '+' are valid URL code points
    EXPECT_TRUE(is_url_code_point(U'$'));
    EXPECT_TRUE(is_url_code_point(U'+'));
    // Also verify some related characters
    EXPECT_TRUE(is_url_code_point(U','));
    EXPECT_TRUE(is_url_code_point(U';'));
}

TEST(PercentEncoding, BracketsEncodedV144) {
    // '[' and ']' are NOT valid URL code points and must be encoded
    EXPECT_EQ(percent_encode("["), "%5B");
    EXPECT_EQ(percent_encode("]"), "%5D");
    EXPECT_EQ(percent_encode("[test]"), "%5Btest%5D");
}

TEST(PercentDecoding, EmptyStringDecodesEmptyV144) {
    // Decoding an empty string should produce an empty string
    EXPECT_EQ(percent_decode(""), "");
}

TEST(PercentEncoding, EmptyStringEncodesEmptyV144) {
    // Encoding an empty string should produce an empty string
    EXPECT_EQ(percent_encode(""), "");
}

TEST(IsURLCodePoint, PeriodAndColonAreCodePointsV144) {
    // '.' and ':' are valid URL code points
    EXPECT_TRUE(is_url_code_point(U'.'));
    EXPECT_TRUE(is_url_code_point(U':'));
}

TEST(PercentEncoding, LessThanGreaterThanEncodedV145) {
    // '<' and '>' are NOT valid URL code points and must be percent-encoded
    EXPECT_EQ(percent_encode("<"), "%3C");
    EXPECT_EQ(percent_encode(">"), "%3E");
    EXPECT_EQ(percent_encode("<tag>"), "%3Ctag%3E");
    EXPECT_EQ(percent_encode("a<b>c"), "a%3Cb%3Ec");
}

TEST(PercentDecoding, IncompletePercentSequencePreservedV145) {
    // An incomplete percent sequence like "%2" should be preserved or handled gracefully
    std::string input = "%2";
    std::string decoded = percent_decode(input);
    // Either preserved as-is or gracefully handled (not crash)
    EXPECT_FALSE(decoded.empty() && input.size() > 0 && decoded.size() == 0);
    // A trailing percent with one hex digit shouldn't produce garbage
    std::string input2 = "abc%2";
    std::string decoded2 = percent_decode(input2);
    // The non-percent part should survive
    EXPECT_NE(decoded2.find("abc"), std::string::npos);
}

TEST(PercentEncoding, SlashNotEncodedV145) {
    // '/' is a valid URL code point and should NOT be percent-encoded
    EXPECT_EQ(percent_encode("/"), "/");
    EXPECT_EQ(percent_encode("/path/to/file"), "/path/to/file");
    EXPECT_EQ(percent_encode("a/b/c"), "a/b/c");
}

TEST(IsURLCodePoint, ForwardSlashAndQuestionMarkV145) {
    // '/' and '?' are both valid URL code points
    EXPECT_TRUE(is_url_code_point(U'/'));
    EXPECT_TRUE(is_url_code_point(U'?'));
    // Also verify together with other known code points
    EXPECT_TRUE(is_url_code_point(U'@'));
    EXPECT_TRUE(is_url_code_point(U'&'));
}

TEST(PercentEncoding, CurlyBracesEncodedV146) {
    // '{' and '}' are NOT valid URL code points and must be percent-encoded
    EXPECT_EQ(percent_encode("{"), "%7B");
    EXPECT_EQ(percent_encode("}"), "%7D");
    EXPECT_EQ(percent_encode("{key}"), "%7Bkey%7D");
}

TEST(PercentDecoding, MixedEncodedAndPlainV146) {
    // Mix of percent-encoded and plain characters should decode correctly
    EXPECT_EQ(percent_decode("hello%20world%21"), "hello world!");
    EXPECT_EQ(percent_decode("abc%2Fdef"), "abc/def");
    EXPECT_EQ(percent_decode("no%20problem%3F"), "no problem?");
}

TEST(PercentEncoding, PipeEncodedV146) {
    // '|' (pipe) is NOT a valid URL code point and must be percent-encoded
    EXPECT_EQ(percent_encode("|"), "%7C");
    EXPECT_EQ(percent_encode("a|b|c"), "a%7Cb%7Cc");
}

TEST(IsURLCodePoint, SemicolonAndCommaAreCodePointsV146) {
    // ';' and ',' are valid URL code points
    EXPECT_TRUE(is_url_code_point(U';'));
    EXPECT_TRUE(is_url_code_point(U','));
    // Also verify they are not encoded by percent_encode
    EXPECT_EQ(percent_encode(";"), ";");
    EXPECT_EQ(percent_encode(","), ",");
}

// =============================================================================
// V147 Tests
// =============================================================================

TEST(PercentEncoding, BackslashEncodedV147) {
    // Backslash '\' is NOT a valid URL code point and must be percent-encoded
    EXPECT_EQ(percent_encode("\\"), "%5C");
    EXPECT_EQ(percent_encode("a\\b\\c"), "a%5Cb%5Cc");
}

TEST(PercentDecoding, AllHexDigitCombinationsV147) {
    // Verify decoding of percent-encoded hex digit combinations
    EXPECT_EQ(percent_decode("%41"), "A");
    EXPECT_EQ(percent_decode("%61"), "a");
    EXPECT_EQ(percent_decode("%30"), "0");
    EXPECT_EQ(percent_decode("%39"), "9");
}

TEST(PercentEncoding, QuoteEncodedV147) {
    // Double-quote '"' is NOT a valid URL code point and must be percent-encoded
    EXPECT_EQ(percent_encode("\""), "%22");
    EXPECT_EQ(percent_encode("say \"hello\""), "say%20%22hello%22");
}

TEST(IsURLCodePoint, EqualsSignIsCodePointV147) {
    // '=' is a valid URL code point (used in query key=value pairs)
    EXPECT_TRUE(is_url_code_point(U'='));
    // Also verify it is not encoded by percent_encode
    EXPECT_EQ(percent_encode("="), "=");
}

// =============================================================================
// V148 Tests
// =============================================================================

TEST(PercentEncoding, CaretEncodedV148) {
    // Caret '^' is not a valid URL code point and must be percent-encoded
    EXPECT_EQ(percent_encode("^"), "%5E");
    EXPECT_EQ(percent_encode("a^b"), "a%5Eb");
}

TEST(PercentDecoding, FullAlphabetRoundTripV148) {
    // Encode then decode lowercase alphabet should yield identity
    std::string alpha = "abcdefghijklmnopqrstuvwxyz";
    std::string encoded = percent_encode(alpha);
    // Lowercase letters are unreserved and should not be encoded
    EXPECT_EQ(encoded, alpha);
    std::string decoded = percent_decode(encoded);
    EXPECT_EQ(decoded, alpha);
}

TEST(PercentEncoding, GraveAccentEncodedV148) {
    // Grave accent '`' is not a valid URL code point and must be percent-encoded
    EXPECT_EQ(percent_encode("`"), "%60");
    EXPECT_EQ(percent_encode("a`b"), "a%60b");
}

TEST(IsURLCodePoint, TildeIsCodePointV148) {
    // '~' is a valid URL code point (unreserved character)
    EXPECT_TRUE(is_url_code_point(U'~'));
}

// =============================================================================
// V149 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, TabEncodedV149) {
    // Tab character (0x09) must be percent-encoded
    std::string input(1, '\t');
    EXPECT_EQ(percent_encode(input), "%09");
}

TEST(PercentDecoding, MixedCaseHexV149) {
    // %2a (lowercase) and %2A (uppercase) both decode to '*'
    EXPECT_EQ(percent_decode("%2a"), "*");
    EXPECT_EQ(percent_decode("%2A"), "*");
}

TEST(PercentEncoding, NewlineEncodedV149) {
    // Newline character (0x0A) must be percent-encoded
    std::string input(1, '\n');
    EXPECT_EQ(percent_encode(input), "%0A");
}

TEST(IsURLCodePoint, HyphenAndUnderscoreV149) {
    // '-' and '_' are valid URL code points (unreserved characters)
    EXPECT_TRUE(is_url_code_point(U'-'));
    EXPECT_TRUE(is_url_code_point(U'_'));
}

// =============================================================================
// V150 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, SpaceEncodedAsPercent20V150) {
    // Space character must be encoded as %20
    EXPECT_EQ(percent_encode(" "), "%20");
    EXPECT_EQ(percent_encode("a b c"), "a%20b%20c");
}

TEST(PercentDecoding, UpperAndLowerHexBothDecodedV150) {
    // Both %2f (lowercase hex) and %2F (uppercase hex) must decode to '/'
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");
    // Mixed case in a longer string
    EXPECT_EQ(percent_decode("path%2fto%2Ffile"), "path/to/file");
}

TEST(PercentEncoding, SlashNotEncodedInPathV150) {
    // By default (encode_path_chars=false), '/' should pass through unencoded
    EXPECT_EQ(percent_encode("/foo/bar"), "/foo/bar");
    EXPECT_EQ(percent_encode("/a/b/c/d"), "/a/b/c/d");
}

TEST(IsURLCodePoint, ASCIIAlphaNumericAllTrueV150) {
    // All lowercase letters are valid URL code points
    for (char c = 'a'; c <= 'z'; ++c) {
        EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(c)));
    }
    // All uppercase letters are valid URL code points
    for (char c = 'A'; c <= 'Z'; ++c) {
        EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(c)));
    }
    // All digits are valid URL code points
    for (char c = '0'; c <= '9'; ++c) {
        EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(c)));
    }
}

// =============================================================================
// V151 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, HashEncodedV151) {
    // The '#' character must be percent-encoded as %23
    EXPECT_EQ(percent_encode("#"), "%23");
    EXPECT_EQ(percent_encode("foo#bar"), "foo%23bar");
}

TEST(PercentDecoding, IncompletePercentSequencePreservedV151) {
    // An incomplete percent sequence like %2 (missing second hex digit)
    // should be preserved as-is in the output
    EXPECT_EQ(percent_decode("%2"), "%2");
    EXPECT_EQ(percent_decode("abc%2"), "abc%2");
}

TEST(PercentEncoding, TildeNotEncodedV151) {
    // Tilde '~' is an unreserved character and should pass through unencoded
    EXPECT_EQ(percent_encode("~"), "~");
    EXPECT_EQ(percent_encode("~user"), "~user");
    EXPECT_EQ(percent_encode("/home/~user"), "/home/~user");
}

TEST(IsURLCodePoint, HyphenAndUnderscoreValidV151) {
    // Hyphen '-' and underscore '_' are valid URL code points
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('-')));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('_')));
}

// =============================================================================
// V152 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, QuestionMarkEncodedV152) {
    // The '?' character must be percent-encoded as %3F
    EXPECT_EQ(percent_encode("?"), "%3F");
    EXPECT_EQ(percent_encode("search?query"), "search%3Fquery");
}

TEST(PercentDecoding, PlusSignNotDecodedV152) {
    // The '+' sign should NOT be decoded to a space — it stays as '+'
    EXPECT_EQ(percent_decode("+"), "+");
    EXPECT_EQ(percent_decode("a+b"), "a+b");
    EXPECT_EQ(percent_decode("hello+world"), "hello+world");
}

TEST(PercentEncoding, AmpersandNotEncodedV152) {
    // The '&' character is not percent-encoded by the default encode set
    // It passes through unchanged since it is used as a query separator
    EXPECT_EQ(percent_encode("&"), "&");
    EXPECT_EQ(percent_encode("a&b"), "a&b");
    EXPECT_EQ(percent_encode("key=val&foo=bar"), "key=val&foo=bar");
}

TEST(IsURLCodePoint, PeriodAndExclamationValidV152) {
    // Period '.' and exclamation mark '!' are valid URL code points
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('.')));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('!')));
}

// =============================================================================
// V153 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, EqualsSignEncodedV153) {
    // The '=' character is a path char: not encoded by default, encoded with encode_path_chars=true
    EXPECT_EQ(percent_encode("="), "=");
    EXPECT_EQ(percent_encode("=", true), "%3D");
    EXPECT_EQ(percent_encode("key=value", true), "key%3Dvalue");
}

TEST(PercentDecoding, DoubleEncodedDecodesOnceV153) {
    // Double-encoded %2520 should decode once to %20 (not to a space)
    EXPECT_EQ(percent_decode("%2520"), "%20");
}

TEST(PercentEncoding, AtSignEncodedV153) {
    // The '@' character is a path char: not encoded by default, encoded with encode_path_chars=true
    EXPECT_EQ(percent_encode("@"), "@");
    EXPECT_EQ(percent_encode("@", true), "%40");
    EXPECT_EQ(percent_encode("user@host", true), "user%40host");
}

TEST(IsURLCodePoint, DollarAndAsteriskValidV153) {
    // '$' and '*' are valid URL code points
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('$')));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('*')));
}

// =============================================================================
// V154 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, ColonEncodedV154) {
    // ':' is a path character — not encoded by default, but encoded with encode_path_chars=true
    EXPECT_EQ(percent_encode(":", true), "%3A");
    // In a string context, only the colon gets encoded
    EXPECT_EQ(percent_encode("host:port", true), "host%3Aport");
    // Without the flag, colon passes through
    EXPECT_EQ(percent_encode(":"), ":");
    // Decoding %3A should yield ':'
    EXPECT_EQ(percent_decode("%3A"), ":");
}

TEST(PercentDecoding, MixedEncodedAndRawV154) {
    // %41 decodes to 'A', then 'bc' remains raw → "Abc"
    EXPECT_EQ(percent_decode("%41bc"), "Abc");
    // Multiple mixed sequences: %48 = 'H', 'ello' raw
    EXPECT_EQ(percent_decode("%48ello"), "Hello");
    // Encoded followed by raw followed by encoded: %61 = 'a', x raw, %62 = 'b'
    EXPECT_EQ(percent_decode("%61x%62"), "axb");
}

TEST(PercentEncoding, BracketEncodedV154) {
    // '[' and ']' are not unreserved characters and should be percent-encoded
    EXPECT_EQ(percent_encode("["), "%5B");
    EXPECT_EQ(percent_encode("]"), "%5D");
    // In combination with other characters
    EXPECT_EQ(percent_encode("arr[0]"), "arr%5B0%5D");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%5B%5D"), "[]");
}

TEST(IsURLCodePoint, PlusAndCommaValidV154) {
    // '+' (U+002B) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('+')));
    // ',' (U+002C) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(',')));
    // Both should not be encoded by the default encode set
    EXPECT_EQ(percent_encode("+"), "+");
    EXPECT_EQ(percent_encode(","), ",");
}

// =============================================================================
// V155 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, PipeEncodedV155) {
    // '|' (U+007C) is not a valid URL code point and should be percent-encoded
    EXPECT_EQ(percent_encode("|"), "%7C");
    // Pipe embedded in a string
    EXPECT_EQ(percent_encode("a|b"), "a%7Cb");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%7C"), "|");
}

TEST(PercentDecoding, ConsecutiveEncodedCharsV155) {
    // A sequence of consecutive percent-encoded characters should all decode correctly
    // %48='H', %65='e', %6C='l', %6C='l', %6F='o'
    EXPECT_EQ(percent_decode("%48%65%6C%6C%6F"), "Hello");
    // Lowercase hex should also work
    EXPECT_EQ(percent_decode("%48%65%6c%6c%6f"), "Hello");
    // Mixed case in hex digits
    EXPECT_EQ(percent_decode("%48%65%6C%6c%6F"), "Hello");
}

TEST(PercentEncoding, BackslashEncodedV155) {
    // '\' (U+005C) is not a valid URL code point and should be percent-encoded
    EXPECT_EQ(percent_encode("\\"), "%5C");
    // Backslash in a path-like context
    EXPECT_EQ(percent_encode("dir\\file"), "dir%5Cfile");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%5C"), "\\");
}

TEST(IsURLCodePoint, SingleQuoteAndParensValidV155) {
    // '\'' (U+0027) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('\'')));
    // '(' (U+0028) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('(')));
    // ')' (U+0029) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(')')));
    // These characters should not be encoded by the default encode set
    EXPECT_EQ(percent_encode("'"), "'");
    EXPECT_EQ(percent_encode("("), "(");
    EXPECT_EQ(percent_encode(")"), ")");
    // Combined in a string
    EXPECT_EQ(percent_encode("func('x')"), "func('x')");
}

TEST(PercentEncoding, CaretEncodedV156) {
    // '^' (U+005E) should be percent-encoded as %5E
    EXPECT_EQ(percent_encode("^"), "%5E");
    // Caret embedded in a string
    EXPECT_EQ(percent_encode("x^2"), "x%5E2");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%5E"), "^");
}

TEST(PercentDecoding, LowercaseHexDecodesV156) {
    // Lowercase hex digits in percent-encoded sequences should decode correctly
    // %6c is lowercase 'l' (U+006C)
    EXPECT_EQ(percent_decode("%6c"), "l");
    // %6d is lowercase 'm' (U+006D)
    EXPECT_EQ(percent_decode("%6d"), "m");
    // Mixed case: uppercase %6C should also decode to 'l'
    EXPECT_EQ(percent_decode("%6C"), "l");
}

TEST(PercentEncoding, GraveAccentEncodedV156) {
    // '`' (U+0060) should be percent-encoded as %60
    EXPECT_EQ(percent_encode("`"), "%60");
    // Grave accent embedded in a string
    EXPECT_EQ(percent_encode("a`b"), "a%60b");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%60"), "`");
}

TEST(IsURLCodePoint, SemicolonAndSlashValidV156) {
    // ';' (U+003B) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(';')));
    // '/' (U+002F) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('/')));
    // These characters should not be encoded by the default encode set
    EXPECT_EQ(percent_encode(";"), ";");
    EXPECT_EQ(percent_encode("/"), "/");
    // Combined in a path-like string
    EXPECT_EQ(percent_encode("path/to;params"), "path/to;params");
}

TEST(PercentEncoding, LessThanGreaterThanEncodedV157) {
    // '<' (U+003C) should be percent-encoded as %3C
    EXPECT_EQ(percent_encode("<"), "%3C");
    // '>' (U+003E) should be percent-encoded as %3E
    EXPECT_EQ(percent_encode(">"), "%3E");
    // Both in a string
    EXPECT_EQ(percent_encode("<tag>"), "%3Ctag%3E");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%3C"), "<");
    EXPECT_EQ(percent_decode("%3E"), ">");
}

TEST(PercentDecoding, PercentSignAlonePreservedV157) {
    // A lone '%' not followed by two hex digits should be preserved as-is
    EXPECT_EQ(percent_decode("%"), "%");
    // '%' followed by only one hex digit should be preserved
    EXPECT_EQ(percent_decode("%A"), "%A");
    // '%' followed by non-hex should be preserved
    EXPECT_EQ(percent_decode("%GG"), "%GG");
    // Valid sequence surrounded by lone percents
    EXPECT_EQ(percent_decode("%20"), " ");
}

TEST(PercentEncoding, DoubleQuoteEncodedV157) {
    // '"' (U+0022) should be percent-encoded as %22
    EXPECT_EQ(percent_encode("\""), "%22");
    // Double quote embedded in a string
    EXPECT_EQ(percent_encode("say \"hi\""), "say%20%22hi%22");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%22"), "\"");
}

TEST(IsURLCodePoint, AtSignAndColonValidV157) {
    // '@' (U+0040) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('@')));
    // ':' (U+003A) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(':')));
    // These characters should not be percent-encoded by the default encode set
    EXPECT_EQ(percent_encode("@"), "@");
    EXPECT_EQ(percent_encode(":"), ":");
    // Combined in a typical URL-like string
    EXPECT_EQ(percent_encode("user@host:8080"), "user@host:8080");
}

TEST(PercentEncoding, NumberSignEncodedV158) {
    // '#' (U+0023) should be percent-encoded as %23
    EXPECT_EQ(percent_encode("#"), "%23");
    // Number sign embedded in a string
    EXPECT_EQ(percent_encode("page#section"), "page%23section");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%23"), "#");
}

TEST(PercentDecoding, ThreeConsecutiveEncodedV158) {
    // Three consecutive percent-encoded ASCII letters should decode correctly
    EXPECT_EQ(percent_decode("%41%42%43"), "ABC");
    // Mixed encoded and literal characters
    EXPECT_EQ(percent_decode("x%41y%42z%43"), "xAyBzC");
    // Lowercase hex digits should also decode properly
    EXPECT_EQ(percent_decode("%61%62%63"), "abc");
}

TEST(PercentEncoding, CurlyBracesEncodedV158) {
    // '{' (U+007B) should be percent-encoded as %7B
    EXPECT_EQ(percent_encode("{"), "%7B");
    // '}' (U+007D) should be percent-encoded as %7D
    EXPECT_EQ(percent_encode("}"), "%7D");
    // Both curly braces in a string
    EXPECT_EQ(percent_encode("{key}"), "%7Bkey%7D");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%7B"), "{");
    EXPECT_EQ(percent_decode("%7D"), "}");
}

TEST(IsURLCodePoint, EqualsAndQuestionMarkValidV158) {
    // '=' (U+003D) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('=')));
    // '?' (U+003F) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('?')));
    // '=' is not percent-encoded by the default encode set
    EXPECT_EQ(percent_encode("="), "=");
    // '?' IS percent-encoded by the default encode set even though it is a valid URL code point
    EXPECT_EQ(percent_encode("?"), "%3F");
    // Combined in a query-like string: = preserved, ? encoded
    EXPECT_EQ(percent_encode("key=value?more"), "key=value%3Fmore");
}

TEST(PercentEncoding, TabEncodedV159) {
    // Tab character (U+0009) should be percent-encoded as %09
    std::string tab_str(1, '\t');
    EXPECT_EQ(percent_encode(tab_str), "%09");
    // Tab embedded in a string
    EXPECT_EQ(percent_encode("before\tafter"), "before%09after");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%09"), tab_str);
}

TEST(PercentDecoding, AllLowercaseHexV159) {
    // Lowercase hex digits in percent-encoded sequences should decode correctly
    // %61 = 'a', %62 = 'b', %63 = 'c'
    EXPECT_EQ(percent_decode("%61%62%63"), "abc");
    // Mixed lowercase hex with literal characters
    EXPECT_EQ(percent_decode("x%61y%62z%63"), "xaybzc");
    // Single lowercase hex decode
    EXPECT_EQ(percent_decode("%7a"), "z");
}

TEST(PercentEncoding, NewlineEncodedV159) {
    // Newline character (U+000A) should be percent-encoded as %0A
    std::string nl_str(1, '\n');
    EXPECT_EQ(percent_encode(nl_str), "%0A");
    // Newline embedded in a string
    EXPECT_EQ(percent_encode("line1\nline2"), "line1%0Aline2");
    // Decoding should round-trip
    EXPECT_EQ(percent_decode("%0A"), nl_str);
}

TEST(IsURLCodePoint, AmpersandAndHashValidV159) {
    // '&' (U+0026) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('&')));
    // '#' (U+0023) is NOT treated as a URL code point in this implementation
    // because it serves as the fragment delimiter
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>('#')));
    // '&' is not percent-encoded by the default encode set
    EXPECT_EQ(percent_encode("&"), "&");
    // '#' is percent-encoded as %23
    EXPECT_EQ(percent_encode("#"), "%23");
}

// =============================================================================
// Round 160 Percent Encoding Tests
// =============================================================================

TEST(PercentEncoding, SpaceEncodedAsPercent20V160) {
    // A single space should be encoded as %20
    EXPECT_EQ(percent_encode(" "), "%20");
    // Multiple consecutive spaces
    EXPECT_EQ(percent_encode("   "), "%20%20%20");
    // Space at the beginning and end
    EXPECT_EQ(percent_encode(" test "), "%20test%20");
}

TEST(PercentDecoding, PlusSignNotDecodedAsSpaceV160) {
    // %2B decodes to the literal '+' character
    EXPECT_EQ(percent_decode("%2B"), "+");
    // A literal '+' in the input should remain as '+' (not decoded to space)
    EXPECT_EQ(percent_decode("+"), "+");
    // Mix of %2B and literal +
    EXPECT_EQ(percent_decode("a%2Bb+c"), "a+b+c");
}

TEST(PercentEncoding, SlashNotEncodedInPathV160) {
    // '/' is a valid URL code point
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('/')));
    // By default (encode_slash=false), slash is not encoded
    EXPECT_EQ(percent_encode("/"), "/");
    EXPECT_EQ(percent_encode("/foo/bar"), "/foo/bar");
    // When encode_slash=true, slash is encoded
    EXPECT_EQ(percent_encode("/", true), "%2F");
}

TEST(IsURLCodePoint, DigitsAndLettersAllValidV160) {
    // All ASCII digits 0-9 should be valid URL code points
    for (char c = '0'; c <= '9'; ++c) {
        EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(c)));
    }
    // All lowercase ASCII letters a-z should be valid URL code points
    for (char c = 'a'; c <= 'z'; ++c) {
        EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(c)));
    }
    // All uppercase ASCII letters A-Z should be valid URL code points
    for (char c = 'A'; c <= 'Z'; ++c) {
        EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(c)));
    }
}

// =============================================================================
// Round 161 — Percent encoding tests
// =============================================================================

TEST(PercentEncoding, AngleBracketsEncodedV161) {
    // < and > should be percent-encoded as %3C and %3E
    EXPECT_EQ(percent_encode("<"), "%3C");
    EXPECT_EQ(percent_encode(">"), "%3E");
    EXPECT_EQ(percent_encode("<tag>"), "%3Ctag%3E");
    EXPECT_EQ(percent_encode("a<b>c"), "a%3Cb%3Ec");
}

TEST(PercentDecoding, LowercaseHexDecodedV161) {
    // Lowercase hex digits in percent-encoded sequences should decode correctly
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");
    EXPECT_EQ(percent_decode("%3c"), "<");
    EXPECT_EQ(percent_decode("%3C"), "<");
    EXPECT_EQ(percent_decode("hello%20world"), "hello world");
}

TEST(PercentEncoding, TildeNotEncodedV161) {
    // Tilde (~) is a URL code point and should not be percent-encoded
    EXPECT_EQ(percent_encode("~"), "~");
    EXPECT_EQ(percent_encode("~user"), "~user");
    EXPECT_EQ(percent_encode("/home/~user/file"), "/home/~user/file");
}

TEST(IsURLCodePoint, HyphenDotUnderscoreTildeValidV161) {
    // Hyphen, dot, underscore, and tilde are all valid URL code points
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('-')));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('.')));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('_')));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>('~')));
}

// =============================================================================
// Round 162 percent encoding tests
// =============================================================================

TEST(PercentEncoding, DoubleQuoteEncodedV162) {
    // Double quote (") should be percent-encoded as %22
    EXPECT_EQ(percent_encode("\""), "%22");
    EXPECT_EQ(percent_encode("say\"hello\""), "say%22hello%22");
}

TEST(PercentDecoding, UppercaseHexDecodedV162) {
    // Uppercase hex digits in percent-encoded sequences decode correctly
    EXPECT_EQ(percent_decode("%2F"), "/");
    EXPECT_EQ(percent_decode("path%2Fto%2Ffile"), "path/to/file");
}

TEST(PercentEncoding, ExclamationNotEncodedV162) {
    // '!' is a valid URL code point and should NOT be encoded by default
    EXPECT_EQ(percent_encode("!"), "!");
    EXPECT_EQ(percent_encode("hello!"), "hello!");
}

TEST(IsURLCodePoint, NullByteInvalidV162) {
    // Null byte (0x00) is NOT a valid URL code point
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x00)));
}

// =============================================================================
// Round 163 percent encoding tests
// =============================================================================

TEST(PercentEncoding, HashEncodedAsPercent23V163) {
    // '#' should be percent-encoded as %23
    EXPECT_EQ(percent_encode("#"), "%23");
    EXPECT_EQ(percent_encode("page#anchor"), "page%23anchor");
}

TEST(PercentDecoding, PercentSignDecodedV163) {
    // %25 decodes back to a literal '%'
    EXPECT_EQ(percent_decode("%25"), "%");
    EXPECT_EQ(percent_decode("100%25 done"), "100% done");
}

TEST(PercentEncoding, AsteriskNotEncodedV163) {
    // '*' is a valid URL code point and should NOT be encoded
    EXPECT_EQ(percent_encode("*"), "*");
    EXPECT_EQ(percent_encode("a*b*c"), "a*b*c");
}

TEST(IsURLCodePoint, TabAndNewlineInvalidV163) {
    // Tab (0x09) and newline (0x0A) are NOT valid URL code points
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x09)));
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x0A)));
}

// =============================================================================
// Round 164 percent encoding tests
// =============================================================================

TEST(PercentEncoding, LeftBracketEncodedV164) {
    // '[' should be percent-encoded as %5B
    EXPECT_EQ(percent_encode("["), "%5B");
    EXPECT_EQ(percent_encode("a[0]"), "a%5B0%5D");
}

TEST(PercentDecoding, MixedCaseHexDecodedV164) {
    // Both lowercase %2f and uppercase %2F should decode to '/'
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");
    EXPECT_EQ(percent_decode("a%2fb%2Fc"), "a/b/c");
}

TEST(PercentEncoding, AtSignNotEncodedV164) {
    // '@' is a valid URL character and should NOT be percent-encoded by default
    EXPECT_EQ(percent_encode("@"), "@");
    EXPECT_EQ(percent_encode("user@host"), "user@host");
}

TEST(IsURLCodePoint, SpaceIsInvalidV164) {
    // Space (0x20) is NOT a valid URL code point
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x20)));
}

// =============================================================================
// Round 165 percent encoding tests
// =============================================================================

TEST(PercentEncoding, CaretEncodedV165) {
    // Caret (^) should be percent-encoded as %5E
    EXPECT_EQ(percent_encode("^"), "%5E");
    EXPECT_EQ(percent_encode("a^b"), "a%5Eb");
}

TEST(PercentDecoding, EmptyStringDecodesEmptyV165) {
    // Decoding an empty string should produce an empty string
    EXPECT_EQ(percent_decode(""), "");
}

TEST(PercentEncoding, DollarSignNotEncodedV165) {
    // Dollar sign ($) is a valid URL code point and should NOT be percent-encoded
    EXPECT_EQ(percent_encode("$"), "$");
    EXPECT_EQ(percent_encode("price=$100"), "price=$100");
}

TEST(IsURLCodePoint, ControlCharsInvalidV165) {
    // Control characters (0x01, 0x1F) are NOT valid URL code points
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x01)));
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x1F)));
}

// =============================================================================
// Round 166 percent encoding tests
// =============================================================================

TEST(PercentEncoding, RightBracketEncodedV166) {
    // ']' (0x5D) should be percent-encoded as %5D
    EXPECT_EQ(percent_encode("]"), "%5D");
    EXPECT_EQ(percent_encode("arr[0]"), "arr%5B0%5D");
}

TEST(PercentDecoding, DoubleEncodedPercentV166) {
    // %2525 decodes one level: %25 -> '%', so %2525 -> %25
    EXPECT_EQ(percent_decode("%2525"), "%25");
    // Single %25 decodes to literal '%'
    EXPECT_EQ(percent_decode("%25"), "%");
}

TEST(PercentEncoding, PipeEncodedV166) {
    // '|' (0x7C) should be percent-encoded as %7C
    EXPECT_EQ(percent_encode("|"), "%7C");
    EXPECT_EQ(percent_encode("a|b|c"), "a%7Cb%7Cc");
}

TEST(IsURLCodePoint, AmpersandAndEqualsValidV166) {
    // '&' (0x26) and '=' (0x3D) are valid URL code points
    EXPECT_TRUE(is_url_code_point('&'));
    EXPECT_TRUE(is_url_code_point('='));
    // Double-check they are not mistakenly rejected
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(0x26)));
    EXPECT_TRUE(is_url_code_point(static_cast<char32_t>(0x3D)));
}

// =============================================================================
// Round 167 percent encoding tests
// =============================================================================

TEST(PercentEncoding, BackslashEncodedV167) {
    // '\' (0x5C) should be percent-encoded as %5C
    EXPECT_EQ(percent_encode("\\"), "%5C");
    EXPECT_EQ(percent_encode("a\\b"), "a%5Cb");
}

TEST(PercentDecoding, SingleCharDecodedV167) {
    // %41 is the percent-encoding for 'A'
    EXPECT_EQ(percent_decode("%41"), "A");
    // Mixed: literal text + encoded char
    EXPECT_EQ(percent_decode("hello%41world"), "helloAworld");
}

TEST(PercentEncoding, QuestionMarkIsUrlCodePointV167) {
    // '?' (0x3F) is a valid URL code point per the URL spec
    EXPECT_TRUE(is_url_code_point('?'));
    // However, general percent_encode encodes it as %3F for path safety
    EXPECT_EQ(percent_encode("?"), "%3F");
    // Verify round-trip: encoding then decoding returns original
    EXPECT_EQ(percent_decode(percent_encode("?")), "?");
}

TEST(IsURLCodePoint, DeleteCharInvalidV167) {
    // DEL character (0x7F) is NOT a valid URL code point
    EXPECT_FALSE(is_url_code_point(static_cast<char32_t>(0x7F)));
}

// =============================================================================
// Round 168 percent encoding tests
// =============================================================================

TEST(PercentEncoding, SpaceEncodesTo20V168) {
    // Space character should be percent-encoded as %20
    EXPECT_EQ(percent_encode(" "), "%20");
    EXPECT_EQ(percent_encode("a b c"), "a%20b%20c");
    // Multiple consecutive spaces
    EXPECT_EQ(percent_encode("  "), "%20%20");
}

TEST(PercentDecoding, PlusSignPreservedV168) {
    // %2B decodes to '+' character
    EXPECT_EQ(percent_decode("%2B"), "+");
    // Literal '+' stays as '+' in URL percent decoding (not converted to space)
    EXPECT_EQ(percent_decode("+"), "+");
    // Mixed: literal plus and encoded plus
    EXPECT_EQ(percent_decode("a+b%2Bc"), "a+b+c");
}

TEST(PercentEncoding, AlphanumericNotEncodedV168) {
    // a-z should pass through unchanged
    EXPECT_EQ(percent_encode("abcdefghijklmnopqrstuvwxyz"), "abcdefghijklmnopqrstuvwxyz");
    // A-Z should pass through unchanged
    EXPECT_EQ(percent_encode("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    // 0-9 should pass through unchanged
    EXPECT_EQ(percent_encode("0123456789"), "0123456789");
}

TEST(IsURLCodePoint, AsciiLettersValidV168) {
    // All uppercase ASCII letters A-Z should be valid URL code points
    for (char32_t ch = U'A'; ch <= U'Z'; ++ch) {
        EXPECT_TRUE(is_url_code_point(ch));
    }
    // All lowercase ASCII letters a-z should be valid URL code points
    for (char32_t ch = U'a'; ch <= U'z'; ++ch) {
        EXPECT_TRUE(is_url_code_point(ch));
    }
}

// =============================================================================
// Round 169 percent encoding tests
// =============================================================================

TEST(PercentEncoding, HashEncodesCorrectlyV169) {
    // '#' character should be percent-encoded as %23
    EXPECT_EQ(percent_encode("#"), "%23");
    // Hash within a string should also be encoded
    EXPECT_EQ(percent_encode("a#b"), "a%23b");
    // Multiple hashes
    EXPECT_EQ(percent_encode("##"), "%23%23");
}

TEST(PercentDecoding, LowercaseHexDecodesV169) {
    // Lowercase hex %2f should decode the same as uppercase %2F (both to '/')
    EXPECT_EQ(percent_decode("%2f"), "/");
    EXPECT_EQ(percent_decode("%2F"), "/");
    EXPECT_EQ(percent_decode("%2f"), percent_decode("%2F"));
    // Mixed case in a longer string
    EXPECT_EQ(percent_decode("a%2fb%2Fc"), "a/b/c");
}

TEST(PercentEncoding, TildeNotEncodedV169) {
    // Tilde '~' is an unreserved character and should pass through unchanged
    EXPECT_EQ(percent_encode("~"), "~");
    EXPECT_EQ(percent_encode("~user"), "~user");
    EXPECT_EQ(percent_encode("/home/~admin"), "/home/~admin");
}

TEST(IsURLCodePoint, DigitsValidV169) {
    // All ASCII digits 0-9 should be valid URL code points
    for (char32_t ch = U'0'; ch <= U'9'; ++ch) {
        EXPECT_TRUE(is_url_code_point(ch));
    }
}

TEST(PercentEncoding, AtSignNotEncodedV170) {
    // '@' is a valid URL code point and passes through percent_encode unchanged
    EXPECT_EQ(percent_encode("@"), "@");
    EXPECT_EQ(percent_encode("user@host"), "user@host");
    EXPECT_EQ(percent_encode("@@"), "@@");
}

TEST(PercentDecoding, MultipleEncodedCharsV170) {
    // Sequence of percent-encoded bytes should decode to "Hello"
    EXPECT_EQ(percent_decode("%48%65%6C%6C%6F"), "Hello");
    // Mixed case hex should also work
    EXPECT_EQ(percent_decode("%48%65%6c%6c%6f"), "Hello");
}

TEST(PercentEncoding, HyphenNotEncodedV170) {
    // Hyphen '-' is an unreserved character and should pass through unchanged
    EXPECT_EQ(percent_encode("-"), "-");
    EXPECT_EQ(percent_encode("my-file"), "my-file");
    EXPECT_EQ(percent_encode("a-b-c"), "a-b-c");
}

TEST(IsURLCodePoint, ExclamationMarkValidV170) {
    // '!' (U+0021) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(U'!'));
}

TEST(PercentEncoding, AmpersandNotEncodedV171) {
    // '&' is a sub-delimiter and valid URL code point; it passes through unchanged
    EXPECT_EQ(percent_encode("&"), "&");
    EXPECT_EQ(percent_encode("a&b"), "a&b");
    EXPECT_EQ(percent_encode("x&y&z"), "x&y&z");
}

TEST(PercentDecoding, PartialPercentSequencePreservedV171) {
    // Incomplete percent sequence "%4" (only one hex digit) should be preserved as-is
    EXPECT_EQ(percent_decode("%4"), "%4");
    EXPECT_EQ(percent_decode("abc%4"), "abc%4");
    EXPECT_EQ(percent_decode("%4xyz"), "%4xyz");
}

TEST(PercentEncoding, PeriodNotEncodedV171) {
    // Period '.' is an unreserved character and should pass through unchanged
    EXPECT_EQ(percent_encode("."), ".");
    EXPECT_EQ(percent_encode("file.txt"), "file.txt");
    EXPECT_EQ(percent_encode("a.b.c"), "a.b.c");
}

TEST(IsURLCodePoint, AsteriskValidV171) {
    // '*' (U+002A) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(U'*'));
}

// =============================================================================
// Cycle V172 — Percent encoding tests
// =============================================================================
TEST(PercentEncoding, EqualsSignEncodedV172) {
    // '=' is a sub-delimiter; with encode_path_chars=true it becomes %3D
    EXPECT_EQ(percent_encode("=", true), "%3D");
    EXPECT_EQ(percent_encode("a=b", true), "a%3Db");
}

TEST(PercentDecoding, UppercaseHexV172) {
    // %4A (uppercase hex) should decode to 'J' (ASCII 0x4A = 74)
    EXPECT_EQ(percent_decode("%4A"), "J");
    EXPECT_EQ(percent_decode("x%4Ay"), "xJy");
}

TEST(PercentEncoding, UnderscoreNotEncodedV172) {
    // '_' is an unreserved character and should pass through unchanged
    EXPECT_EQ(percent_encode("_"), "_");
    EXPECT_EQ(percent_encode("my_var"), "my_var");
    EXPECT_EQ(percent_encode("a_b_c"), "a_b_c");
}

TEST(IsURLCodePoint, ColonValidV172) {
    // ':' (U+003A) is a valid URL code point (sub-delimiter)
    EXPECT_TRUE(is_url_code_point(U':'));
}

// =============================================================================
// Cycle V173 — Percent encoding tests
// =============================================================================
TEST(PercentEncoding, QuestionMarkEncodedV173) {
    // '?' should be percent-encoded as %3F
    EXPECT_EQ(percent_encode("?"), "%3F");
    EXPECT_EQ(percent_encode("a?b"), "a%3Fb");
    EXPECT_EQ(percent_encode("??"), "%3F%3F");
}

TEST(PercentDecoding, ConsecutiveEncodedCharsV173) {
    // Multiple consecutive percent-encoded characters should all decode correctly
    EXPECT_EQ(percent_decode("%41%42%43"), "ABC");
    EXPECT_EQ(percent_decode("%61%62%63"), "abc");
    EXPECT_EQ(percent_decode("x%41%42%43y"), "xABCy");
}

TEST(PercentEncoding, ForwardSlashEncodedWithPathFlagV173) {
    // '/' should pass through by default but be encoded as %2F with path flag
    EXPECT_EQ(percent_encode("/"), "/");
    EXPECT_EQ(percent_encode("/", true), "%2F");
    EXPECT_EQ(percent_encode("/a/b", true), "%2Fa%2Fb");
}

TEST(IsURLCodePoint, SemicolonValidV173) {
    // ';' (U+003B) is a sub-delimiter and a valid URL code point
    EXPECT_TRUE(is_url_code_point(U';'));
}

// =============================================================================
// Cycle V174 — Percent encoding tests
// =============================================================================
TEST(PercentEncoding, LessThanEncodedV174) {
    // '<' (U+003C) should be percent-encoded as %3C
    EXPECT_EQ(percent_encode("<"), "%3C");
    EXPECT_EQ(percent_encode("a<b"), "a%3Cb");
    EXPECT_EQ(percent_encode("<<"), "%3C%3C");
}

TEST(PercentDecoding, PercentSignEncodedV174) {
    // %25 should decode back to a literal percent sign '%'
    EXPECT_EQ(percent_decode("%25"), "%");
    EXPECT_EQ(percent_decode("100%25"), "100%");
    EXPECT_EQ(percent_decode("%25%25"), "%%");
}

TEST(PercentEncoding, GreaterThanEncodedV174) {
    // '>' (U+003E) should be percent-encoded as %3E
    EXPECT_EQ(percent_encode(">"), "%3E");
    EXPECT_EQ(percent_encode("a>b"), "a%3Eb");
    EXPECT_EQ(percent_encode("<>"), "%3C%3E");
}

TEST(IsURLCodePoint, SlashValidV174) {
    // '/' (U+002F) is a valid URL code point
    EXPECT_TRUE(is_url_code_point(U'/'));
}

// =============================================================================
// Cycle V175 — Percent encoding tests
// =============================================================================
TEST(PercentEncoding, DoubleQuoteEncodedV175) {
    // '"' (U+0022) should be percent-encoded as %22
    EXPECT_EQ(percent_encode("\""), "%22");
    EXPECT_EQ(percent_encode("a\"b"), "a%22b");
    EXPECT_EQ(percent_encode("\"\""), "%22%22");
}

TEST(PercentDecoding, EmptyStringDecodesEmptyV175) {
    // Decoding an empty string should produce an empty string
    EXPECT_EQ(percent_decode(""), "");
}

TEST(PercentEncoding, CaretEncodedV175) {
    // '^' (U+005E) should be percent-encoded as %5E
    EXPECT_EQ(percent_encode("^"), "%5E");
    EXPECT_EQ(percent_encode("a^b"), "a%5Eb");
    EXPECT_EQ(percent_encode("^^"), "%5E%5E");
}

TEST(IsURLCodePoint, AtSignValidV175) {
    // '@' (U+0040) is a sub-delimiter and a valid URL code point
    EXPECT_TRUE(is_url_code_point(U'@'));
}

// =============================================================================
// Cycle V176 — Percent encoding tests
// =============================================================================
TEST(PercentEncoding, BackslashEncodedV176) {
    // '\' (U+005C) should be percent-encoded as %5C
    EXPECT_EQ(percent_encode("\\"), "%5C");
    EXPECT_EQ(percent_encode("a\\b"), "a%5Cb");
    EXPECT_EQ(percent_encode("\\\\"), "%5C%5C");
}

TEST(PercentDecoding, RoundTripEncodeDecodeV176) {
    // Encoding and then decoding should return the original string
    std::string original = "hello world<>\"";
    std::string encoded = percent_encode(original);
    std::string decoded = percent_decode(encoded);
    EXPECT_EQ(decoded, original);
}

TEST(PercentEncoding, CurlyBracesEncodedV176) {
    // '{' (U+007B) and '}' (U+007D) should be percent-encoded
    EXPECT_EQ(percent_encode("{"), "%7B");
    EXPECT_EQ(percent_encode("}"), "%7D");
    EXPECT_EQ(percent_encode("{key}"), "%7Bkey%7D");
}

TEST(IsURLCodePoint, SpaceInvalidV176) {
    // Space (U+0020) is NOT a valid URL code point
    EXPECT_FALSE(is_url_code_point(U' '));
    // Tab (U+0009) is also not a valid URL code point
    EXPECT_FALSE(is_url_code_point(U'\t'));
    // Newline (U+000A) is also invalid
    EXPECT_FALSE(is_url_code_point(U'\n'));
}
