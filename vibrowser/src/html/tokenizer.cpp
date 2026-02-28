#include <clever/html/tokenizer.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace clever::html {

Tokenizer::Tokenizer(std::string_view input) : input_(input) {}

char Tokenizer::consume() {
    if (pos_ < input_.size()) {
        return input_[pos_++];
    }
    return '\0';
}

char Tokenizer::peek() const {
    if (pos_ < input_.size()) {
        return input_[pos_];
    }
    return '\0';
}

bool Tokenizer::at_end() const {
    return pos_ >= input_.size();
}

void Tokenizer::reconsume() {
    if (pos_ > 0) {
        --pos_;
    }
}

bool Tokenizer::is_appropriate_end_tag() const {
    return !last_start_tag_.empty() && current_token_.name == last_start_tag_;
}

Token Tokenizer::emit_character(char c) {
    Token t;
    t.type = Token::Character;
    t.data = std::string(1, c);
    return t;
}

Token Tokenizer::emit_string(const std::string& s) {
    Token t;
    t.type = Token::Character;
    t.data = s;
    return t;
}

std::string Tokenizer::try_consume_entity() {
    // Called after '&' has been consumed. Try to match named or numeric entity.
    size_t start = pos_;

    if (at_end()) return "&";

    // Numeric character reference: &#...;
    if (peek() == '#') {
        consume(); // '#'
        if (at_end()) { pos_ = start; return "&"; }

        bool hex = false;
        if (peek() == 'x' || peek() == 'X') {
            hex = true;
            consume();
        }

        std::string digits;
        if (hex) {
            while (!at_end() && std::isxdigit(static_cast<unsigned char>(peek()))) {
                digits += consume();
            }
        } else {
            while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
                digits += consume();
            }
        }

        if (digits.empty()) { pos_ = start; return "&"; }

        // Consume optional ';'
        if (!at_end() && peek() == ';') consume();

        unsigned long codepoint = std::strtoul(digits.c_str(), nullptr, hex ? 16 : 10);
        if (codepoint == 0 || codepoint > 0x10FFFF) return "\xEF\xBF\xBD"; // replacement char

        // Encode UTF-8
        std::string result;
        if (codepoint < 0x80) {
            result += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            result += static_cast<char>(0xC0 | (codepoint >> 6));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            result += static_cast<char>(0xE0 | (codepoint >> 12));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            result += static_cast<char>(0xF0 | (codepoint >> 18));
            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        return result;
    }

    // Named character reference: &name;
    // Read alphanumeric characters
    std::string name;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == ';')) {
        char c = consume();
        name += c;
        if (c == ';') break;
    }

    // Check whether the entity was properly terminated with ';'
    bool has_semicolon = !name.empty() && name.back() == ';';

    // Strip trailing ';' for lookup
    std::string lookup = name;
    if (has_semicolon) {
        lookup.pop_back();
    }

    // Comprehensive HTML named character references (WHATWG spec)
    static const std::unordered_map<std::string, std::string> entities = {
        // === Core entities ===
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},

        // === Latin-1 Supplement (U+00A0-U+00FF) ===
        {"nbsp", "\xC2\xA0"},     // U+00A0 non-breaking space
        {"iexcl", "\xC2\xA1"},    // U+00A1 inverted exclamation mark
        {"cent", "\xC2\xA2"},     // U+00A2 cent sign
        {"pound", "\xC2\xA3"},    // U+00A3 pound sign
        {"curren", "\xC2\xA4"},   // U+00A4 currency sign
        {"yen", "\xC2\xA5"},      // U+00A5 yen sign
        {"brvbar", "\xC2\xA6"},   // U+00A6 broken bar
        {"sect", "\xC2\xA7"},     // U+00A7 section sign
        {"uml", "\xC2\xA8"},      // U+00A8 diaeresis
        {"copy", "\xC2\xA9"},     // U+00A9 copyright sign
        {"ordf", "\xC2\xAA"},     // U+00AA feminine ordinal indicator
        {"laquo", "\xC2\xAB"},    // U+00AB left-pointing double angle quotation
        {"not", "\xC2\xAC"},      // U+00AC not sign
        {"shy", "\xC2\xAD"},      // U+00AD soft hyphen
        {"reg", "\xC2\xAE"},      // U+00AE registered sign
        {"macr", "\xC2\xAF"},     // U+00AF macron
        {"deg", "\xC2\xB0"},      // U+00B0 degree sign
        {"plusmn", "\xC2\xB1"},   // U+00B1 plus-minus sign
        {"sup2", "\xC2\xB2"},     // U+00B2 superscript two
        {"sup3", "\xC2\xB3"},     // U+00B3 superscript three
        {"acute", "\xC2\xB4"},    // U+00B4 acute accent
        {"micro", "\xC2\xB5"},    // U+00B5 micro sign
        {"para", "\xC2\xB6"},     // U+00B6 pilcrow sign
        {"middot", "\xC2\xB7"},   // U+00B7 middle dot
        {"cedil", "\xC2\xB8"},    // U+00B8 cedilla
        {"sup1", "\xC2\xB9"},     // U+00B9 superscript one
        {"ordm", "\xC2\xBA"},     // U+00BA masculine ordinal indicator
        {"raquo", "\xC2\xBB"},    // U+00BB right-pointing double angle quotation
        {"frac14", "\xC2\xBC"},   // U+00BC vulgar fraction one quarter
        {"frac12", "\xC2\xBD"},   // U+00BD vulgar fraction one half
        {"frac34", "\xC2\xBE"},   // U+00BE vulgar fraction three quarters
        {"iquest", "\xC2\xBF"},   // U+00BF inverted question mark

        // === Latin Extended-A: Uppercase accented (U+00C0-U+00DE) ===
        {"Agrave", "\xC3\x80"},   // U+00C0
        {"Aacute", "\xC3\x81"},   // U+00C1
        {"Acirc", "\xC3\x82"},    // U+00C2
        {"Atilde", "\xC3\x83"},   // U+00C3
        {"Auml", "\xC3\x84"},     // U+00C4
        {"Aring", "\xC3\x85"},    // U+00C5
        {"AElig", "\xC3\x86"},    // U+00C6
        {"Ccedil", "\xC3\x87"},   // U+00C7
        {"Egrave", "\xC3\x88"},   // U+00C8
        {"Eacute", "\xC3\x89"},   // U+00C9
        {"Ecirc", "\xC3\x8A"},    // U+00CA
        {"Euml", "\xC3\x8B"},     // U+00CB
        {"Igrave", "\xC3\x8C"},   // U+00CC
        {"Iacute", "\xC3\x8D"},   // U+00CD
        {"Icirc", "\xC3\x8E"},    // U+00CE
        {"Iuml", "\xC3\x8F"},     // U+00CF
        {"ETH", "\xC3\x90"},      // U+00D0
        {"Ntilde", "\xC3\x91"},   // U+00D1
        {"Ograve", "\xC3\x92"},   // U+00D2
        {"Oacute", "\xC3\x93"},   // U+00D3
        {"Ocirc", "\xC3\x94"},    // U+00D4
        {"Otilde", "\xC3\x95"},   // U+00D5
        {"Ouml", "\xC3\x96"},     // U+00D6
        {"times", "\xC3\x97"},    // U+00D7 multiplication sign
        {"Oslash", "\xC3\x98"},   // U+00D8
        {"Ugrave", "\xC3\x99"},   // U+00D9
        {"Uacute", "\xC3\x9A"},   // U+00DA
        {"Ucirc", "\xC3\x9B"},    // U+00DB
        {"Uuml", "\xC3\x9C"},     // U+00DC
        {"Yacute", "\xC3\x9D"},   // U+00DD
        {"THORN", "\xC3\x9E"},    // U+00DE

        // === Latin Extended-A: Lowercase accented (U+00DF-U+00FF) ===
        {"szlig", "\xC3\x9F"},    // U+00DF sharp s
        {"agrave", "\xC3\xA0"},   // U+00E0
        {"aacute", "\xC3\xA1"},   // U+00E1
        {"acirc", "\xC3\xA2"},    // U+00E2
        {"atilde", "\xC3\xA3"},   // U+00E3
        {"auml", "\xC3\xA4"},     // U+00E4
        {"aring", "\xC3\xA5"},    // U+00E5
        {"aelig", "\xC3\xA6"},    // U+00E6
        {"ccedil", "\xC3\xA7"},   // U+00E7
        {"egrave", "\xC3\xA8"},   // U+00E8
        {"eacute", "\xC3\xA9"},   // U+00E9
        {"ecirc", "\xC3\xAA"},    // U+00EA
        {"euml", "\xC3\xAB"},     // U+00EB
        {"igrave", "\xC3\xAC"},   // U+00EC
        {"iacute", "\xC3\xAD"},   // U+00ED
        {"icirc", "\xC3\xAE"},    // U+00EE
        {"iuml", "\xC3\xAF"},     // U+00EF
        {"eth", "\xC3\xB0"},      // U+00F0
        {"ntilde", "\xC3\xB1"},   // U+00F1
        {"ograve", "\xC3\xB2"},   // U+00F2
        {"oacute", "\xC3\xB3"},   // U+00F3
        {"ocirc", "\xC3\xB4"},    // U+00F4
        {"otilde", "\xC3\xB5"},   // U+00F5
        {"ouml", "\xC3\xB6"},     // U+00F6
        {"divide", "\xC3\xB7"},   // U+00F7 division sign
        {"oslash", "\xC3\xB8"},   // U+00F8
        {"ugrave", "\xC3\xB9"},   // U+00F9
        {"uacute", "\xC3\xBA"},   // U+00FA
        {"ucirc", "\xC3\xBB"},    // U+00FB
        {"uuml", "\xC3\xBC"},     // U+00FC
        {"yacute", "\xC3\xBD"},   // U+00FD
        {"thorn", "\xC3\xBE"},    // U+00FE
        {"yuml", "\xC3\xBF"},     // U+00FF

        // === Latin Extended-B and other Latin (U+0100+) ===
        {"fnof", "\xC6\x92"},     // U+0192 latin small f with hook
        {"OElig", "\xC5\x92"},    // U+0152 latin capital ligature OE
        {"oelig", "\xC5\x93"},    // U+0153 latin small ligature oe
        {"Scaron", "\xC5\xA0"},   // U+0160 latin capital S with caron
        {"scaron", "\xC5\xA1"},   // U+0161 latin small s with caron
        {"Yuml", "\xC5\xB8"},     // U+0178 latin capital Y with diaeresis

        // === Spacing modifier letters ===
        {"circ", "\xCB\x86"},     // U+02C6 modifier letter circumflex accent
        {"tilde", "\xCB\x9C"},    // U+02DC small tilde

        // === Greek capital letters (U+0391-U+03A9) ===
        {"Alpha", "\xCE\x91"},    // U+0391
        {"Beta", "\xCE\x92"},     // U+0392
        {"Gamma", "\xCE\x93"},    // U+0393
        {"Delta", "\xCE\x94"},    // U+0394
        {"Epsilon", "\xCE\x95"},  // U+0395
        {"Zeta", "\xCE\x96"},     // U+0396
        {"Eta", "\xCE\x97"},      // U+0397
        {"Theta", "\xCE\x98"},    // U+0398
        {"Iota", "\xCE\x99"},     // U+0399
        {"Kappa", "\xCE\x9A"},    // U+039A
        {"Lambda", "\xCE\x9B"},   // U+039B
        {"Mu", "\xCE\x9C"},       // U+039C
        {"Nu", "\xCE\x9D"},       // U+039D
        {"Xi", "\xCE\x9E"},       // U+039E
        {"Omicron", "\xCE\x9F"},  // U+039F
        {"Pi", "\xCE\xA0"},       // U+03A0
        {"Rho", "\xCE\xA1"},      // U+03A1
        {"Sigma", "\xCE\xA3"},    // U+03A3
        {"Tau", "\xCE\xA4"},      // U+03A4
        {"Upsilon", "\xCE\xA5"},  // U+03A5
        {"Phi", "\xCE\xA6"},      // U+03A6
        {"Chi", "\xCE\xA7"},      // U+03A7
        {"Psi", "\xCE\xA8"},      // U+03A8
        {"Omega", "\xCE\xA9"},    // U+03A9

        // === Greek lowercase letters (U+03B1-U+03C9) ===
        {"alpha", "\xCE\xB1"},    // U+03B1
        {"beta", "\xCE\xB2"},     // U+03B2
        {"gamma", "\xCE\xB3"},    // U+03B3
        {"delta", "\xCE\xB4"},    // U+03B4
        {"epsilon", "\xCE\xB5"},  // U+03B5
        {"zeta", "\xCE\xB6"},     // U+03B6
        {"eta", "\xCE\xB7"},      // U+03B7
        {"theta", "\xCE\xB8"},    // U+03B8
        {"iota", "\xCE\xB9"},     // U+03B9
        {"kappa", "\xCE\xBA"},    // U+03BA
        {"lambda", "\xCE\xBB"},   // U+03BB
        {"mu", "\xCE\xBC"},       // U+03BC
        {"nu", "\xCE\xBD"},       // U+03BD
        {"xi", "\xCE\xBE"},       // U+03BE
        {"omicron", "\xCE\xBF"},  // U+03BF
        {"pi", "\xCF\x80"},       // U+03C0
        {"rho", "\xCF\x81"},      // U+03C1
        {"sigmaf", "\xCF\x82"},   // U+03C2 final sigma
        {"sigma", "\xCF\x83"},    // U+03C3
        {"tau", "\xCF\x84"},      // U+03C4
        {"upsilon", "\xCF\x85"},  // U+03C5
        {"phi", "\xCF\x86"},      // U+03C6
        {"chi", "\xCF\x87"},      // U+03C7
        {"psi", "\xCF\x88"},      // U+03C8
        {"omega", "\xCF\x89"},    // U+03C9

        // === Greek symbols ===
        {"thetasym", "\xCF\x91"}, // U+03D1 theta symbol
        {"upsih", "\xCF\x92"},    // U+03D2 upsilon with hook symbol
        {"piv", "\xCF\x96"},      // U+03D6 pi symbol

        // === Cyrillic letters (U+0400-U+04FF) ===
        {"IOcy", "\xD0\x81"},     // U+0401
        {"DJcy", "\xD0\x82"},     // U+0402
        {"GJcy", "\xD0\x83"},     // U+0403
        {"Jukcy", "\xD0\x84"},    // U+0404
        {"DScy", "\xD0\x85"},     // U+0405
        {"Iukcy", "\xD0\x86"},    // U+0406
        {"YIcy", "\xD0\x87"},     // U+0407
        {"Jsercy", "\xD0\x88"},   // U+0408
        {"LJcy", "\xD0\x89"},     // U+0409
        {"NJcy", "\xD0\x8A"},     // U+040A
        {"TSHcy", "\xD0\x8B"},    // U+040B
        {"KJcy", "\xD0\x8C"},     // U+040C
        {"Ubrcy", "\xD0\x8E"},    // U+040E
        {"DZcy", "\xD0\x8F"},     // U+040F
        {"Acy", "\xD0\x90"},      // U+0410
        {"Bcy", "\xD0\x91"},      // U+0411
        {"Vcy", "\xD0\x92"},      // U+0412
        {"Gcy", "\xD0\x93"},      // U+0413
        {"Dcy", "\xD0\x94"},      // U+0414
        {"IEcy", "\xD0\x95"},     // U+0415
        {"ZHcy", "\xD0\x96"},     // U+0416
        {"Zcy", "\xD0\x97"},      // U+0417
        {"Icy", "\xD0\x98"},      // U+0418
        {"Jcy", "\xD0\x99"},      // U+0419
        {"Kcy", "\xD0\x9A"},      // U+041A
        {"Lcy", "\xD0\x9B"},      // U+041B
        {"Mcy", "\xD0\x9C"},      // U+041C
        {"Ncy", "\xD0\x9D"},      // U+041D
        {"Ocy", "\xD0\x9E"},      // U+041E
        {"Pcy", "\xD0\x9F"},      // U+041F
        {"Rcy", "\xD0\xA0"},      // U+0420
        {"Scy", "\xD0\xA1"},      // U+0421
        {"Tcy", "\xD0\xA2"},      // U+0422
        {"Ucy", "\xD0\xA3"},      // U+0423
        {"Fcy", "\xD0\xA4"},      // U+0424
        {"KHcy", "\xD0\xA5"},     // U+0425
        {"TScy", "\xD0\xA6"},     // U+0426
        {"CHcy", "\xD0\xA7"},     // U+0427
        {"SHcy", "\xD0\xA8"},     // U+0428
        {"SHCHcy", "\xD0\xA9"},   // U+0429
        {"HARDcy", "\xD0\xAA"},   // U+042A
        {"Ycy", "\xD0\xAB"},      // U+042B
        {"SOFTcy", "\xD0\xAC"},   // U+042C
        {"Ecy", "\xD0\xAD"},      // U+042D
        {"YUcy", "\xD0\xAE"},     // U+042E
        {"YAcy", "\xD0\xAF"},     // U+042F
        {"acy", "\xD0\xB0"},      // U+0430
        {"bcy", "\xD0\xB1"},      // U+0431
        {"vcy", "\xD0\xB2"},      // U+0432
        {"gcy", "\xD0\xB3"},      // U+0433
        {"dcy", "\xD0\xB4"},      // U+0434
        {"iecy", "\xD0\xB5"},     // U+0435
        {"zhcy", "\xD0\xB6"},     // U+0436
        {"zcy", "\xD0\xB7"},      // U+0437
        {"icy", "\xD0\xB8"},      // U+0438
        {"jcy", "\xD0\xB9"},      // U+0439
        {"kcy", "\xD0\xBA"},      // U+043A
        {"lcy", "\xD0\xBB"},      // U+043B
        {"mcy", "\xD0\xBC"},      // U+043C
        {"ncy", "\xD0\xBD"},      // U+043D
        {"ocy", "\xD0\xBE"},      // U+043E
        {"pcy", "\xD0\xBF"},      // U+043F
        {"rcy", "\xD1\x80"},      // U+0440
        {"scy", "\xD1\x81"},      // U+0441
        {"tcy", "\xD1\x82"},      // U+0442
        {"ucy", "\xD1\x83"},      // U+0443
        {"fcy", "\xD1\x84"},      // U+0444
        {"khcy", "\xD1\x85"},     // U+0445
        {"tscy", "\xD1\x86"},     // U+0446
        {"chcy", "\xD1\x87"},     // U+0447
        {"shcy", "\xD1\x88"},     // U+0448
        {"shchcy", "\xD1\x89"},   // U+0449
        {"hardcy", "\xD1\x8A"},   // U+044A
        {"ycy", "\xD1\x8B"},      // U+044B
        {"softcy", "\xD1\x8C"},   // U+044C
        {"ecy", "\xD1\x8D"},      // U+044D
        {"yucy", "\xD1\x8E"},     // U+044E
        {"yacy", "\xD1\x8F"},     // U+044F
        {"iocy", "\xD1\x91"},     // U+0451
        {"djcy", "\xD1\x92"},     // U+0452
        {"gjcy", "\xD1\x93"},     // U+0453
        {"jukcy", "\xD1\x94"},    // U+0454
        {"dscy", "\xD1\x95"},     // U+0455
        {"iukcy", "\xD1\x96"},    // U+0456
        {"yicy", "\xD1\x97"},     // U+0457
        {"jsercy", "\xD1\x98"},   // U+0458
        {"ljcy", "\xD1\x99"},     // U+0459
        {"njcy", "\xD1\x9A"},     // U+045A
        {"tshcy", "\xD1\x9B"},    // U+045B
        {"kjcy", "\xD1\x9C"},     // U+045C
        {"ubrcy", "\xD1\x9E"},    // U+045E
        {"dzcy", "\xD1\x9F"},     // U+045F

        // === General punctuation (U+2000-U+206F) ===
        {"ensp", "\xE2\x80\x82"},     // U+2002 en space
        {"emsp", "\xE2\x80\x83"},     // U+2003 em space
        {"numsp", "\xE2\x80\x87"},    // U+2007 figure space
        {"puncsp", "\xE2\x80\x88"},   // U+2008 punctuation space
        {"thinsp", "\xE2\x80\x89"},   // U+2009 thin space
        {"hairsp", "\xE2\x80\x8A"},   // U+200A hair space
        {"ZeroWidthSpace", "\xE2\x80\x8B"}, // U+200B zero width space
        {"zwnj", "\xE2\x80\x8C"},     // U+200C zero width non-joiner
        {"zwj", "\xE2\x80\x8D"},      // U+200D zero width joiner
        {"lrm", "\xE2\x80\x8E"},      // U+200E left-to-right mark
        {"rlm", "\xE2\x80\x8F"},      // U+200F right-to-left mark
        {"ndash", "\xE2\x80\x93"},    // U+2013 en dash
        {"mdash", "\xE2\x80\x94"},    // U+2014 em dash
        {"lsquo", "\xE2\x80\x98"},    // U+2018 left single quotation mark
        {"OpenCurlyQuote", "\xE2\x80\x98"}, // U+2018
        {"rsquo", "\xE2\x80\x99"},    // U+2019 right single quotation mark
        {"CloseCurlyQuote", "\xE2\x80\x99"}, // U+2019
        {"sbquo", "\xE2\x80\x9A"},    // U+201A single low-9 quotation mark
        {"ldquo", "\xE2\x80\x9C"},    // U+201C left double quotation mark
        {"OpenCurlyDoubleQuote", "\xE2\x80\x9C"}, // U+201C
        {"rdquo", "\xE2\x80\x9D"},    // U+201D right double quotation mark
        {"CloseCurlyDoubleQuote", "\xE2\x80\x9D"}, // U+201D
        {"bdquo", "\xE2\x80\x9E"},    // U+201E double low-9 quotation mark
        {"dagger", "\xE2\x80\xA0"},   // U+2020 dagger
        {"Dagger", "\xE2\x80\xA1"},   // U+2021 double dagger
        {"bull", "\xE2\x80\xA2"},     // U+2022 bullet
        {"hellip", "\xE2\x80\xA6"},   // U+2026 horizontal ellipsis
        {"permil", "\xE2\x80\xB0"},   // U+2030 per mille sign
        {"prime", "\xE2\x80\xB2"},    // U+2032 prime
        {"Prime", "\xE2\x80\xB3"},    // U+2033 double prime
        {"lsaquo", "\xE2\x80\xB9"},   // U+2039 single left-pointing angle quotation
        {"rsaquo", "\xE2\x80\xBA"},   // U+203A single right-pointing angle quotation
        {"oline", "\xE2\x80\xBE"},    // U+203E overline
        {"NoBreak", "\xE2\x81\xA0"},  // U+2060 word joiner
        {"ApplyFunction", "\xE2\x81\xA1"}, // U+2061 function application
        {"InvisibleTimes", "\xE2\x81\xA2"}, // U+2062 invisible times
        {"InvisibleComma", "\xE2\x81\xA3"}, // U+2063 invisible comma

        // === Currency symbols ===
        {"euro", "\xE2\x82\xAC"},     // U+20AC euro sign
        {"frasl", "\xE2\x81\x84"},    // U+2044 fraction slash

        // === Letterlike symbols (U+2100-U+214F) ===
        {"weierp", "\xE2\x84\x98"},   // U+2118 script capital P
        {"image", "\xE2\x84\x91"},    // U+2111 black-letter capital I
        {"real", "\xE2\x84\x9C"},     // U+211C black-letter capital R
        {"trade", "\xE2\x84\xA2"},    // U+2122 trade mark sign
        {"alefsym", "\xE2\x84\xB5"},  // U+2135 alef symbol

        // === Arrows (U+2190-U+21FF) ===
        {"larr", "\xE2\x86\x90"},     // U+2190 leftwards arrow
        {"leftarrow", "\xE2\x86\x90"}, // U+2190
        {"uarr", "\xE2\x86\x91"},     // U+2191 upwards arrow
        {"uparrow", "\xE2\x86\x91"},  // U+2191
        {"rarr", "\xE2\x86\x92"},     // U+2192 rightwards arrow
        {"rightarrow", "\xE2\x86\x92"}, // U+2192
        {"darr", "\xE2\x86\x93"},     // U+2193 downwards arrow
        {"downarrow", "\xE2\x86\x93"}, // U+2193
        {"harr", "\xE2\x86\x94"},     // U+2194 left right arrow
        {"leftrightarrow", "\xE2\x86\x94"}, // U+2194
        {"crarr", "\xE2\x86\xB5"},    // U+21B5 downwards arrow with corner leftwards
        {"lArr", "\xE2\x87\x90"},     // U+21D0 leftwards double arrow
        {"Leftarrow", "\xE2\x87\x90"}, // U+21D0
        {"uArr", "\xE2\x87\x91"},     // U+21D1 upwards double arrow
        {"Uparrow", "\xE2\x87\x91"},  // U+21D1
        {"rArr", "\xE2\x87\x92"},     // U+21D2 rightwards double arrow
        {"Rightarrow", "\xE2\x87\x92"}, // U+21D2
        {"dArr", "\xE2\x87\x93"},     // U+21D3 downwards double arrow
        {"Downarrow", "\xE2\x87\x93"}, // U+21D3
        {"hArr", "\xE2\x87\x94"},     // U+21D4 left right double arrow
        {"Leftrightarrow", "\xE2\x87\x94"}, // U+21D4

        // === Mathematical operators (U+2200-U+22FF) ===
        {"forall", "\xE2\x88\x80"},   // U+2200 for all
        {"part", "\xE2\x88\x82"},     // U+2202 partial differential
        {"exist", "\xE2\x88\x83"},    // U+2203 there exists
        {"empty", "\xE2\x88\x85"},    // U+2205 empty set
        {"nabla", "\xE2\x88\x87"},    // U+2207 nabla
        {"isin", "\xE2\x88\x88"},     // U+2208 element of
        {"notin", "\xE2\x88\x89"},    // U+2209 not an element of
        {"ni", "\xE2\x88\x8B"},       // U+220B contains as member
        {"prod", "\xE2\x88\x8F"},     // U+220F n-ary product
        {"sum", "\xE2\x88\x91"},      // U+2211 n-ary summation
        {"minus", "\xE2\x88\x92"},    // U+2212 minus sign
        {"lowast", "\xE2\x88\x97"},   // U+2217 asterisk operator
        {"radic", "\xE2\x88\x9A"},    // U+221A square root
        {"prop", "\xE2\x88\x9D"},     // U+221D proportional to
        {"infin", "\xE2\x88\x9E"},    // U+221E infinity
        {"ang", "\xE2\x88\xA0"},      // U+2220 angle
        {"and", "\xE2\x88\xA7"},      // U+2227 logical and
        {"or", "\xE2\x88\xA8"},       // U+2228 logical or
        {"cap", "\xE2\x88\xA9"},      // U+2229 intersection
        {"cup", "\xE2\x88\xAA"},      // U+222A union
        {"int", "\xE2\x88\xAB"},      // U+222B integral
        {"there4", "\xE2\x88\xB4"},   // U+2234 therefore
        {"sim", "\xE2\x88\xBC"},      // U+223C tilde operator
        {"cong", "\xE2\x89\x85"},     // U+2245 approximately equal to
        {"asymp", "\xE2\x89\x88"},    // U+2248 almost equal to
        {"ne", "\xE2\x89\xA0"},       // U+2260 not equal to
        {"neq", "\xE2\x89\xA0"},      // U+2260 not equal to
        {"NotEqual", "\xE2\x89\xA0"}, // U+2260 not equal to
        {"equiv", "\xE2\x89\xA1"},    // U+2261 identical to
        {"le", "\xE2\x89\xA4"},       // U+2264 less-than or equal to
        {"leq", "\xE2\x89\xA4"},      // U+2264 less-than or equal to
        {"ge", "\xE2\x89\xA5"},       // U+2265 greater-than or equal to
        {"geq", "\xE2\x89\xA5"},      // U+2265 greater-than or equal to
        {"sub", "\xE2\x8A\x82"},      // U+2282 subset of
        {"sup", "\xE2\x8A\x83"},      // U+2283 superset of
        {"nsub", "\xE2\x8A\x84"},     // U+2284 not a subset of
        {"sube", "\xE2\x8A\x86"},     // U+2286 subset of or equal to
        {"subseteq", "\xE2\x8A\x86"}, // U+2286 subset of or equal to
        {"supe", "\xE2\x8A\x87"},     // U+2287 superset of or equal to
        {"supseteq", "\xE2\x8A\x87"}, // U+2287 superset of or equal to
        {"nsubseteq", "\xE2\x8A\x88"}, // U+2288 not subset of or equal to
        {"nsupseteq", "\xE2\x8A\x89"}, // U+2289 not superset of or equal to
        {"oplus", "\xE2\x8A\x95"},    // U+2295 circled plus
        {"otimes", "\xE2\x8A\x97"},   // U+2297 circled times
        {"perp", "\xE2\x8A\xA5"},     // U+22A5 up tack (perpendicular)
        {"sdot", "\xE2\x8B\x85"},     // U+22C5 dot operator

        // === Miscellaneous technical (U+2300-U+23FF) ===
        {"lceil", "\xE2\x8C\x88"},    // U+2308 left ceiling
        {"rceil", "\xE2\x8C\x89"},    // U+2309 right ceiling
        {"lfloor", "\xE2\x8C\x8A"},   // U+230A left floor
        {"rfloor", "\xE2\x8C\x8B"},   // U+230B right floor
        {"lang", "\xE2\x9F\xA8"},     // U+27E8 mathematical left angle bracket
        {"rang", "\xE2\x9F\xA9"},     // U+27E9 mathematical right angle bracket

        // === Geometric shapes (U+25A0-U+25FF) ===
        {"loz", "\xE2\x97\x8A"},      // U+25CA lozenge

        // === Miscellaneous symbols (U+2600-U+26FF) ===
        {"spades", "\xE2\x99\xA0"},   // U+2660 black spade suit
        {"clubs", "\xE2\x99\xA3"},    // U+2663 black club suit
        {"hearts", "\xE2\x99\xA5"},   // U+2665 black heart suit
        {"diams", "\xE2\x99\xA6"},    // U+2666 black diamond suit
    };

    auto it = entities.find(lookup);
    if (it != entities.end()) {
        // Per WHATWG spec: without trailing ';', only resolve the five XML
        // entities (amp, lt, gt, quot, apos) which browsers historically
        // handle without semicolons.  All others require ';' to avoid
        // misinterpreting URL query strings like &lang=en.
        if (has_semicolon ||
            lookup == "amp" || lookup == "lt" || lookup == "gt" ||
            lookup == "quot" || lookup == "apos") {
            return it->second;
        }
    }

    // Not a recognized entity — rewind and return '&'
    pos_ = start;
    return "&";
}

Token Tokenizer::emit_eof() {
    Token t;
    t.type = Token::EndOfFile;
    return t;
}

void Tokenizer::set_state(TokenizerState state) {
    state_ = state;
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c + 32);
    return c;
}

Token Tokenizer::next_token() {
    if (!pending_character_data_.empty()) {
        const char c = pending_character_data_.front();
        pending_character_data_.erase(0, 1);
        return emit_character(c);
    }

    while (true) {
        switch (state_) {

        // ====================================================================
        // Data state
        // ====================================================================
        case TokenizerState::Data: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::TagOpen;
                continue;
            }
            if (c == '&') {
                return emit_string(try_consume_entity());
            }
            return emit_character(c);
        }

        // ====================================================================
        // Tag Open state
        // ====================================================================
        case TokenizerState::TagOpen: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_character('<');
            }
            char c = consume();
            if (c == '!') {
                state_ = TokenizerState::MarkupDeclarationOpen;
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::EndTagOpen;
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::StartTag;
                current_token_.name.clear();
                current_token_.attributes.clear();
                current_token_.self_closing = false;
                reconsume();
                state_ = TokenizerState::TagName;
                continue;
            }
            if (c == '?') {
                current_token_ = Token{};
                current_token_.type = Token::Comment;
                current_token_.data.clear();
                reconsume();
                state_ = TokenizerState::BogusComment;
                continue;
            }
            // Parse error, emit '<' as character
            state_ = TokenizerState::Data;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // End Tag Open state
        // ====================================================================
        case TokenizerState::EndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                current_token_.attributes.clear();
                current_token_.self_closing = false;
                reconsume();
                state_ = TokenizerState::TagName;
                continue;
            }
            if (c == '>') {
                // Parse error: </>
                state_ = TokenizerState::Data;
                continue;
            }
            current_token_ = Token{};
            current_token_.type = Token::Comment;
            current_token_.data.clear();
            reconsume();
            state_ = TokenizerState::BogusComment;
            continue;
        }

        // ====================================================================
        // Tag Name state
        // ====================================================================
        case TokenizerState::TagName: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.name += to_lower(c);
            continue;
        }

        // ====================================================================
        // Before Attribute Name state
        // ====================================================================
        case TokenizerState::BeforeAttributeName: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '/' || c == '>') {
                reconsume();
                state_ = TokenizerState::AfterAttributeName;
                continue;
            }
            if (c == '=') {
                Attribute attr;
                attr.name = "=";
                current_token_.attributes.push_back(attr);
                state_ = TokenizerState::AttributeName;
                continue;
            }
            current_token_.attributes.push_back(Attribute{});
            reconsume();
            state_ = TokenizerState::AttributeName;
            continue;
        }

        // ====================================================================
        // Attribute Name state
        // ====================================================================
        case TokenizerState::AttributeName: {
            if (at_end()) {
                state_ = TokenizerState::AfterAttributeName;
                continue;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' '
                || c == '/' || c == '>') {
                reconsume();
                state_ = TokenizerState::AfterAttributeName;
                continue;
            }
            if (c == '=') {
                state_ = TokenizerState::BeforeAttributeValue;
                continue;
            }
            current_token_.attributes.back().name += to_lower(c);
            continue;
        }

        // ====================================================================
        // After Attribute Name state
        // ====================================================================
        case TokenizerState::AfterAttributeName: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '=') {
                state_ = TokenizerState::BeforeAttributeValue;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.attributes.push_back(Attribute{});
            reconsume();
            state_ = TokenizerState::AttributeName;
            continue;
        }

        // ====================================================================
        // Before Attribute Value state
        // ====================================================================
        case TokenizerState::BeforeAttributeValue: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '"') {
                state_ = TokenizerState::AttributeValueDoubleQuoted;
                continue;
            }
            if (c == '\'') {
                state_ = TokenizerState::AttributeValueSingleQuoted;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::AttributeValueUnquoted;
            continue;
        }

        // ====================================================================
        // Attribute Value (Double-Quoted) state
        // ====================================================================
        case TokenizerState::AttributeValueDoubleQuoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '"') {
                state_ = TokenizerState::AfterAttributeValueQuoted;
                continue;
            }
            if (c == '&') {
                current_token_.attributes.back().value += try_consume_entity();
                continue;
            }
            current_token_.attributes.back().value += c;
            continue;
        }

        // ====================================================================
        // Attribute Value (Single-Quoted) state
        // ====================================================================
        case TokenizerState::AttributeValueSingleQuoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\'') {
                state_ = TokenizerState::AfterAttributeValueQuoted;
                continue;
            }
            if (c == '&') {
                current_token_.attributes.back().value += try_consume_entity();
                continue;
            }
            current_token_.attributes.back().value += c;
            continue;
        }

        // ====================================================================
        // Attribute Value (Unquoted) state
        // ====================================================================
        case TokenizerState::AttributeValueUnquoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '&') {
                current_token_.attributes.back().value += try_consume_entity();
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.attributes.back().value += c;
            continue;
        }

        // ====================================================================
        // After Attribute Value (Quoted) state
        // ====================================================================
        case TokenizerState::AfterAttributeValueQuoted: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/') {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::BeforeAttributeName;
            continue;
        }

        // ====================================================================
        // Self-Closing Start Tag state
        // ====================================================================
        case TokenizerState::SelfClosingStartTag: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return emit_eof();
            }
            char c = consume();
            if (c == '>') {
                current_token_.self_closing = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::BeforeAttributeName;
            continue;
        }

        // ====================================================================
        // Bogus Comment state
        // ====================================================================
        case TokenizerState::BogusComment: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.data += c;
            continue;
        }

        // ====================================================================
        // Markup Declaration Open state
        // ====================================================================
        case TokenizerState::MarkupDeclarationOpen: {
            // Check for "--" (comment)
            if (pos_ + 1 < input_.size() && input_[pos_] == '-' && input_[pos_ + 1] == '-') {
                pos_ += 2;
                current_token_ = Token{};
                current_token_.type = Token::Comment;
                current_token_.data.clear();
                state_ = TokenizerState::CommentStart;
                continue;
            }
            // Check for DOCTYPE (case-insensitive)
            if (pos_ + 6 < input_.size()) {
                std::string next7;
                for (size_t i = 0; i < 7; ++i) {
                    next7 += static_cast<char>(std::toupper(
                        static_cast<unsigned char>(input_[pos_ + i])));
                }
                if (next7 == "DOCTYPE") {
                    pos_ += 7;
                    state_ = TokenizerState::DOCTYPE;
                    continue;
                }
            }
            // Check for "[CDATA[" (case-sensitive)
            if (pos_ + 6 < input_.size()
                && input_[pos_] == '['
                && input_[pos_ + 1] == 'C'
                && input_[pos_ + 2] == 'D'
                && input_[pos_ + 3] == 'A'
                && input_[pos_ + 4] == 'T'
                && input_[pos_ + 5] == 'A'
                && input_[pos_ + 6] == '[') {
                pos_ += 7;
                state_ = TokenizerState::CDATASection;
                continue;
            }
            // Bogus comment
            current_token_ = Token{};
            current_token_.type = Token::Comment;
            current_token_.data.clear();
            state_ = TokenizerState::BogusComment;
            continue;
        }

        // ====================================================================
        // Comment Start state
        // ====================================================================
        case TokenizerState::CommentStart: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentStartDash;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment Start Dash state
        // ====================================================================
        case TokenizerState::CommentStartDash: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentEnd;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.data += '-';
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment state
        // ====================================================================
        case TokenizerState::Comment: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentEndDash;
                continue;
            }
            current_token_.data += c;
            continue;
        }

        // ====================================================================
        // Comment End Dash state
        // ====================================================================
        case TokenizerState::CommentEndDash: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                state_ = TokenizerState::CommentEnd;
                continue;
            }
            current_token_.data += '-';
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment End state
        // ====================================================================
        case TokenizerState::CommentEnd: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (c == '!') {
                state_ = TokenizerState::CommentEndBang;
                continue;
            }
            if (c == '-') {
                current_token_.data += '-';
                continue;
            }
            current_token_.data += "--";
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // Comment End Bang state
        // ====================================================================
        case TokenizerState::CommentEndBang: {
            if (at_end()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '-') {
                current_token_.data += "--!";
                state_ = TokenizerState::CommentEndDash;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.data += "--!";
            reconsume();
            state_ = TokenizerState::Comment;
            continue;
        }

        // ====================================================================
        // DOCTYPE state
        // ====================================================================
        case TokenizerState::DOCTYPE: {
            if (at_end()) {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::BeforeDOCTYPEName;
                continue;
            }
            if (c == '>') {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            reconsume();
            state_ = TokenizerState::BeforeDOCTYPEName;
            continue;
        }

        // ====================================================================
        // Before DOCTYPE Name state
        // ====================================================================
        case TokenizerState::BeforeDOCTYPEName: {
            if (at_end()) {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '>') {
                current_token_ = Token{};
                current_token_.type = Token::DOCTYPE;
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_ = Token{};
            current_token_.type = Token::DOCTYPE;
            current_token_.name = std::string(1, to_lower(c));
            state_ = TokenizerState::DOCTYPEName;
            continue;
        }

        // ====================================================================
        // DOCTYPE Name state
        // ====================================================================
        case TokenizerState::DOCTYPEName: {
            if (at_end()) {
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                state_ = TokenizerState::AfterDOCTYPEName;
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            current_token_.name += to_lower(c);
            continue;
        }

        // ====================================================================
        // After DOCTYPE Name state
        // ====================================================================
        case TokenizerState::AfterDOCTYPEName: {
            if (at_end()) {
                current_token_.force_quirks = true;
                state_ = TokenizerState::Data;
                return current_token_;
            }
            char c = consume();
            if (c == '\t' || c == '\n' || c == '\f' || c == ' ') {
                continue;
            }
            if (c == '>') {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            // Simplified: skip PUBLIC/SYSTEM identifiers
            while (!at_end()) {
                c = consume();
                if (c == '>') {
                    state_ = TokenizerState::Data;
                    return current_token_;
                }
            }
            current_token_.force_quirks = true;
            state_ = TokenizerState::Data;
            return current_token_;
        }

        // ====================================================================
        // RAWTEXT state
        // ====================================================================
        case TokenizerState::RAWTEXT: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::RAWTEXTLessThanSign;
                continue;
            }
            return emit_character(c);
        }

        // ====================================================================
        // RAWTEXT Less-Than Sign state
        // ====================================================================
        case TokenizerState::RAWTEXTLessThanSign: {
            if (at_end()) {
                state_ = TokenizerState::RAWTEXT;
                return emit_character('<');
            }
            char c = consume();
            if (c == '/') {
                temp_buffer_.clear();
                state_ = TokenizerState::RAWTEXTEndTagOpen;
                continue;
            }
            state_ = TokenizerState::RAWTEXT;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RAWTEXT End Tag Open state
        // ====================================================================
        case TokenizerState::RAWTEXTEndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::RAWTEXT;
                pending_character_data_ = "/";
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                reconsume();
                state_ = TokenizerState::RAWTEXTEndTagName;
                continue;
            }
            state_ = TokenizerState::RAWTEXT;
            reconsume();
            pending_character_data_ = "/";
            return emit_character('<');
        }

        // ====================================================================
        // RAWTEXT End Tag Name state
        // ====================================================================
        case TokenizerState::RAWTEXTEndTagName: {
            if (at_end()) {
                state_ = TokenizerState::RAWTEXT;
                pending_character_data_ = "/";
                pending_character_data_ += temp_buffer_;
                return emit_character('<');
            }
            char c = consume();
            if ((c == '\t' || c == '\n' || c == '\f' || c == ' ')
                && is_appropriate_end_tag()) {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/' && is_appropriate_end_tag()) {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>' && is_appropriate_end_tag()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_.name += to_lower(c);
                temp_buffer_ += c;
                continue;
            }
            state_ = TokenizerState::RAWTEXT;
            reconsume();
            pending_character_data_ = "/";
            pending_character_data_ += temp_buffer_;
            return emit_character('<');
        }

        // ====================================================================
        // RCDATA state
        // ====================================================================
        case TokenizerState::RCDATA: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::RCDATALessThanSign;
                continue;
            }
            if (c == '&') {
                return emit_string(try_consume_entity());
            }
            return emit_character(c);
        }

        // ====================================================================
        // RCDATA Less-Than Sign state
        // ====================================================================
        case TokenizerState::RCDATALessThanSign: {
            if (at_end()) {
                state_ = TokenizerState::RCDATA;
                return emit_character('<');
            }
            char c = consume();
            if (c == '/') {
                temp_buffer_.clear();
                state_ = TokenizerState::RCDATAEndTagOpen;
                continue;
            }
            state_ = TokenizerState::RCDATA;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // RCDATA End Tag Open state
        // ====================================================================
        case TokenizerState::RCDATAEndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::RCDATA;
                pending_character_data_ = "/";
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                reconsume();
                state_ = TokenizerState::RCDATAEndTagName;
                continue;
            }
            state_ = TokenizerState::RCDATA;
            reconsume();
            pending_character_data_ = "/";
            return emit_character('<');
        }

        // ====================================================================
        // RCDATA End Tag Name state
        // ====================================================================
        case TokenizerState::RCDATAEndTagName: {
            if (at_end()) {
                state_ = TokenizerState::RCDATA;
                pending_character_data_ = "/";
                pending_character_data_ += temp_buffer_;
                return emit_character('<');
            }
            char c = consume();
            if ((c == '\t' || c == '\n' || c == '\f' || c == ' ')
                && is_appropriate_end_tag()) {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/' && is_appropriate_end_tag()) {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>' && is_appropriate_end_tag()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_.name += to_lower(c);
                temp_buffer_ += c;
                continue;
            }
            state_ = TokenizerState::RCDATA;
            reconsume();
            pending_character_data_ = "/";
            pending_character_data_ += temp_buffer_;
            return emit_character('<');
        }

        // ====================================================================
        // ScriptData state
        // ====================================================================
        case TokenizerState::ScriptData: {
            if (at_end()) return emit_eof();
            char c = consume();
            if (c == '<') {
                state_ = TokenizerState::ScriptDataLessThanSign;
                continue;
            }
            return emit_character(c);
        }

        // ====================================================================
        // ScriptData Less-Than Sign state
        // ====================================================================
        case TokenizerState::ScriptDataLessThanSign: {
            if (at_end()) {
                state_ = TokenizerState::ScriptData;
                return emit_character('<');
            }
            char c = consume();
            if (c == '/') {
                temp_buffer_.clear();
                state_ = TokenizerState::ScriptDataEndTagOpen;
                continue;
            }
            state_ = TokenizerState::ScriptData;
            reconsume();
            return emit_character('<');
        }

        // ====================================================================
        // ScriptData End Tag Open state
        // ====================================================================
        case TokenizerState::ScriptDataEndTagOpen: {
            if (at_end()) {
                state_ = TokenizerState::ScriptData;
                pending_character_data_ = "/";
                return emit_character('<');
            }
            char c = consume();
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_ = Token{};
                current_token_.type = Token::EndTag;
                current_token_.name.clear();
                reconsume();
                state_ = TokenizerState::ScriptDataEndTagName;
                continue;
            }
            state_ = TokenizerState::ScriptData;
            reconsume();
            pending_character_data_ = "/";
            return emit_character('<');
        }

        // ====================================================================
        // ScriptData End Tag Name state
        // ====================================================================
        case TokenizerState::ScriptDataEndTagName: {
            if (at_end()) {
                state_ = TokenizerState::ScriptData;
                pending_character_data_ = "/";
                pending_character_data_ += temp_buffer_;
                return emit_character('<');
            }
            char c = consume();
            if ((c == '\t' || c == '\n' || c == '\f' || c == ' ')
                && is_appropriate_end_tag()) {
                state_ = TokenizerState::BeforeAttributeName;
                continue;
            }
            if (c == '/' && is_appropriate_end_tag()) {
                state_ = TokenizerState::SelfClosingStartTag;
                continue;
            }
            if (c == '>' && is_appropriate_end_tag()) {
                state_ = TokenizerState::Data;
                return current_token_;
            }
            if (std::isalpha(static_cast<unsigned char>(c))) {
                current_token_.name += to_lower(c);
                temp_buffer_ += c;
                continue;
            }
            state_ = TokenizerState::ScriptData;
            reconsume();
            pending_character_data_ = "/";
            pending_character_data_ += temp_buffer_;
            return emit_character('<');
        }

        // ====================================================================
        // PLAINTEXT state
        // ====================================================================
        case TokenizerState::PLAINTEXT: {
            if (at_end()) return emit_eof();
            return emit_character(consume());
        }

        // ====================================================================
        // Character Reference state (simplified)
        // ====================================================================
        case TokenizerState::CharacterReference: {
            state_ = TokenizerState::Data;
            return emit_character('&');
        }

        // ====================================================================
        // CDATA Section state
        // ====================================================================
        case TokenizerState::CDATASection: {
            if (at_end()) return emit_eof();

            if (pos_ + 2 < input_.size()
                && input_[pos_] == ']'
                && input_[pos_ + 1] == ']'
                && input_[pos_ + 2] == '>') {
                pos_ += 3;
                state_ = TokenizerState::Data;
                continue;
            }

            return emit_character(consume());
        }

        } // end switch
    } // end while
}

} // namespace clever::html
