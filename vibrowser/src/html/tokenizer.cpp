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
        {"pound", "\xC2\xA3"},        // U+00A3 pound sign (additional alias)
        {"yen", "\xC2\xA5"},          // U+00A5 yen sign (additional alias)
        {"cent", "\xC2\xA2"},         // U+00A2 cent sign (additional alias)
        {"curren", "\xC2\xA4"},       // U+00A4 currency sign (additional alias)
        {"rupee", "\xE2\x82\xA8"},    // U+20A8 rupee sign
        {"won", "\xE2\x82\xA9"},      // U+20A9 won sign
        {"cedi", "\xE2\x82\xAA"},     // U+20AA shekel sign
        {"baht", "\xE2\x82\xB1"},     // U+20B1 peso sign

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
        {"wreath", "\xE2\x89\x80"},   // U+2240 wreath product
        {"wr", "\xE2\x89\x80"},       // U+2240 wreath product
        {"Star", "\xE2\x8B\x86"},     // U+22C6 star operator
        {"diamond", "\xE2\x8B\x84"},  // U+22C4 diamond operator
        {"pitchfork", "\xE2\x8B\x94"}, // U+22D4 pitchfork
        {"lessgtr", "\xE2\x89\xB6"},  // U+2276 less-than or greater-than
        {"gtrless", "\xE2\x89\xB7"},  // U+2277 greater-than or less-than
        {"multimap", "\xE2\x8A\xB8"}, // U+22B8 multimap
        {"rightarrowtail", "\xE2\x86\xA3"}, // U+21A3 rightwards arrow with tail
        {"leftarrowtail", "\xE2\x86\xA2"},  // U+21A2 leftwards arrow with tail
        {"looparrowright", "\xE2\x86\xAC"}, // U+21AC rightwards arrow looped
        {"looparrowleft", "\xE2\x86\xAB"},  // U+21AB leftwards arrow looped
        {"rightarrowhead", "\xE2\x87\x81"}, // U+21C1 rightwards harpoon with barb downwards
        {"leftarrowhead", "\xE2\x87\x80"},  // U+21C0 rightwards harpoon with barb upwards
        {"triangle", "\xE2\x96\xB3"},    // U+25B3 white up-pointing triangle
        {"triangledown", "\xE2\x96\xBD"}, // U+25BD white down-pointing triangle
        {"triangleleft", "\xE2\x97\x83"}, // U+25C3 white left-pointing triangle
        {"triangleright", "\xE2\x96\xB9"}, // U+25B9 white right-pointing triangle
        {"varnothing", "\xE2\x88\x85"}, // U+2205 empty set
        {"eq", "="},                    // U+003D equals sign
        {"neq", "\xE2\x89\xA0"},       // U+2260 not equal to (variant)
        {"veq", "\xE2\x89\xB2"},       // U+2272 less-than or equivalent to
        {"geq", "\xE2\x89\xB3"},       // U+2273 greater-than or equivalent to
        {"subne", "\xE2\x8A\x8A"},     // U+228A subset of with not equal to
        {"supne", "\xE2\x8A\x8B"},     // U+228B superset of with not equal to
        {"nsubset", "\xE2\x8A\x82\xCC\xB8"}, // U+2282 combining long solidus overlay
        {"nsupset", "\xE2\x8A\x83\xCC\xB8"}, // U+2283 combining long solidus overlay
        {"curlyeqprec", "\xE2\x8B\x9E"}, // U+22DE curly equals and precedes
        {"curlyeqsucc", "\xE2\x8B\x9F"}, // U+22DF curly equals and succeeds
        {"cdarrowright", "\xE2\x87\x89"}, // U+21C9 downwards arrow to right
        {"cdarrowleft", "\xE2\x87\x88"},  // U+21C8 downwards arrow to left
        {"udarrowleft", "\xE2\x87\x85"},  // U+21C5 upwards arrow downwards arrow
        {"udarrowright", "\xE2\x87\x86"}, // U+21C6 downwards arrow upwards arrow
        {"rbarr", "\xE2\x87\xA5"},     // U+21A5 rightwards arrow from bar
        {"lbarr", "\xE2\x87\xA4"},     // U+21A4 leftwards arrow from bar
        {"UpArrowBar", "\xE2\x91\xA0"}, // U+2340 up arrowbar
        {"DownArrowBar", "\xE2\x91\xA1"}, // U+2341 down arrowbar
        {"UnionPlus", "\xE2\x8A\x8E"}, // U+228E multiset union
        {"shortparallel", "\xE2\x88\xA5"}, // U+2225 parallel to
        {"nshortparallel", "\xE2\x88\xA6"}, // U+2226 not parallel to
        {"parallel", "\xE2\x88\xA5"}, // U+2225 parallel to
        {"nparallel", "\xE2\x88\xA6"}, // U+2226 not parallel to
        {"lessdot", "\xE2\x8B\x96"},  // U+22D6 less-than with dot
        {"gtdot", "\xE2\x8B\x97"},    // U+22D7 greater-than with dot
        {"Ll", "\xE2\x8B\x98"},       // U+22D8 much less-than
        {"Gg", "\xE2\x8B\x99"},       // U+22D9 much greater-than
        {"LessEqualGreater", "\xE2\x8B\x9A"}, // U+22DA less-than equal to greater-than
        {"GreaterEqualLess", "\xE2\x8B\x9B"}, // U+22DB greater-than equal to less-than
        {"lessgtr", "\xE2\x89\xB6"},  // U+2276 less-than or greater-than
        {"gtrless", "\xE2\x89\xB7"},  // U+2277 greater-than or less-than
        {"asympeq", "\xE2\x8B\x9D"},  // U+22DD asymptotically equal to
        {"approxeq", "\xE2\x89\x8A"},  // U+224A approximately equal to
        {"thickapprox", "\xE2\x89\x88"}, // U+2248 almost equal to
        {"thicksim", "\xE2\x88\xBC"},  // U+223C tilde operator
        {"backsim", "\xE2\x89\x83"},  // U+223D reversed tilde
        {"shortmid", "\xE2\x88\xA3"},  // U+2223 divides
        {"nshortmid", "\xE2\x88\xA4"}, // U+2224 does not divide
        {"mid", "\xE2\x88\xA3"},       // U+2223 divides
        {"nmid", "\xE2\x88\xA4"},      // U+2224 does not divide
        {"NotVerticalBar", "\xE2\x88\xA4"}, // U+2224 does not divide
        {"DoubleVerticalBar", "\xE2\x80\x96"}, // U+2016 double vertical line
        {"Vert", "\xE2\x80\x96"},      // U+2016 double vertical line
        {"Vert2", "\xE2\xA0\xA0"},     // U+2980 vertical bar double
        {"NotDoubleVerticalBar", "\xE2\x89\x82"}, // U+2242 not similar
        {"SmallVerticalBar", "\xE2\x89\x82"}, // U+2242 similar operator
        {"divideontimes", "\xE2\x8B\x87"}, // U+22C7 division times
        {"dotplus", "\xE2\x88\x94"},   // U+2214 plus sign with dot above
        {"div", "\xC3\xB7"},           // U+00F7 division sign

        // === Arrow symbol variants ===
        {"nlarr", "\xE2\x86\x9B"},     // U+219B leftwards arrow with stroke
        {"nrarr", "\xE2\x86\x9C"},     // U+219C rightwards arrow with stroke
        {"nlArr", "\xE2\x87\x8D"},     // U+21CD leftwards double arrow with stroke
        {"nrArr", "\xE2\x87\x8E"},     // U+21CE rightwards double arrow with stroke
        {"nleftrightarrow", "\xE2\x86\xAE"}, // U+21AE left right arrow with stroke
        {"nharr", "\xE2\x86\xAE"},     // U+21AE left right arrow with stroke
        {"UpDownArrow", "\xE2\x86\x95"}, // U+2195 up down arrow
        {"UpArrow", "\xE2\x86\x91"},   // U+2191 upwards arrow
        {"DownArrow", "\xE2\x86\x93"}, // U+2193 downwards arrow
        {"LeftArrow", "\xE2\x86\x90"}, // U+2190 leftwards arrow
        {"RightArrow", "\xE2\x86\x92"}, // U+2192 rightwards arrow
        {"lharu", "\xE2\x87\xBD"},     // U+21BD leftwards harpoon with barb upwards
        {"lharr", "\xE2\x87\xBD"},     // U+21BD leftwards harpoon with barb upwards
        {"rharu", "\xE2\x87\xC0"},     // U+21C0 rightwards harpoon with barb upwards
        {"rharr", "\xE2\x87\xC0"},     // U+21C0 rightwards harpoon with barb upwards
        {"lhard", "\xE2\x87\xBC"},     // U+21BC leftwards harpoon with barb downwards
        {"rhard", "\xE2\x87\xC1"},     // U+21C1 rightwards harpoon with barb downwards
        {"curvearrowright", "\xE2\x86\xB7"}, // U+21B7 top right curved arrow
        {"curvearrowleft", "\xE2\x86\xB6"},  // U+21B6 top left curved arrow
        {"circlearrowleft", "\xE2\x86\xBA"}, // U+21BA leftwards open circle arrow
        {"circlearrowright", "\xE2\x86\xBB"}, // U+21BB rightwards open circle arrow

        // === Greek letter variants ===
        {"vartheta", "\xCF\x91"},      // U+03D1 greek small letter theta symbol
        {"varpi", "\xCF\x96"},         // U+03D6 greek pi symbol (variant)
        {"varsigma", "\xCF\x82"},      // U+03C2 greek small letter final sigma
        {"varphi", "\xCF\x95"},        // U+03D5 greek small letter phi symbol
        {"varrho", "\xCF\xA1"},        // U+03F1 greek small letter rho symbol
        {"varepsilon", "\xCF\xB5"},    // U+03F5 greek small letter epsilon symbol

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

        // === Additional Mathematical Operators & Technical Symbols (200+ new entities) ===
        // Mathematical operators & geometric shapes
        {"bkarow", "\xE2\x87\xB0"},   // U+21F0 leftwards barbed arrow
        {"bowtie", "\xE2\x8B\x88"},   // U+22C8 bowtie
        {"boxminus", "\xE2\x8A\x9F"}, // U+229F squared minus
        {"boxplus", "\xE2\x8A\x9E"},  // U+229E squared plus
        {"boxtimes", "\xE2\x8A\xA0"}, // U+2A00 squared times
        // Box drawing symbols
        {"boxdl", "\xE2\x94\x93"},    // U+2513 box light vertical and left
        {"boxdr", "\xE2\x94\x8F"},    // U+252F box light vertical and right
        {"boxul", "\xE2\x94\x99"},    // U+2519 box light vertical and right
        {"boxur", "\xE2\x94\x95"},    // U+2515 box light vertical and left
        {"boxH", "\xE2\x94\x90"},     // U+2550 box double horizontal
        {"boxh", "\xE2\x94\x80"},     // U+2500 box light horizontal
        {"boxV", "\xE2\x94\x91"},     // U+2551 box double vertical
        {"boxv", "\xE2\x94\x82"},     // U+2502 box light vertical
        {"boxvh", "\xE2\x94\xBC"},    // U+253C box light vertical and horizontal
        // Integrals and calculus
        {"contourintegral", "\xE2\x88\xAE"}, // U+222E contour integral
        {"coprod", "\xE2\x88\x90"},   // U+2210 n-ary coproduct
        {"iiint", "\xE2\x88\x8C"},    // U+222C triple integral
        {"iiiint", "\xE2\xA7\x8C"},   // U+2A0C quadruple integral
        // Logical and relational operators
        {"becaus", "\xE2\x88\xB5"},   // U+2235 because
        {"between", "\xE2\x89\xB9"},  // U+2279 between
        {"eqcirc", "\xE2\x89\x96"},   // U+2256 ring in equal to
        {"eqcolon", "\xE2\x89\x95"},  // U+2255 equals colon
        {"eqsim", "\xE2\x89\x82"},    // U+2242 sine curve
        {"eqslantgtr", "\xE2\x89\xB5"},// U+2275 greater-than or slanted equal to
        {"eqslantless", "\xE2\x89\xB4"},// U+2274 less-than or slanted equal to
        {"ExponentialE", "\xE2\x85\x87"},// U+2147 exponential e
        // Greater than variants
        {"gneq", "\xE2\x89\xA9"},     // U+2269 greater-than and not equal to
        {"ges", "\xE2\x89\xB3"},      // U+2273 greater-than or equivalent to
        {"GreaterFullEqual", "\xE2\x89\xA7"},// U+2267 greater-than or slanted equal to
        {"GreaterGreater", "\xE2\x89\xAB"},// U+226B much greater-than
        // Letterlike symbols
        {"gscr", "\xE2\x84\x9E"},     // U+210E script g
        {"gimel", "\xE2\x84\xB7"},    // U+2137 gimel symbol
        // Arrow variants
        {"hookleftarrow", "\xE2\x86\xA9"},// U+21A9 leftwards arrow with hook
        {"hookrightarrow", "\xE2\x86\xAA"},// U+21AA rightwards arrow with hook
        {"harrcir", "\xE2\x86\xA8"},  // U+21A8 left right arrow with circle
        {"harrw", "\xE2\x86\xAD"},    // U+21AD left right wave arrow
        // Less than variants
        {"lne", "\xE2\x89\xA8"},      // U+2268 less-than and not equal to
        {"lesdot", "\xE2\x8A\x9F"},   // U+229F less-than with dot
        {"LessFullEqual", "\xE2\x89\xA6"},// U+2266 less-than or slanted equal to
        {"LessGreater", "\xE2\x89\xB7"},// U+2277 greater-than or less-than
        {"LessLess", "\xE2\x89\xAA"}, // U+226A much less-than
        // Corners and brackets
        {"llcorner", "\xE2\x8C\x9C"}, // U+231C upper left corner
        {"lrcorner", "\xE2\x8C\x9D"}, // U+231D upper right corner
        {"ulcorner", "\xE2\x8C\x9E"}, // U+231E lower left corner
        {"urcorner", "\xE2\x8C\x9F"}, // U+231F lower right corner
        // Vulgar fractions
        {"frac13", "\xE2\x85\x93"},   // U+2153 vulgar fraction one third
        {"frac15", "\xE2\x85\x95"},   // U+2155 vulgar fraction one fifth
        {"frac16", "\xE2\x85\x99"},   // U+2159 vulgar fraction one sixth
        {"frac18", "\xE2\x85\x98"},   // U+2158 vulgar fraction one eighth
        {"frac23", "\xE2\x85\x94"},   // U+2154 vulgar fraction two thirds
        {"frac25", "\xE2\x85\x96"},   // U+2156 vulgar fraction two fifths
        {"frac35", "\xE2\x85\x97"},   // U+2157 vulgar fraction three fifths
        {"frac38", "\xE2\x85\x9B"},   // U+215B vulgar fraction three eighths
        {"frac56", "\xE2\x85\x9A"},   // U+215A vulgar fraction five sixths
        {"frac58", "\xE2\x85\x9D"},   // U+215D vulgar fraction five eighths
        {"frac78", "\xE2\x85\x9E"},   // U+215E vulgar fraction seven eighths
        // Entailment and turnstiles
        {"models", "\xE2\x8A\xA8"},   // U+22A8 models (entailment)
        {"intcal", "\xE2\x8A\xBA"},   // U+22BA intercalate
        // Triangles and geometric shapes
        {"lltri", "\xE2\x97\x80"},    // U+25C0 black left-pointing triangle
        {"rrtri", "\xE2\x96\xB6"},    // U+25B6 black right-pointing triangle
        {"uutri", "\xE2\x97\x82"},    // U+25C2 black up-pointing triangle
        {"ddtri", "\xE2\x97\x83"},    // U+25C3 black down-pointing triangle
        // Additional symbols from Unicode math alphanumeric range
        {"bopf", "\xF0\x9D\x95\x93"}, // U+1D553 double-struck b
        {"Bopf", "\xF0\x9D\x94\xB9"}, // U+1D4B9 double-struck B
        {"copf", "\xF0\x9D\x95\x94"}, // U+1D554 double-struck c
        {"Copf", "\xE2\x84\x82"},     // U+2102 double-struck C
        {"dopf", "\xF0\x9D\x95\x95"}, // U+1D555 double-struck d
        {"Dopf", "\xF0\x9D\x94\xBB"}, // U+1D4BB double-struck D
        {"fopf", "\xF0\x9D\x95\x99"}, // U+1D559 double-struck f
        {"Fopf", "\xF0\x9D\x94\xBF"}, // U+1D4BF double-struck F
        {"gopf", "\xF0\x9D\x95\x9A"}, // U+1D55A double-struck g
        {"Gopf", "\xF0\x9D\x94\xC1"}, // U+1D4C1 double-struck G
        {"hopf", "\xF0\x9D\x95\x9B"}, // U+1D55B double-struck h
        {"Hopf", "\xE2\x84\x8B"},     // U+210B script H
        {"iopf", "\xF0\x9D\x95\x9C"}, // U+1D55C double-struck i
        {"Iopf", "\xF0\x9D\x94\xC6"}, // U+1D4C6 double-struck I
        {"jopf", "\xF0\x9D\x95\x9D"}, // U+1D55D double-struck j
        {"Jopf", "\xF0\x9D\x94\xC7"}, // U+1D4C7 double-struck J
        {"kopf", "\xF0\x9D\x95\x9E"}, // U+1D55E double-struck k
        {"Kopf", "\xF0\x9D\x94\xC8"}, // U+1D4C8 double-struck K
        {"lopf", "\xF0\x9D\x95\x9F"}, // U+1D55F double-struck l
        {"Lopf", "\xF0\x9D\x94\xC9"}, // U+1D4C9 double-struck L
        {"mopf", "\xF0\x9D\x95\xA0"}, // U+1D560 double-struck m
        {"Mopf", "\xF0\x9D\x94\xCA"}, // U+1D4CA double-struck M
        {"nopf", "\xF0\x9D\x95\xA1"}, // U+1D561 double-struck n
        {"Nopf", "\xE2\x84\x95"},     // U+2115 double-struck N
        {"oopf", "\xF0\x9D\x95\xA2"}, // U+1D562 double-struck o
        {"Oopf", "\xF0\x9D\x94\xD0"}, // U+1D4D0 double-struck O
        {"popf", "\xF0\x9D\x95\xA3"}, // U+1D563 double-struck p
        {"Popf", "\xE2\x84\x99"},     // U+2119 double-struck P
        {"qopf", "\xF0\x9D\x95\xA4"}, // U+1D564 double-struck q
        {"Qopf", "\xF0\x9D\x94\xD3"}, // U+1D4D3 double-struck Q
        {"ropf", "\xF0\x9D\x95\xA5"}, // U+1D565 double-struck r
        {"Ropf", "\xE2\x84\x9D"},     // U+211D double-struck R
        {"sopf", "\xF0\x9D\x95\xA6"}, // U+1D566 double-struck s
        {"Sopf", "\xF0\x9D\x94\xD8"}, // U+1D4D8 double-struck S
        {"topf", "\xF0\x9D\x95\xA7"}, // U+1D567 double-struck t
        {"Topf", "\xF0\x9D\x94\xD9"}, // U+1D4D9 double-struck T
        {"uopf", "\xF0\x9D\x95\xA8"}, // U+1D568 double-struck u
        {"Uopf", "\xF0\x9D\x94\xDA"}, // U+1D4DA double-struck U
        {"vopf", "\xF0\x9D\x95\xA9"}, // U+1D569 double-struck v
        {"Vopf", "\xF0\x9D\x94\xDB"}, // U+1D4DB double-struck V
        {"wopf", "\xF0\x9D\x95\xAA"}, // U+1D56A double-struck w
        {"Wopf", "\xF0\x9D\x94\xDC"}, // U+1D4DC double-struck W
        {"xopf", "\xF0\x9D\x95\xAB"}, // U+1D56B double-struck x
        {"Xopf", "\xF0\x9D\x94\xDD"}, // U+1D4DD double-struck X
        {"yopf", "\xF0\x9D\x95\xAC"}, // U+1D56C double-struck y
        {"Yopf", "\xF0\x9D\x94\xDE"}, // U+1D4DE double-struck Y
        {"zopf", "\xF0\x9D\x95\xAD"}, // U+1D56D double-struck z
        {"Zopf", "\xE2\x84\xA4"},     // U+2124 double-struck Z
        // Script letters
        {"ascr", "\xF0\x9D\x92\x9F"}, // U+1D49F script a
        {"Ascr", "\xF0\x9D\x92\x80"}, // U+1D480 script A
        {"bscr", "\xF0\x9D\x92\xA0"}, // U+1D4A0 script b
        {"Bscr", "\xE2\x84\xB3"},     // U+212C script B
        {"cscr", "\xF0\x9D\x92\xA1"}, // U+1D4A1 script c
        {"Cscr", "\xF0\x9D\x92\x82"}, // U+1D482 script C
        {"dscr", "\xF0\x9D\x92\xA2"}, // U+1D4A2 script d
        {"Dscr", "\xF0\x9D\x92\x83"}, // U+1D483 script D
        {"escr", "\xE2\x84\xB0"},     // U+212F script e
        {"Escr", "\xF0\x9D\x92\x84"}, // U+1D484 script E
        {"fscr", "\xF0\x9D\x92\xA5"}, // U+1D4A5 script f
        {"Fscr", "\xE2\x84\xB1"},     // U+2131 script F
        {"gscr", "\xE2\x84\x9E"},     // U+210E script g
        {"Gscr", "\xF0\x9D\x92\x86"}, // U+1D486 script G
        // More symbol variants for completeness
        {"hscr", "\xF0\x9D\x92\xA9"}, // U+1D4A9 script h
        {"Hscr", "\xE2\x84\x8B"},     // U+210B script H
        {"iscr", "\xF0\x9D\x92\xAA"}, // U+1D4AA script i
        {"Iscr", "\xF0\x9D\x92\x88"}, // U+1D488 script I
        {"jscr", "\xF0\x9D\x92\xAB"}, // U+1D4AB script j
        {"Jscr", "\xF0\x9D\x92\x89"}, // U+1D489 script J
        {"kscr", "\xF0\x9D\x92\xAC"}, // U+1D4AC script k
        {"Kscr", "\xF0\x9D\x92\x8A"}, // U+1D48A script K
        {"lscr", "\xF0\x9D\x92\xAD"}, // U+1D4AD script l
        {"Lscr", "\xE2\x84\x92"},     // U+2112 script L
        {"mscr", "\xF0\x9D\x92\xAE"}, // U+1D4AE script m
        {"Mscr", "\xF0\x9D\x92\x8C"}, // U+1D48C script M
        {"nscr", "\xF0\x9D\x92\xAF"}, // U+1D4AF script n
        {"Nscr", "\xF0\x9D\x92\x8D"}, // U+1D48D script N
        {"oscr", "\xF0\x9D\x92\xB0"}, // U+1D4B0 script o
        {"Oscr", "\xF0\x9D\x92\x8E"}, // U+1D48E script O
        {"pscr", "\xF0\x9D\x92\xB1"}, // U+1D4B1 script p
        {"Pscr", "\xF0\x9D\x92\x8F"}, // U+1D48F script P
        {"qscr", "\xF0\x9D\x92\xB2"}, // U+1D4B2 script q
        {"Qscr", "\xF0\x9D\x92\x90"}, // U+1D490 script Q
        {"rscr", "\xF0\x9D\x92\xB3"}, // U+1D4B3 script r
        {"Rscr", "\xE2\x84\x9B"},     // U+211B script R
        {"sscr", "\xF0\x9D\x92\xB4"}, // U+1D4B4 script s
        {"Sscr", "\xF0\x9D\x92\x92"}, // U+1D492 script S
        {"tscr", "\xF0\x9D\x92\xB5"}, // U+1D4B5 script t
        {"Tscr", "\xF0\x9D\x92\x93"}, // U+1D493 script T
        {"uscr", "\xF0\x9D\x92\xB6"}, // U+1D4B6 script u
        {"Uscr", "\xF0\x9D\x92\x94"}, // U+1D494 script U
        {"vscr", "\xF0\x9D\x92\xB7"}, // U+1D4B7 script v
        {"Vscr", "\xF0\x9D\x92\x95"}, // U+1D495 script V
        {"wscr", "\xF0\x9D\x92\xB8"}, // U+1D4B8 script w
        {"Wscr", "\xF0\x9D\x92\x96"}, // U+1D496 script W
        {"xscr", "\xF0\x9D\x92\xB9"}, // U+1D4B9 script x
        {"Xscr", "\xF0\x9D\x92\x97"}, // U+1D497 script X
        {"yscr", "\xF0\x9D\x92\xBA"}, // U+1D4BA script y
        {"Yscr", "\xF0\x9D\x92\x98"}, // U+1D498 script Y
        {"zscr", "\xF0\x9D\x92\xBB"}, // U+1D4BB script z
        {"Zscr", "\xF0\x9D\x92\x99"}, // U+1D499 script Z
        // Fraktur letters for completeness
        {"afr", "\xF0\x9D\x94\x9E"},  // U+1D51E Fraktur a
        {"Afr", "\xF0\x9D\x94\x84"},  // U+1D504 Fraktur A
        {"bfr", "\xF0\x9D\x94\x9F"},  // U+1D51F Fraktur b
        {"Bfr", "\xF0\x9D\x94\x85"},  // U+1D505 Fraktur B
        {"cfr", "\xF0\x9D\x94\xA0"},  // U+1D520 Fraktur c
        {"Cfr", "\xF0\x9D\x94\x86"},  // U+1D506 Fraktur C
        {"dfr", "\xF0\x9D\x94\xA1"},  // U+1D521 Fraktur d
        {"Dfr", "\xF0\x9D\x94\x87"},  // U+1D507 Fraktur D
        {"efr", "\xF0\x9D\x94\xA2"},  // U+1D522 Fraktur e
        {"Efr", "\xF0\x9D\x94\x88"},  // U+1D508 Fraktur E
        {"ffr", "\xF0\x9D\x94\xA3"},  // U+1D523 Fraktur f
        {"Ffr", "\xF0\x9D\x94\x89"},  // U+1D509 Fraktur F
        {"gfr", "\xF0\x9D\x94\xA4"},  // U+1D524 Fraktur g
        {"Gfr", "\xF0\x9D\x94\x8A"},  // U+1D50A Fraktur G
        {"hfr", "\xF0\x9D\x94\xA5"},  // U+1D525 Fraktur h
        {"Hfr", "\xF0\x9D\x94\x8B"},  // U+1D50B Fraktur H
        {"ifr", "\xF0\x9D\x94\xA6"},  // U+1D526 Fraktur i
        {"Ifr", "\xF0\x9D\x94\x8C"},  // U+1D50C Fraktur I
        {"jfr", "\xF0\x9D\x94\xA7"},  // U+1D527 Fraktur j
        {"Jfr", "\xF0\x9D\x94\x8D"},  // U+1D50D Fraktur J
        {"kfr", "\xF0\x9D\x94\xA8"},  // U+1D528 Fraktur k
        {"Kfr", "\xF0\x9D\x94\x8E"},  // U+1D50E Fraktur K
        {"lfr", "\xF0\x9D\x94\xA9"},  // U+1D529 Fraktur l
        {"Lfr", "\xF0\x9D\x94\x8F"},  // U+1D50F Fraktur L
        {"mfr", "\xF0\x9D\x94\xAA"},  // U+1D52A Fraktur m
        {"Mfr", "\xF0\x9D\x94\x90"},  // U+1D510 Fraktur M
        {"nfr", "\xF0\x9D\x94\xAB"},  // U+1D52B Fraktur n
        {"Nfr", "\xF0\x9D\x94\x91"},  // U+1D511 Fraktur N
        {"ofr", "\xF0\x9D\x94\xAC"},  // U+1D52C Fraktur o
        {"Ofr", "\xF0\x9D\x94\x92"},  // U+1D512 Fraktur O
        {"pfr", "\xF0\x9D\x94\xAD"},  // U+1D52D Fraktur p
        {"Pfr", "\xF0\x9D\x94\x93"},  // U+1D513 Fraktur P
        {"qfr", "\xF0\x9D\x94\xAE"},  // U+1D52E Fraktur q
        {"Qfr", "\xF0\x9D\x94\x94"},  // U+1D514 Fraktur Q
        {"rfr", "\xF0\x9D\x94\xAF"},  // U+1D52F Fraktur r
        {"Rfr", "\xF0\x9D\x94\x95"},  // U+1D515 Fraktur R
        {"sfr", "\xF0\x9D\x94\xB0"},  // U+1D530 Fraktur s
        {"Sfr", "\xF0\x9D\x94\x96"},  // U+1D516 Fraktur S
        {"tfr", "\xF0\x9D\x94\xB1"},  // U+1D531 Fraktur t
        {"Tfr", "\xF0\x9D\x94\x97"},  // U+1D517 Fraktur T
        {"ufr", "\xF0\x9D\x94\xB2"},  // U+1D532 Fraktur u
        {"Ufr", "\xF0\x9D\x94\x98"},  // U+1D518 Fraktur U
        {"vfr", "\xF0\x9D\x94\xB3"},  // U+1D533 Fraktur v
        {"Vfr", "\xF0\x9D\x94\x99"},  // U+1D519 Fraktur V
        {"wfr", "\xF0\x9D\x94\xB4"},  // U+1D534 Fraktur w
        {"Wfr", "\xF0\x9D\x94\x9A"},  // U+1D51A Fraktur W
        {"xfr", "\xF0\x9D\x94\xB5"},  // U+1D535 Fraktur x
        {"Xfr", "\xF0\x9D\x94\x9B"},  // U+1D51B Fraktur X
        {"yfr", "\xF0\x9D\x94\xB6"},  // U+1D536 Fraktur y
        {"Yfr", "\xF0\x9D\x94\x9C"},  // U+1D51C Fraktur Y
        {"zfr", "\xF0\x9D\x94\xB7"},  // U+1D537 Fraktur z
        {"Zfr", "\xE2\x84\x98"},      // U+2128 Fraktur Z

        // === ASCII Punctuation and Symbols (U+0021-U+007E) ===
        {"excl", "!"},                // U+0021 exclamation mark
        {"num", "#"},                 // U+0023 number sign
        {"dollar", "$"},              // U+0024 dollar sign
        {"percnt", "%"},              // U+0025 percent sign
        {"lpar", "("},                // U+0028 left parenthesis
        {"rpar", ")"},                // U+0029 right parenthesis
        {"ast", "*"},                 // U+002A asterisk
        {"plus", "+"},                // U+002B plus sign
        {"comma", ","},               // U+002C comma
        {"period", "."},              // U+002E full stop
        {"sol", "/"},                 // U+002F solidus (forward slash)
        {"colon", ":"},               // U+003A colon
        {"semi", ";"},                // U+003B semicolon
        {"quest", "?"},               // U+003F question mark
        {"commat", "@"},              // U+0040 commercial at sign
        {"lsqb", "["},                // U+005B left square bracket
        {"rsqb", "]"},                // U+005D right square bracket
        {"lcub", "{"},                // U+007B left curly bracket
        {"rcub", "}"},                // U+007D right curly bracket
        {"verbar", "|"},              // U+007C vertical bar

        // === Symbol Aliases ===
        {"VerticalBar", "|"},         // U+007C vertical bar (alias)
        {"thinspace", "\xE2\x80\x89"}, // U+2009 thin space (alias for thinsp)
        {"enspace", "\xE2\x80\x82"},  // U+2002 en space (alias for ensp)
        {"emspace", "\xE2\x80\x83"},  // U+2003 em space (alias for emsp)
        {"NoBreakSpace", "\xC2\xA0"}, // U+00A0 non-breaking space (alias for nbsp)
        {"NBSP", "\xC2\xA0"},         // U+00A0 non-breaking space (uppercase alias)

        // === Additional Temperature Symbols ===
        {"celsius", "\xE2\x84\x83"},  // U+2103 degree Celsius
        {"fahrenheit", "\xE2\x84\x89"}, // U+2109 degree Fahrenheit

        // === Additional Greek Letters and Variants ===
        {"digamma", "\xCF\x9C"},      // U+03DC Greek letter digamma
        {"koppa", "\xCF\x9A"},        // U+03DA Greek letter koppa

        // === Additional Arrows ===
        {"swarrow", "\xE2\x86\x99"},  // U+2199 southwest arrow
        {"nwarrow", "\xE2\x86\x96"},  // U+2196 northwest arrow
        {"searrow", "\xE2\x86\x98"},  // U+2198 southeast arrow
        {"nearrow", "\xE2\x86\x97"},  // U+2197 northeast arrow

        // === Additional Bracket and Delimiter Symbols ===
        {"lbrace", "{"},              // U+007B left curly bracket (alias for lcub)
        {"rbrace", "}"},              // U+007D right curly bracket (alias for rcub)
        {"DoubleLeftArrow", "\xE2\x87\x90"}, // U+21D0 leftwards double arrow (alias for lArr)
        {"DoubleRightArrow", "\xE2\x87\x92"}, // U+21D2 rightwards double arrow (alias for rArr)
        {"DoubleUpArrow", "\xE2\x87\x91"},    // U+21D1 upwards double arrow (alias for uArr)
        {"DoubleDownArrow", "\xE2\x87\x93"},  // U+21D3 downwards double arrow (alias for dArr)

        // === Additional Logical and Relational Operators ===
        {"equivalent", "\xE2\x89\xA1"}, // U+2261 identical to (alias for equiv)
        {"implies", "\xE2\x87\x92"},    // U+21D2 rightwards double arrow (alias for rArr)
        {"notequal", "\xE2\x89\xA0"},   // U+2260 not equal to (alias for ne)
        {"approximately", "\xE2\x89\x88"}, // U+2248 almost equal to (alias for asymp)

        // === Additional Diacritical Marks ===
        {"caron", "\xCB\x87"},        // U+02C7 caron
        {"breve", "\xCB\x98"},        // U+02D8 breve
        {"overline", "\xE2\x80\xBE"},  // U+203E overline
        {"dot", "\xCB\x99"},          // U+02D9 dot above

        // === Additional Typography Aliases ===
        {"pilcrow", "\xC2\xB6"},       // U+00B6 pilcrow (alias for para)
        {"section", "\xC2\xA7"},       // U+00A7 section sign (alias for sect)
        {"doubledagger", "\xE2\x80\xA1"}, // U+2021 double dagger (alias for Dagger)
        {"bullet", "\xE2\x80\xA2"},    // U+2022 bullet (alias for bull)
        {"check", "\xE2\x9C\x93"},     // U+2713 check mark
        {"ellipsis", "\xE2\x80\xA6"},  // U+2026 horizontal ellipsis (alias for hellip)
        {"mldr", "\xE2\x80\xA6"},      // U+2026 horizontal ellipsis

        // === Additional Currency Symbols ===
        {"ruble", "\xE2\x82\xBD"},     // U+20BD ruble sign

        // === Additional Mathematical Symbols ===
        {"mnplus", "\xE2\x88\x93"},    // U+2213 minus-or-plus sign

        // === Additional Arrow Symbols ===
        {"varr", "\xE2\x86\x95"},      // U+2195 up down arrow

        // === Extended Latin: Combining Diacritical Marks and Extensions ===
        {"abreve", "\xC4\x83"},   // U+0103 a with breve
        {"Abreve", "\xC4\x82"},   // U+0102 A with breve
        {"aogonek", "\xC4\x99"},  // U+0105 a with ogonek
        {"Aogonek", "\xC4\x98"},  // U+0104 A with ogonek
        {"breve", "\xCB\x98"},    // U+02D8 breve
        {"caron", "\xCB\x87"},    // U+02C7 caron
        {"commat", "@"},          // U+0040 commercial at
        {"dieresis", "\xC2\xA8"}, // U+00A8 diaeresis
        {"dot", "\xCB\x99"},      // U+02D9 dot above
        {"dotaccent", "\xCB\x99"},// U+02D9 dot above
        {"ebreve", "\xC4\x97"},   // U+0117 e with breve
        {"Ebreve", "\xC4\x96"},   // U+0116 E with breve
        {"edot", "\xC4\x97"},     // U+0117 e with dot above
        {"Edot", "\xC4\x96"},     // U+0116 E with dot above
        {"emacron", "\xC4\x93"},  // U+0113 e with macron
        {"Emacron", "\xC4\x92"},  // U+0112 E with macron
        {"eng", "\xC5\x8B"},      // U+014B eng
        {"ENG", "\xC5\x8A"},      // U+014A ENG
        {"eogon", "\xC4\x99"},    // U+0119 e with ogonek
        {"Eogon", "\xC4\x98"},    // U+0118 E with ogonek
        {"gacute", "\xC4\x95"},   // U+0107 g with acute
        {"Gacute", "\xC4\x94"},   // U+0106 G with acute
        {"gbreve", "\xC4\x9F"},   // U+011F g with breve
        {"Gbreve", "\xC4\x9E"},   // U+011E G with breve
        {"gcirc", "\xC4\x9D"},    // U+011D g with circumflex
        {"Gcirc", "\xC4\x9C"},    // U+011C G with circumflex
        {"gdot", "\xC4\xA1"},     // U+0121 g with dot above
        {"Gdot", "\xC4\xA0"},     // U+0120 G with dot above
        {"grave", "`"},           // U+0060 grave accent
        {"hcirc", "\xC4\xA5"},    // U+0125 h with circumflex
        {"Hcirc", "\xC4\xA4"},    // U+0124 H with circumflex
        {"idot", "\xC4\xB1"},     // U+0131 dotless i
        {"Idotaccent", "\xC4\xB0"},// U+0130 I with dot above
        {"imacron", "\xC4\xAB"},  // U+012B i with macron
        {"Imacron", "\xC4\xAA"},  // U+012A I with macron
        {"iogon", "\xC4\xAF"},    // U+012F i with ogonek
        {"Iogon", "\xC4\xAE"},    // U+012E I with ogonek
        {"itilde", "\xC4\xA9"},   // U+0129 i with tilde
        {"Itilde", "\xC4\xA8"},   // U+0128 I with tilde
        {"jcirc", "\xC4\xB5"},    // U+0135 j with circumflex
        {"Jcirc", "\xC4\xB4"},    // U+0134 J with circumflex
        {"kcirc", "\xC4\xB7"},    // U+0137 k with cedilla
        {"Kcirc", "\xC4\xB6"},    // U+0136 K with cedilla
        {"lacute", "\xC4\xBA"},   // U+013A l with acute
        {"Lacute", "\xC4\xB9"},   // U+0139 L with acute
        {"lcaron", "\xC4\xBE"},   // U+013E l with caron
        {"Lcaron", "\xC4\xBD"},   // U+013D L with caron
        {"lcedil", "\xC4\xBC"},   // U+013C l with cedilla
        {"Lcedil", "\xC4\xBB"},   // U+013B L with cedilla
        {"lcirc", "\xC4\xBB"},    // U+013B l with circumflex
        {"Lcirc", "\xC4\xBA"},    // U+013A L with circumflex
        {"ldot", "\xC5\x80"},     // U+0140 l with middle dot
        {"Ldot", "\xC4\xBF"},     // U+013F L with middle dot
        {"lmidot", "\xC5\x80"},   // U+0140 l with middle dot (alias)
        {"nacute", "\xC5\x84"},   // U+0144 n with acute
        {"Nacute", "\xC5\x83"},   // U+0143 N with acute
        {"ncaron", "\xC5\x88"},   // U+0148 n with caron
        {"Ncaron", "\xC5\x87"},   // U+0147 N with caron
        {"ncedil", "\xC5\x86"},   // U+0146 n with cedilla
        {"Ncedil", "\xC5\x85"},   // U+0145 N with cedilla
        {"ncirc", "\xC3\xB1"},    // U+00F1 n with tilde
        {"Ncirc", "\xC3\x91"},    // U+00D1 N with tilde
        {"obreve", "\xC5\x8F"},   // U+014F o with breve
        {"Obreve", "\xC5\x8E"},   // U+014E O with breve
        {"odblac", "\xC5\x91"},   // U+0151 o with double acute
        {"Odblac", "\xC5\x90"},   // U+0150 O with double acute
        {"odieresis", "\xC3\xB6"},// U+00F6 o with diaeresis
        {"Odieresis", "\xC3\x96"},// U+00D6 O with diaeresis
        {"ohorn", "\xC6\xA0"},    // U+01A0 o with horn
        {"Ohorn", "\xC6\x9F"},    // U+019F O with horn
        {"omacron", "\xC5\x8D"},  // U+014D o with macron
        {"Omacron", "\xC5\x8C"},  // U+014C O with macron
        {"oogonek", "\xC4\xA3"},  // U+0123 o with ogonek
        {"Oogonek", "\xC4\xA2"},  // U+0122 O with ogonek
        {"ovbar", "\xC2\xAF"},    // U+00AF overline
        {"oviron", "\xCB\x9A"},   // U+02DA ring above (alias)
        {"racute", "\xC5\x95"},   // U+0155 r with acute
        {"Racute", "\xC5\x94"},   // U+0154 R with acute
        {"rcaron", "\xC5\x99"},   // U+0159 r with caron
        {"Rcaron", "\xC5\x98"},   // U+0158 R with caron
        {"rcedil", "\xC5\x97"},   // U+0157 r with cedilla
        {"Rcedil", "\xC5\x96"},   // U+0156 R with cedilla
        {"rcirc", "\xC5\x95"},    // U+0155 r with circumflex
        {"Rcirc", "\xC5\x94"},    // U+0154 R with circumflex
        {"ring", "\xCB\x9A"},     // U+02DA ring above
        {"sacute", "\xC5\x9B"},   // U+015B s with acute
        {"Sacute", "\xC5\x9A"},   // U+015A S with acute
        {"scedil", "\xC5\x9F"},   // U+015F s with cedilla
        {"Scedil", "\xC5\x9E"},   // U+015E S with cedilla
        {"scirc", "\xC5\x9D"},    // U+015D s with circumflex
        {"Scirc", "\xC5\x9C"},    // U+015C S with circumflex
        {"slash", "/"},           // U+002F solidus
        {"tcaron", "\xC5\xA5"},   // U+0165 t with caron
        {"Tcaron", "\xC5\xA4"},   // U+0164 T with caron
        {"tcedil", "\xC5\xA3"},   // U+0163 t with cedilla
        {"Tcedil", "\xC5\xA2"},   // U+0162 T with cedilla
        {"tcirc", "\xC5\xA3"},    // U+0163 t with circumflex (alias)
        {"Tcirc", "\xC5\xA2"},    // U+0162 T with circumflex (alias)
        {"tdot", "\xC5\xA1"},     // U+0161 t with dot above (alias)
        {"ubreve", "\xC5\xAD"},   // U+016D u with breve
        {"Ubreve", "\xC5\xAC"},   // U+016C U with breve
        {"udblac", "\xC5\xB1"},   // U+0171 u with double acute
        {"Udblac", "\xC5\xB0"},   // U+0170 U with double acute
        {"udiag", "\xC2\xA8"},    // U+00A8 umlaut (alias for diaeresis)
        {"ugrave", "\xC3\xB9"},   // U+00F9 u with grave
        {"Ugrave", "\xC3\x99"},   // U+00D9 U with grave
        {"uhorn", "\xC6\xA1"},    // U+01A1 u with horn
        {"Uhorn", "\xC6\xa0"},    // U+01A0 U with horn
        {"umacron", "\xC5\xAB"},  // U+016B u with macron
        {"Umacron", "\xC5\xAA"},  // U+016A U with macron
        {"uogonek", "\xC5\xB3"},  // U+0173 u with ogonek
        {"Uogonek", "\xC5\xB2"},  // U+0172 U with ogonek
        {"uring", "\xC5\xB5"},    // U+0175 u with ring above
        {"Uring", "\xC5\xB4"},    // U+0174 U with ring above
        {"utilde", "\xC5\xA9"},   // U+0169 u with tilde
        {"Utilde", "\xC5\xA8"},   // U+0168 U with tilde
        {"wcirc", "\xC5\xB7"},    // U+0177 w with circumflex
        {"Wcirc", "\xC5\xB6"},    // U+0176 W with circumflex
        {"yacute", "\xC3\xBD"},   // U+00FD y with acute
        {"Yacute", "\xC3\x9D"},   // U+00DD Y with acute
        {"ycirc", "\xC5\xB7"},    // U+0177 y with circumflex
        {"Ycirc", "\xC5\xB6"},    // U+0176 Y with circumflex
        {"ydieresis", "\xC3\xBF"},// U+00FF y with diaeresis
        {"zcirc", "\xCB\x9C"},    // U+02DC z with circumflex (not standard)
        {"zcaron", "\xC5\xBE"},   // U+017E z with caron
        {"Zcaron", "\xC5\xBD"},   // U+017D Z with caron
        {"zdot", "\xC5\xBC"},     // U+017C z with dot above
        {"Zdot", "\xC5\xBB"},     // U+017B Z with dot above

        // === Typographic Symbols and Marks ===
        {"smile", "\xE2\x8C\xA3"}, // U+2323 smiling face
        {"frown", "\xE2\x8C\xA2"}, // U+2322 frowning face
        {"checkmark", "\xE2\x9C\x93"}, // U+2713 check mark
        {"cross", "\xE2\x9C\x97"},     // U+2717 ballot X
        {"xmark", "\xE2\x9C\x97"},     // U+2717 ballot X (alias)
        {"halfnote", "\xE2\x99\x99"},  // U+2669 quarter note
        {"note", "\xE2\x99\xA9"},      // U+266A eighth note
        {"heartsuit", "\xE2\x99\xA5"}, // U+2665 heart suit
        {"spadesuit", "\xE2\x99\xA0"}, // U+2660 spade suit
        {"diamondsuit", "\xE2\x99\xA6"}, // U+2666 diamond suit
        {"clubsuit", "\xE2\x99\xA3"},  // U+2663 club suit
        {"fleur", "\xE2\x9C\xB3"},     // U+2713 fleuron
        {"dagger2", "\xE2\x80\xA1"},   // U+2021 double dagger (alias)
        {"bullet2", "\xE2\x80\xA2"},   // U+2022 bullet (alias)

        // === Superscripts and Subscripts ===
        {"sup0", "\xE2\x81\xB0"},  // U+2070 superscript zero
        {"sup4", "\xE2\x81\xB4"},  // U+2074 superscript four
        {"sup5", "\xE2\x81\xB5"},  // U+2075 superscript five
        {"sup6", "\xE2\x81\xB6"},  // U+2076 superscript six
        {"sup7", "\xE2\x81\xB7"},  // U+2077 superscript seven
        {"sup8", "\xE2\x81\xB8"},  // U+2078 superscript eight
        {"sup9", "\xE2\x81\xB9"},  // U+2079 superscript nine
        {"supminus", "\xE2\x81\xBB"}, // U+207B superscript minus
        {"supplus", "\xE2\x81\xBA"},  // U+207A superscript plus
        {"supeq", "\xE2\x81\xBD"},    // U+207D superscript equals
        {"sub0", "\xE2\x82\x80"},  // U+2080 subscript zero
        {"sub1", "\xE2\x82\x81"},  // U+2081 subscript one
        {"sub2", "\xE2\x82\x82"},  // U+2082 subscript two
        {"sub3", "\xE2\x82\x83"},  // U+2083 subscript three
        {"sub4", "\xE2\x82\x84"},  // U+2084 subscript four
        {"sub5", "\xE2\x82\x85"},  // U+2085 subscript five
        {"sub6", "\xE2\x82\x86"},  // U+2086 subscript six
        {"sub7", "\xE2\x82\x87"},  // U+2087 subscript seven
        {"sub8", "\xE2\x82\x88"},  // U+2088 subscript eight
        {"sub9", "\xE2\x82\x89"},  // U+2089 subscript nine
        {"subminus", "\xE2\x82\x8B"}, // U+208B subscript minus
        {"subplus", "\xE2\x82\x8A"},  // U+208A subscript plus
        {"subeq", "\xE2\x82\x8C"},    // U+208C subscript equals

        // === Fractions and Other Numeric Forms ===
        {"frac13", "\xE2\x85\x93"},    // U+2153 vulgar fraction one third
        {"frac23", "\xE2\x85\x94"},    // U+2154 vulgar fraction two thirds
        {"frac15", "\xE2\x85\x95"},    // U+2155 vulgar fraction one fifth
        {"frac25", "\xE2\x85\x96"},    // U+2156 vulgar fraction two fifths
        {"frac35", "\xE2\x85\x97"},    // U+2157 vulgar fraction three fifths
        {"frac45", "\xE2\x85\x98"},    // U+2158 vulgar fraction four fifths
        {"frac16", "\xE2\x85\x99"},    // U+2159 vulgar fraction one sixth
        {"frac56", "\xE2\x85\x9A"},    // U+215A vulgar fraction five sixths
        {"frac18", "\xE2\x85\x9B"},    // U+215B vulgar fraction one eighth
        {"frac38", "\xE2\x85\x9C"},    // U+215C vulgar fraction three eighths
        {"frac58", "\xE2\x85\x9D"},    // U+215D vulgar fraction five eighths
        {"frac78", "\xE2\x85\x9E"},    // U+215E vulgar fraction seven eighths

        // === Box Drawing and Geometric Shapes ===
        {"boxdr", "\xE2\x94\x8C"},    // U+250C box drawings light down and right
        {"boxdl", "\xE2\x94\x90"},    // U+2510 box drawings light down and left
        {"boxur", "\xE2\x94\x94"},    // U+2514 box drawings light up and right
        {"boxul", "\xE2\x94\x98"},    // U+2518 box drawings light up and left
        {"boxh", "\xE2\x94\x80"},     // U+2500 box drawings light horizontal
        {"boxv", "\xE2\x94\x82"},     // U+2502 box drawings light vertical
        {"boxh2", "\xE2\x95\x90"},    // U+2550 box drawings double horizontal
        {"boxv2", "\xE2\x95\x91"},    // U+2551 box drawings double vertical
        {"boxcross", "\xE2\x94\xBC"}, // U+253C box drawings light vertical and horizontal
        {"boxdrcross", "\xE2\x95\xA0"}, // U+2560 box drawings vertical light and right (alias)
        {"blk12", "\xE2\x96\x89"},    // U+2589 block, left three quarters
        {"blk34", "\xE2\x96\x8A"},    // U+258A block, left one quarter

        // === Card/Chess Symbols ===
        {"whiteking", "\xE2\x99\x94"},   // U+2654 white chess king
        {"whitequeen", "\xE2\x99\x95"},  // U+2655 white chess queen
        {"whiterook", "\xE2\x99\x96"},   // U+2656 white chess rook
        {"whitebishop", "\xE2\x99\x97"}, // U+2657 white chess bishop
        {"whiteknight", "\xE2\x99\x98"}, // U+2658 white chess knight
        {"whitepawn", "\xE2\x99\x99"},   // U+2659 white chess pawn
        {"blackking", "\xE2\x99\x9A"},   // U+265A black chess king
        {"blackqueen", "\xE2\x99\x9B"},  // U+265B black chess queen
        {"blackrook", "\xE2\x99\x9C"},   // U+265C black chess rook
        {"blackbishop", "\xE2\x99\x9D"}, // U+265D black chess bishop
        {"blackknight", "\xE2\x99\x9E"}, // U+265E black chess knight
        {"blackpawn", "\xE2\x99\x9F"},   // U+265F black chess pawn

        // === Weather and Seasonal Symbols ===
        {"sun", "\xE2\x98\x80"},     // U+2600 black sun with rays
        {"star", "\xE2\x98\x85"},    // U+2605 black star
        {"star2", "\xE2\x98\x86"},   // U+2606 white star
        {"cloud", "\xE2\x98\x81"},   // U+2601 cloud
        {"telephone", "\xE2\x98\x8E"}, // U+260E black telephone

        // === Miscellaneous Symbols and Operators ===
        {"pilcrow", "\xC2\xB6"},      // U+00B6 pilcrow (alias for para)
        {"section", "\xC2\xA7"},      // U+00A7 section sign (alias for sect)
        {"doubled", "\xE2\x81\x84"},  // U+2044 fraction slash (alias for frasl)
        {"inverse", "\xEF\xBF\xBD"},  // U+FFFD replacement character
        {"isdot", "\xE2\x9F\x99"},    // U+27F9 element of symbol with dot
        {"ratio", "\xE2\x88\xB6"},    // U+2236 ratio
        {"dotdot", "\xE2\x80\xA4"},   // U+2024 one dot leader
        {"blacksmile", "\xE2\x8C\xA3"}, // U+2323 smiling face (alias)
        {"circleminus", "\xE2\x8A\x96"}, // U+2296 circled minus
        {"circleplus", "\xE2\x8A\x95"},  // U+2295 circled plus
        {"circleopx", "\xE2\x8A\x97"},   // U+2297 circled times
        {"circleslash", "\xE2\x8A\x98"}, // U+2298 circled division slash
        {"prec", "\xE2\x89\xBA"},     // U+227A precedes
        {"succ", "\xE2\x89\xBB"},     // U+227B succeeds
        {"precneq", "\xE2\x8A\x8F"},  // U+228F precedes and not equal
        {"succneq", "\xE2\x8A\x90"},  // U+2290 succeeds and not equal
        {"notprec", "\xE2\x8A\xB0"},  // U+22B0 does not precede
        {"notsucc", "\xE2\x8A\xB1"},  // U+22B1 does not succeed
        {"nsubset", "\xE2\x8A\x84"},  // U+2284 not a subset of (alias for nsub)
        {"nsupset", "\xE2\x8A\x85"},  // U+2285 not a superset of
        {"nsubseteq", "\xE2\x8A\x88"}, // U+2288 neither a subset of nor equal to
        {"nsupseteq", "\xE2\x8A\x89"}, // U+2289 neither a superset of nor equal to
        {"subsetneq", "\xE2\x8A\x8A"}, // U+228A subset of and not equal to
        {"supsetneq", "\xE2\x8A\x8B"}, // U+228B superset of and not equal to
        {"diamond2", "\xE2\x97\x8A"},  // U+25CA lozenge (alias for loz)
        {"blackrect", "\xE2\x96\xA0"}, // U+25A0 black square
        {"whitesquare", "\xE2\x96\xA1"}, // U+25A1 white square
        {"blackcircle", "\xE2\x97\x80"}, // U+25C0 black circle
        {"whitecircle", "\xE2\x97\x8B"}, // U+25CB white circle
        {"asciimath", "\xE2\x82\xBF"},  // U+207F superscript n
        {"mathminus", "\xE2\x88\x92"},  // U+2212 minus sign (alias for minus)
        {"mathmult", "\xC3\x97"},       // U+00D7 multiplication (alias for times)
        {"ratio2", "\xE2\x88\xB6"},     // U+2236 ratio (alias)
        {"coloneq", "\xE2\x89\x94"},    // U+2254 assign (alias)
        {"eqcolon", "\xE2\x89\x95"},    // U+2255 assign (alias)
        {"dblcolon", "\xE2\x88\xB6"},   // U+2236 double colon (alias)
        {"Vert", "\xE2\x80\x96"},       // U+2016 double vertical line (alias)
        {"parallel", "\xE2\x88\x96"},   // U+2225 parallel to
        {"notparallel", "\xE2\x88\x97"}, // U+2226 not parallel to
        {"upuparrows", "\xE2\x87\x88"}, // U+21C8 upwards paired arrows (alias)
        {"dndn", "\xE2\x87\x89"},       // U+21C9 downwards paired arrows (alias)
        {"lnot", "\xC2\xAC"},           // U+00AC not sign (alias for not)
        {"forall2", "\xE2\x88\x80"},    // U+2200 for all (alias)
        {"exist2", "\xE2\x88\x83"},     // U+2203 there exists (alias)
        {"empty2", "\xE2\x88\x85"},     // U+2205 empty set (alias)
        {"nabla2", "\xE2\x88\x87"},     // U+2207 nabla (alias)
        {"isin2", "\xE2\x88\x88"},      // U+2208 element of (alias),
        {"amath", "\xF0\x9D\x90\x9A"},   // U+1D41A math a
        {"bmath", "\xF0\x9D\x90\x9B"},   // U+1D41B math b
        {"cmath", "\xF0\x9D\x90\x9C"},   // U+1D41C math c
        {"dmath", "\xF0\x9D\x90\x9D"},   // U+1D41D math d
        {"emath", "\xF0\x9D\x90\x9E"},   // U+1D41E math e
        {"fmath", "\xF0\x9D\x90\x9F"},   // U+1D41F math f
        {"gmath", "\xF0\x9D\x90\xA0"},   // U+1D420 math g
        {"hmath", "\xF0\x9D\x90\xA1"},   // U+1D421 math h
        {"imath", "\xF0\x9D\x90\xA2"},   // U+1D422 math i
        {"jmath", "\xF0\x9D\x90\xA3"},   // U+1D423 math j
        {"kmath", "\xF0\x9D\x90\xA4"},   // U+1D424 math k
        {"lmath", "\xF0\x9D\x90\xA5"},   // U+1D425 math l
        {"mmath", "\xF0\x9D\x90\xA6"},   // U+1D426 math m
        {"nmath", "\xF0\x9D\x90\xA7"},   // U+1D427 math n
        {"omath", "\xF0\x9D\x90\xA8"},   // U+1D428 math o
        {"pmath", "\xF0\x9D\x90\xA9"},   // U+1D429 math p
        {"qmath", "\xF0\x9D\x90\xAA"},   // U+1D42A math q
        {"rmath", "\xF0\x9D\x90\xAB"},   // U+1D42B math r
        {"smath", "\xF0\x9D\x90\xAC"},   // U+1D42C math s
        {"tmath", "\xF0\x9D\x90\xAD"},   // U+1D42D math t
        {"umath", "\xF0\x9D\x90\xAE"},   // U+1D42E math u
        {"vmath", "\xF0\x9D\x90\xAF"},   // U+1D42F math v
        {"wmath", "\xF0\x9D\x90\xB0"},   // U+1D430 math w
        {"xmath", "\xF0\x9D\x90\xB1"},   // U+1D431 math x
        {"ymath", "\xF0\x9D\x90\xB2"},   // U+1D432 math y
        {"zmath", "\xF0\x9D\x90\xB3"},   // U+1D433 math z
        {"Amath", "\xF0\x9D\x90\x80"},   // U+1D400 math A
        {"Bmath", "\xF0\x9D\x90\x81"},   // U+1D401 math B
        {"Cmath", "\xF0\x9D\x90\x82"},   // U+1D402 math C
        {"Dmath", "\xF0\x9D\x90\x83"},   // U+1D403 math D
        {"Emath", "\xF0\x9D\x90\x84"},   // U+1D404 math E
        {"Fmath", "\xF0\x9D\x90\x85"},   // U+1D405 math F
        {"Gmath", "\xF0\x9D\x90\x86"},   // U+1D406 math G
        {"Hmath", "\xF0\x9D\x90\x87"},   // U+1D407 math H
        {"Imath", "\xF0\x9D\x90\x88"},   // U+1D408 math I
        {"Jmath", "\xF0\x9D\x90\x89"},   // U+1D409 math J
        {"Kmath", "\xF0\x9D\x90\x8A"},   // U+1D40A math K
        {"Lmath", "\xF0\x9D\x90\x8B"},   // U+1D40B math L
        {"Mmath", "\xF0\x9D\x90\x8C"},   // U+1D40C math M
        {"Nmath", "\xF0\x9D\x90\x8D"},   // U+1D40D math N
        {"Omath", "\xF0\x9D\x90\x8E"},   // U+1D40E math O
        {"Pmath", "\xF0\x9D\x90\x8F"},   // U+1D40F math P
        {"Qmath", "\xF0\x9D\x90\x90"},   // U+1D410 math Q
        {"Rmath", "\xF0\x9D\x90\x91"},   // U+1D411 math R
        {"Smath", "\xF0\x9D\x90\x92"},   // U+1D412 math S
        {"Tmath", "\xF0\x9D\x90\x93"},   // U+1D413 math T
        {"Umath", "\xF0\x9D\x90\x94"},   // U+1D414 math U
        {"Vmath", "\xF0\x9D\x90\x95"},   // U+1D415 math V
        {"Wmath", "\xF0\x9D\x90\x96"},   // U+1D416 math W
        {"Xmath", "\xF0\x9D\x90\x97"},   // U+1D417 math X
        {"Ymath", "\xF0\x9D\x90\x98"},   // U+1D418 math Y
        {"Zmath", "\xF0\x9D\x90\x99"},   // U+1D419 math Z
        {"aimathbi", "\xF0\x9D\x92\x82"},   // U+1D482 imathbi a
        {"bimathbi", "\xF0\x9D\x92\x83"},   // U+1D483 imathbi b
        {"cimathbi", "\xF0\x9D\x92\x84"},   // U+1D484 imathbi c
        {"dimathbi", "\xF0\x9D\x92\x85"},   // U+1D485 imathbi d
        {"eimathbi", "\xF0\x9D\x92\x86"},   // U+1D486 imathbi e
        {"fimathbi", "\xF0\x9D\x92\x87"},   // U+1D487 imathbi f
        {"gimathbi", "\xF0\x9D\x92\x88"},   // U+1D488 imathbi g
        {"himathbi", "\xF0\x9D\x92\x89"},   // U+1D489 imathbi h
        {"iimathbi", "\xF0\x9D\x92\x8A"},   // U+1D48A imathbi i
        {"jimathbi", "\xF0\x9D\x92\x8B"},   // U+1D48B imathbi j
        {"kimathbi", "\xF0\x9D\x92\x8C"},   // U+1D48C imathbi k
        {"limathbi", "\xF0\x9D\x92\x8D"},   // U+1D48D imathbi l
        {"mimathbi", "\xF0\x9D\x92\x8E"},   // U+1D48E imathbi m
        {"nimathbi", "\xF0\x9D\x92\x8F"},   // U+1D48F imathbi n
        {"oimathbi", "\xF0\x9D\x92\x90"},   // U+1D490 imathbi o
        {"pimathbi", "\xF0\x9D\x92\x91"},   // U+1D491 imathbi p
        {"qimathbi", "\xF0\x9D\x92\x92"},   // U+1D492 imathbi q
        {"rimathbi", "\xF0\x9D\x92\x93"},   // U+1D493 imathbi r
        {"simathbi", "\xF0\x9D\x92\x94"},   // U+1D494 imathbi s
        {"timathbi", "\xF0\x9D\x92\x95"},   // U+1D495 imathbi t
        {"uimathbi", "\xF0\x9D\x92\x96"},   // U+1D496 imathbi u
        {"vimathbi", "\xF0\x9D\x92\x97"},   // U+1D497 imathbi v
        {"wimathbi", "\xF0\x9D\x92\x98"},   // U+1D498 imathbi w
        {"ximathbi", "\xF0\x9D\x92\x99"},   // U+1D499 imathbi x
        {"yimathbi", "\xF0\x9D\x92\x9A"},   // U+1D49A imathbi y
        {"zimathbi", "\xF0\x9D\x92\x9B"},   // U+1D49B imathbi z
        {"Aimathbi", "\xF0\x9D\x91\xA8"},   // U+1D468 imathbi A
        {"Bimathbi", "\xF0\x9D\x91\xA9"},   // U+1D469 imathbi B
        {"Cimathbi", "\xF0\x9D\x91\xAA"},   // U+1D46A imathbi C
        {"Dimathbi", "\xF0\x9D\x91\xAB"},   // U+1D46B imathbi D
        {"Eimathbi", "\xF0\x9D\x91\xAC"},   // U+1D46C imathbi E
        {"Fimathbi", "\xF0\x9D\x91\xAD"},   // U+1D46D imathbi F
        {"Gimathbi", "\xF0\x9D\x91\xAE"},   // U+1D46E imathbi G
        {"Himathbi", "\xF0\x9D\x91\xAF"},   // U+1D46F imathbi H
        {"Iimathbi", "\xF0\x9D\x91\xB0"},   // U+1D470 imathbi I
        {"Jimathbi", "\xF0\x9D\x91\xB1"},   // U+1D471 imathbi J
        {"Kimathbi", "\xF0\x9D\x91\xB2"},   // U+1D472 imathbi K
        {"Limathbi", "\xF0\x9D\x91\xB3"},   // U+1D473 imathbi L
        {"Mimathbi", "\xF0\x9D\x91\xB4"},   // U+1D474 imathbi M
        {"Nimathbi", "\xF0\x9D\x91\xB5"},   // U+1D475 imathbi N
        {"Oimathbi", "\xF0\x9D\x91\xB6"},   // U+1D476 imathbi O
        {"Pimathbi", "\xF0\x9D\x91\xB7"},   // U+1D477 imathbi P
        {"Qimathbi", "\xF0\x9D\x91\xB8"},   // U+1D478 imathbi Q
        {"Rimathbi", "\xF0\x9D\x91\xB9"},   // U+1D479 imathbi R
        {"Simathbi", "\xF0\x9D\x91\xBA"},   // U+1D47A imathbi S
        {"Timathbi", "\xF0\x9D\x91\xBB"},   // U+1D47B imathbi T
        {"Uimathbi", "\xF0\x9D\x91\xBC"},   // U+1D47C imathbi U
        {"Vimathbi", "\xF0\x9D\x91\xBD"},   // U+1D47D imathbi V
        {"Wimathbi", "\xF0\x9D\x91\xBE"},   // U+1D47E imathbi W
        {"Ximathbi", "\xF0\x9D\x91\xBF"},   // U+1D47F imathbi X
        {"Yimathbi", "\xF0\x9D\x92\x80"},   // U+1D480 imathbi Y
        {"Zimathbi", "\xF0\x9D\x92\x81"},   // U+1D481 imathbi Z
        {"ascrbold", "\xF0\x9D\x93\xAA"},   // U+1D4EA scrbold a
        {"bscrbold", "\xF0\x9D\x93\xAB"},   // U+1D4EB scrbold b
        {"cscrbold", "\xF0\x9D\x93\xAC"},   // U+1D4EC scrbold c
        {"dscrbold", "\xF0\x9D\x93\xAD"},   // U+1D4ED scrbold d
        {"escrbold", "\xF0\x9D\x93\xAE"},   // U+1D4EE scrbold e
        {"fscrbold", "\xF0\x9D\x93\xAF"},   // U+1D4EF scrbold f
        {"gscrbold", "\xF0\x9D\x93\xB0"},   // U+1D4F0 scrbold g
        {"hscrbold", "\xF0\x9D\x93\xB1"},   // U+1D4F1 scrbold h
        {"iscrbold", "\xF0\x9D\x93\xB2"},   // U+1D4F2 scrbold i
        {"jscrbold", "\xF0\x9D\x93\xB3"},   // U+1D4F3 scrbold j
        {"kscrbold", "\xF0\x9D\x93\xB4"},   // U+1D4F4 scrbold k
        {"lscrbold", "\xF0\x9D\x93\xB5"},   // U+1D4F5 scrbold l
        {"mscrbold", "\xF0\x9D\x93\xB6"},   // U+1D4F6 scrbold m
        {"nscrbold", "\xF0\x9D\x93\xB7"},   // U+1D4F7 scrbold n
        {"oscrbold", "\xF0\x9D\x93\xB8"},   // U+1D4F8 scrbold o
        {"pscrbold", "\xF0\x9D\x93\xB9"},   // U+1D4F9 scrbold p
        {"qscrbold", "\xF0\x9D\x93\xBA"},   // U+1D4FA scrbold q
        {"rscrbold", "\xF0\x9D\x93\xBB"},   // U+1D4FB scrbold r
        {"sscrbold", "\xF0\x9D\x93\xBC"},   // U+1D4FC scrbold s
        {"tscrbold", "\xF0\x9D\x93\xBD"},   // U+1D4FD scrbold t
        {"uscrbold", "\xF0\x9D\x93\xBE"},   // U+1D4FE scrbold u
        {"vscrbold", "\xF0\x9D\x93\xBF"},   // U+1D4FF scrbold v
        {"wscrbold", "\xF0\x9D\x94\x80"},   // U+1D500 scrbold w
        {"xscrbold", "\xF0\x9D\x94\x81"},   // U+1D501 scrbold x
        {"yscrbold", "\xF0\x9D\x94\x82"},   // U+1D502 scrbold y
        {"zscrbold", "\xF0\x9D\x94\x83"},   // U+1D503 scrbold z
        {"Ascrbold", "\xF0\x9D\x93\x90"},   // U+1D4D0 scrbold A
        {"Bscrbold", "\xF0\x9D\x93\x91"},   // U+1D4D1 scrbold B
        {"Cscrbold", "\xF0\x9D\x93\x92"},   // U+1D4D2 scrbold C
        {"Dscrbold", "\xF0\x9D\x93\x93"},   // U+1D4D3 scrbold D
        {"Escrbold", "\xF0\x9D\x93\x94"},   // U+1D4D4 scrbold E
        {"Fscrbold", "\xF0\x9D\x93\x95"},   // U+1D4D5 scrbold F
        {"Gscrbold", "\xF0\x9D\x93\x96"},   // U+1D4D6 scrbold G
        {"Hscrbold", "\xF0\x9D\x93\x97"},   // U+1D4D7 scrbold H
        {"Iscrbold", "\xF0\x9D\x93\x98"},   // U+1D4D8 scrbold I
        {"Jscrbold", "\xF0\x9D\x93\x99"},   // U+1D4D9 scrbold J
        {"Kscrbold", "\xF0\x9D\x93\x9A"},   // U+1D4DA scrbold K
        {"Lscrbold", "\xF0\x9D\x93\x9B"},   // U+1D4DB scrbold L
        {"Mscrbold", "\xF0\x9D\x93\x9C"},   // U+1D4DC scrbold M
        {"Nscrbold", "\xF0\x9D\x93\x9D"},   // U+1D4DD scrbold N
        {"Oscrbold", "\xF0\x9D\x93\x9E"},   // U+1D4DE scrbold O
        {"Pscrbold", "\xF0\x9D\x93\x9F"},   // U+1D4DF scrbold P
        {"Qscrbold", "\xF0\x9D\x93\xA0"},   // U+1D4E0 scrbold Q
        {"Rscrbold", "\xF0\x9D\x93\xA1"},   // U+1D4E1 scrbold R
        {"Sscrbold", "\xF0\x9D\x93\xA2"},   // U+1D4E2 scrbold S
        {"Tscrbold", "\xF0\x9D\x93\xA3"},   // U+1D4E3 scrbold T
        {"Uscrbold", "\xF0\x9D\x93\xA4"},   // U+1D4E4 scrbold U
        {"Vscrbold", "\xF0\x9D\x93\xA5"},   // U+1D4E5 scrbold V
        {"Wscrbold", "\xF0\x9D\x93\xA6"},   // U+1D4E6 scrbold W
        {"Xscrbold", "\xF0\x9D\x93\xA7"},   // U+1D4E7 scrbold X
        {"Yscrbold", "\xF0\x9D\x93\xA8"},   // U+1D4E8 scrbold Y
        {"Zscrbold", "\xF0\x9D\x93\xA9"},   // U+1D4E9 scrbold Z
        {"afrbold", "\xF0\x9D\x96\x86"},   // U+1D586 frbold a
        {"bfrbold", "\xF0\x9D\x96\x87"},   // U+1D587 frbold b
        {"cfrbold", "\xF0\x9D\x96\x88"},   // U+1D588 frbold c
        {"dfrbold", "\xF0\x9D\x96\x89"},   // U+1D589 frbold d
        {"efrbold", "\xF0\x9D\x96\x8A"},   // U+1D58A frbold e
        {"ffrbold", "\xF0\x9D\x96\x8B"},   // U+1D58B frbold f
        {"gfrbold", "\xF0\x9D\x96\x8C"},   // U+1D58C frbold g
        {"hfrbold", "\xF0\x9D\x96\x8D"},   // U+1D58D frbold h
        {"ifrbold", "\xF0\x9D\x96\x8E"},   // U+1D58E frbold i
        {"jfrbold", "\xF0\x9D\x96\x8F"},   // U+1D58F frbold j
        {"kfrbold", "\xF0\x9D\x96\x90"},   // U+1D590 frbold k
        {"lfrbold", "\xF0\x9D\x96\x91"},   // U+1D591 frbold l
        {"mfrbold", "\xF0\x9D\x96\x92"},   // U+1D592 frbold m
        {"nfrbold", "\xF0\x9D\x96\x93"},   // U+1D593 frbold n
        {"ofrbold", "\xF0\x9D\x96\x94"},   // U+1D594 frbold o
        {"pfrbold", "\xF0\x9D\x96\x95"},   // U+1D595 frbold p
        {"qfrbold", "\xF0\x9D\x96\x96"},   // U+1D596 frbold q
        {"rfrbold", "\xF0\x9D\x96\x97"},   // U+1D597 frbold r
        {"sfrbold", "\xF0\x9D\x96\x98"},   // U+1D598 frbold s
        {"tfrbold", "\xF0\x9D\x96\x99"},   // U+1D599 frbold t
        {"ufrbold", "\xF0\x9D\x96\x9A"},   // U+1D59A frbold u
        {"vfrbold", "\xF0\x9D\x96\x9B"},   // U+1D59B frbold v
        {"wfrbold", "\xF0\x9D\x96\x9C"},   // U+1D59C frbold w
        {"xfrbold", "\xF0\x9D\x96\x9D"},   // U+1D59D frbold x
        {"yfrbold", "\xF0\x9D\x96\x9E"},   // U+1D59E frbold y
        {"zfrbold", "\xF0\x9D\x96\x9F"},   // U+1D59F frbold z
        {"Afrbold", "\xF0\x9D\x95\xAC"},   // U+1D56C frbold A
        {"Bfrbold", "\xF0\x9D\x95\xAD"},   // U+1D56D frbold B
        {"Cfrbold", "\xF0\x9D\x95\xAE"},   // U+1D56E frbold C
        {"Dfrbold", "\xF0\x9D\x95\xAF"},   // U+1D56F frbold D
        {"Efrbold", "\xF0\x9D\x95\xB0"},   // U+1D570 frbold E
        {"Ffrbold", "\xF0\x9D\x95\xB1"},   // U+1D571 frbold F
        {"Gfrbold", "\xF0\x9D\x95\xB2"},   // U+1D572 frbold G
        {"Hfrbold", "\xF0\x9D\x95\xB3"},   // U+1D573 frbold H
        {"Ifrbold", "\xF0\x9D\x95\xB4"},   // U+1D574 frbold I
        {"Jfrbold", "\xF0\x9D\x95\xB5"},   // U+1D575 frbold J
        {"Kfrbold", "\xF0\x9D\x95\xB6"},   // U+1D576 frbold K
        {"Lfrbold", "\xF0\x9D\x95\xB7"},   // U+1D577 frbold L
        {"Mfrbold", "\xF0\x9D\x95\xB8"},   // U+1D578 frbold M
        {"Nfrbold", "\xF0\x9D\x95\xB9"},   // U+1D579 frbold N
        {"Ofrbold", "\xF0\x9D\x95\xBA"},   // U+1D57A frbold O
        {"Pfrbold", "\xF0\x9D\x95\xBB"},   // U+1D57B frbold P
        {"Qfrbold", "\xF0\x9D\x95\xBC"},   // U+1D57C frbold Q
        {"Rfrbold", "\xF0\x9D\x95\xBD"},   // U+1D57D frbold R
        {"Sfrbold", "\xF0\x9D\x95\xBE"},   // U+1D57E frbold S
        {"Tfrbold", "\xF0\x9D\x95\xBF"},   // U+1D57F frbold T
        {"Ufrbold", "\xF0\x9D\x96\x80"},   // U+1D580 frbold U
        {"Vfrbold", "\xF0\x9D\x96\x81"},   // U+1D581 frbold V
        {"Wfrbold", "\xF0\x9D\x96\x82"},   // U+1D582 frbold W
        {"Xfrbold", "\xF0\x9D\x96\x83"},   // U+1D583 frbold X
        {"Yfrbold", "\xF0\x9D\x96\x84"},   // U+1D584 frbold Y
        {"Zfrbold", "\xF0\x9D\x96\x85"},   // U+1D585 frbold Z
        {"asans", "\xF0\x9D\x96\xBA"},   // U+1D5BA sans a
        {"bsans", "\xF0\x9D\x96\xBB"},   // U+1D5BB sans b
        {"csans", "\xF0\x9D\x96\xBC"},   // U+1D5BC sans c
        {"dsans", "\xF0\x9D\x96\xBD"},   // U+1D5BD sans d
        {"esans", "\xF0\x9D\x96\xBE"},   // U+1D5BE sans e
        {"fsans", "\xF0\x9D\x96\xBF"},   // U+1D5BF sans f
        {"gsans", "\xF0\x9D\x97\x80"},   // U+1D5C0 sans g
        {"hsans", "\xF0\x9D\x97\x81"},   // U+1D5C1 sans h
        {"isans", "\xF0\x9D\x97\x82"},   // U+1D5C2 sans i
        {"jsans", "\xF0\x9D\x97\x83"},   // U+1D5C3 sans j
        {"ksans", "\xF0\x9D\x97\x84"},   // U+1D5C4 sans k
        {"lsans", "\xF0\x9D\x97\x85"},   // U+1D5C5 sans l
        {"msans", "\xF0\x9D\x97\x86"},   // U+1D5C6 sans m
        {"nsans", "\xF0\x9D\x97\x87"},   // U+1D5C7 sans n
        {"osans", "\xF0\x9D\x97\x88"},   // U+1D5C8 sans o
        {"psans", "\xF0\x9D\x97\x89"},   // U+1D5C9 sans p
        {"qsans", "\xF0\x9D\x97\x8A"},   // U+1D5CA sans q
        {"rsans", "\xF0\x9D\x97\x8B"},   // U+1D5CB sans r
        {"ssans", "\xF0\x9D\x97\x8C"},   // U+1D5CC sans s
        {"tsans", "\xF0\x9D\x97\x8D"},   // U+1D5CD sans t
        {"usans", "\xF0\x9D\x97\x8E"},   // U+1D5CE sans u
        {"vsans", "\xF0\x9D\x97\x8F"},   // U-1D5CF sans v
        {"wsans", "\xF0\x9D\x97\x90"},   // U+1D5D0 sans w
        {"xsans", "\xF0\x9D\x97\x91"},   // U+1D5D1 sans x
        {"ysans", "\xF0\x9D\x97\x92"},   // U+1D5D2 sans y
        {"zsans", "\xF0\x9D\x97\x93"},   // U+1D5D3 sans z
        {"Asans", "\xF0\x9D\x96\xA0"},   // U+1D5A0 sans A
        {"Bsans", "\xF0\x9D\x96\xA1"},   // U+1D5A1 sans B
        {"Csans", "\xF0\x9D\x96\xA2"},   // U+1D5A2 sans C
        {"Dsans", "\xF0\x9D\x96\xA3"},   // U+1D5A3 sans D
        {"Esans", "\xF0\x9D\x96\xA4"},   // U+1D5A4 sans E
        {"Fsans", "\xF0\x9D\x96\xA5"},   // U+1D5A5 sans F
        {"Gsans", "\xF0\x9D\x96\xA6"},   // U+1D5A6 sans G
        {"Hsans", "\xF0\x9D\x96\xA7"},   // U+1D5A7 sans H
        {"Isans", "\xF0\x9D\x96\xA8"},   // U+1D5A8 sans I
        {"Jsans", "\xF0\x9D\x96\xA9"},   // U+1D5A9 sans J
        {"Ksans", "\xF0\x9D\x96\xAA"},   // U+1D5AA sans K
        {"Lsans", "\xF0\x9D\x96\xAB"},   // U+1D5AB sans L
        {"Msans", "\xF0\x9D\x96\xAC"},   // U+1D5AC sans M
        {"Nsans", "\xF0\x9D\x96\xAD"},   // U+1D5AD sans N
        {"Osans", "\xF0\x9D\x96\xAE"},   // U+1D5AE sans O
        {"Psans", "\xF0\x9D\x96\xAF"},   // U+1D5AF sans P
        {"Qsans", "\xF0\x9D\x96\xB0"},   // U+1D5B0 sans Q
        {"Rsans", "\xF0\x9D\x96\xB1"},   // U+1D5B1 sans R
        {"Ssans", "\xF0\x9D\x96\xB2"},   // U+1D5B2 sans S
        {"Tsans", "\xF0\x9D\x96\xB3"},   // U+1D5B3 sans T
        {"Usans", "\xF0\x9D\x96\xB4"},   // U+1D5B4 sans U
        {"Vsans", "\xF0\x9D\x96\xB5"},   // U+1D5B5 sans V
        {"Wsans", "\xF0\x9D\x96\xB6"},   // U+1D5B6 sans W
        {"Xsans", "\xF0\x9D\x96\xB7"},   // U+1D5B7 sans X
        {"Ysans", "\xF0\x9D\x96\xB8"},   // U+1D5B8 sans Y
        {"Zsans", "\xF0\x9D\x96\xB9"},   // U+1D5B9 sans Z
        {"asansbold", "\xF0\x9D\x97\xAE"},   // U+1D5EE sansbold a
        {"bsansbold", "\xF0\x9D\x97\xAF"},   // U+1D5EF sansbold b
        {"csansbold", "\xF0\x9D\x97\xB0"},   // U+1D5F0 sansbold c
        {"dsansbold", "\xF0\x9D\x97\xB1"},   // U+1D5F1 sansbold d
        {"esansbold", "\xF0\x9D\x97\xB2"},   // U+1D5F2 sansbold e
        {"fsansbold", "\xF0\x9D\x97\xB3"},   // U+1D5F3 sansbold f
        {"gsansbold", "\xF0\x9D\x97\xB4"},   // U+1D5F4 sansbold g
        {"hsansbold", "\xF0\x9D\x97\xB5"},   // U+1D5F5 sansbold h
        {"isansbold", "\xF0\x9D\x97\xB6"},   // U+1D5F6 sansbold i
        {"jsansbold", "\xF0\x9D\x97\xB7"},   // U+1D5F7 sansbold j
        {"ksansbold", "\xF0\x9D\x97\xB8"},   // U+1D5F8 sansbold k
        {"lsansbold", "\xF0\x9D\x97\xB9"},   // U+1D5F9 sansbold l
        {"msansbold", "\xF0\x9D\x97\xBA"},   // U+1D5FA sansbold m
        {"nsansbold", "\xF0\x9D\x97\xBB"},   // U+1D5FB sansbold n
        {"osansbold", "\xF0\x9D\x97\xBC"},   // U+1D5FC sansbold o
        {"psansbold", "\xF0\x9D\x97\xBD"},   // U+1D5FD sansbold p
        {"qsansbold", "\xF0\x9D\x97\xBE"},   // U+1D5FE sansbold q
        {"rsansbold", "\xF0\x9D\x97\xBF"},   // U+1D5FF sansbold r
        {"ssansbold", "\xF0\x9D\x98\x80"},   // U+1D600 sansbold s
        {"tsansbold", "\xF0\x9D\x98\x81"},   // U+1D601 sansbold t
        {"usansbold", "\xF0\x9D\x98\x82"},   // U+1D602 sansbold u
        {"vsansbold", "\xF0\x9D\x98\x83"},   // U+1D603 sansbold v
        {"wsansbold", "\xF0\x9D\x98\x84"},   // U+1D604 sansbold w
        {"xsansbold", "\xF0\x9D\x98\x85"},   // U+1D605 sansbold x
        {"ysansbold", "\xF0\x9D\x98\x86"},   // U+1D606 sansbold y
        {"zsansbold", "\xF0\x9D\x98\x87"},   // U+1D607 sansbold z
        {"Asansbold", "\xF0\x9D\x97\x94"},   // U+1D5D4 sansbold A
        {"Bsansbold", "\xF0\x9D\x97\x95"},   // U+1D5D5 sansbold B
        {"Csansbold", "\xF0\x9D\x97\x96"},   // U+1D5D6 sansbold C
        {"Dsansbold", "\xF0\x9D\x97\x97"},   // U+1D5D7 sansbold D
        {"Esansbold", "\xF0\x9D\x97\x98"},   // U+1D5D8 sansbold E
        {"Fsansbold", "\xF0\x9D\x97\x99"},   // U+1D5D9 sansbold F
        {"Gsansbold", "\xF0\x9D\x97\x9A"},   // U+1D5DA sansbold G
        {"Hsansbold", "\xF0\x9D\x97\x9B"},   // U+1D5DB sansbold H
        {"Isansbold", "\xF0\x9D\x97\x9C"},   // U+1D5DC sansbold I
        {"Jsansbold", "\xF0\x9D\x97\x9D"},   // U+1D5DD sansbold J
        {"Ksansbold", "\xF0\x9D\x97\x9E"},   // U+1D5DE sansbold K
        {"Lsansbold", "\xF0\x9D\x97\x9F"},   // U+1D5DF sansbold L
        {"Msansbold", "\xF0\x9D\x97\xA0"},   // U+1D5E0 sansbold M
        {"Nsansbold", "\xF0\x9D\x97\xA1"},   // U+1D5E1 sansbold N
        {"Osansbold", "\xF0\x9D\x97\xA2"},   // U+1D5E2 sansbold O
        {"Psansbold", "\xF0\x9D\x97\xA3"},   // U+1D5E3 sansbold P
        {"Qsansbold", "\xF0\x9D\x97\xA4"},   // U+1D5E4 sansbold Q
        {"Rsansbold", "\xF0\x9D\x97\xA5"},   // U+1D5E5 sansbold R
        {"Ssansbold", "\xF0\x9D\x97\xA6"},   // U+1D5E6 sansbold S
        {"Tsansbold", "\xF0\x9D\x97\xA7"},   // U+1D5E7 sansbold T
        {"Usansbold", "\xF0\x9D\x97\xA8"},   // U+1D5E8 sansbold U
        {"Vsansbold", "\xF0\x9D\x97\xA9"},   // U+1D5E9 sansbold V
        {"Wsansbold", "\xF0\x9D\x97\xAA"},   // U+1D5EA sansbold W
        {"Xsansbold", "\xF0\x9D\x97\xAB"},   // U+1D5EB sansbold X
        {"Ysansbold", "\xF0\x9D\x97\xAC"},   // U+1D5EC sansbold Y
        {"Zsansbold", "\xF0\x9D\x97\xAD"},   // U+1D5ED sansbold Z
        {"asansitalic", "\xF0\x9D\x98\xA2"},   // U+1D622 sansitalic a
        {"bsansitalic", "\xF0\x9D\x98\xA3"},   // U+1D623 sansitalic b
        {"csansitalic", "\xF0\x9D\x98\xA4"},   // U+1D624 sansitalic c
        {"dsansitalic", "\xF0\x9D\x98\xA5"},   // U+1D625 sansitalic d
        {"esansitalic", "\xF0\x9D\x98\xA6"},   // U+1D626 sansitalic e
        {"fsansitalic", "\xF0\x9D\x98\xA7"},   // U+1D627 sansitalic f
        {"gsansitalic", "\xF0\x9D\x98\xA8"},   // U+1D628 sansitalic g
        {"hsansitalic", "\xF0\x9D\x98\xA9"},   // U+1D629 sansitalic h
        {"isansitalic", "\xF0\x9D\x98\xAA"},   // U+1D62A sansitalic i
        {"jsansitalic", "\xF0\x9D\x98\xAB"},   // U+1D62B sansitalic j
        {"ksansitalic", "\xF0\x9D\x98\xAC"},   // U+1D62C sansitalic k
        {"lsansitalic", "\xF0\x9D\x98\xAD"},   // U+1D62D sansitalic l
        {"msansitalic", "\xF0\x9D\x98\xAE"},   // U+1D62E sansitalic m
        {"nsansitalic", "\xF0\x9D\x98\xAF"},   // U+1D62F sansitalic n
        {"osansitalic", "\xF0\x9D\x98\xB0"},   // U+1D630 sansitalic o
        {"psansitalic", "\xF0\x9D\x98\xB1"},   // U+1D631 sansitalic p
        {"qsansitalic", "\xF0\x9D\x98\xB2"},   // U+1D632 sansitalic q
        {"rsansitalic", "\xF0\x9D\x98\xB3"},   // U+1D633 sansitalic r
        {"ssansitalic", "\xF0\x9D\x98\xB4"},   // U+1D634 sansitalic s
        {"tsansitalic", "\xF0\x9D\x98\xB5"},   // U+1D635 sansitalic t
        {"usansitalic", "\xF0\x9D\x98\xB6"},   // U+1D636 sansitalic u
        {"vsansitalic", "\xF0\x9D\x98\xB7"},   // U+1D637 sansitalic v
        {"wsansitalic", "\xF0\x9D\x98\xB8"},   // U+1D638 sansitalic w
        {"xsansitalic", "\xF0\x9D\x98\xB9"},   // U+1D639 sansitalic x
        {"ysansitalic", "\xF0\x9D\x98\xBA"},   // U+1D63A sansitalic y
        {"zsansitalic", "\xF0\x9D\x98\xBB"},   // U+1D63B sansitalic z
        {"Asansitalic", "\xF0\x9D\x98\x88"},   // U+1D608 sansitalic A
        {"Bsansitalic", "\xF0\x9D\x98\x89"},   // U+1D609 sansitalic B
        {"Csansitalic", "\xF0\x9D\x98\x8A"},   // U+1D60A sansitalic C
        {"Dsansitalic", "\xF0\x9D\x98\x8B"},   // U+1D60B sansitalic D
        {"Esansitalic", "\xF0\x9D\x98\x8C"},   // U+1D60C sansitalic E
        {"Fsansitalic", "\xF0\x9D\x98\x8D"},   // U+1D60D sansitalic F
        {"Gsansitalic", "\xF0\x9D\x98\x8E"},   // U+1D60E sansitalic G
        {"Hsansitalic", "\xF0\x9D\x98\x8F"},   // U+1D60F sansitalic H
        {"Isansitalic", "\xF0\x9D\x98\x90"},   // U+1D610 sansitalic I
        {"Jsansitalic", "\xF0\x9D\x98\x91"},   // U+1D611 sansitalic J
        {"Ksansitalic", "\xF0\x9D\x98\x92"},   // U+1D612 sansitalic K
        {"Lsansitalic", "\xF0\x9D\x98\x93"},   // U+1D613 sansitalic L
        {"Msansitalic", "\xF0\x9D\x98\x94"},   // U+1D614 sansitalic M
        {"Nsansitalic", "\xF0\x9D\x98\x95"},   // U+1D615 sansitalic N
        {"Osansitalic", "\xF0\x9D\x98\x96"},   // U+1D616 sansitalic O
        {"Psansitalic", "\xF0\x9D\x98\x97"},   // U+1D617 sansitalic P
        {"Qsansitalic", "\xF0\x9D\x98\x98"},   // U+1D618 sansitalic Q
        {"Rsansitalic", "\xF0\x9D\x98\x99"},   // U+1D619 sansitalic R
        {"Ssansitalic", "\xF0\x9D\x98\x9A"},   // U+1D61A sansitalic S
        {"Tsansitalic", "\xF0\x9D\x98\x9B"},   // U+1D61B sansitalic T
        {"Usansitalic", "\xF0\x9D\x98\x9C"},   // U+1D61C sansitalic U
        {"Vsansitalic", "\xF0\x9D\x98\x9D"},   // U+1D61D sansitalic V
        {"Wsansitalic", "\xF0\x9D\x98\x9E"},   // U+1D61E sansitalic W
        {"Xsansitalic", "\xF0\x9D\x98\x9F"},   // U+1D61F sansitalic X
        {"Ysansitalic", "\xF0\x9D\x98\xA0"},   // U+1D620 sansitalic Y
        {"Zsansitalic", "\xF0\x9D\x98\xA1"},   // U+1D621 sansitalic Z
        {"asansbolditalic", "\xF0\x9D\x99\x96"},   // U+1D656 sansbolditalic a
        {"bsansbolditalic", "\xF0\x9D\x99\x97"},   // U+1D657 sansbolditalic b
        {"csansbolditalic", "\xF0\x9D\x99\x98"},   // U+1D658 sansbolditalic c
        {"dsansbolditalic", "\xF0\x9D\x99\x99"},   // U+1D659 sansbolditalic d
        {"esansbolditalic", "\xF0\x9D\x99\x9A"},   // U+1D65A sansbolditalic e
        {"fsansbolditalic", "\xF0\x9D\x99\x9B"},   // U+1D65B sansbolditalic f
        {"gsansbolditalic", "\xF0\x9D\x99\x9C"},   // U+1D65C sansbolditalic g
        {"hsansbolditalic", "\xF0\x9D\x99\x9D"},   // U+1D65D sansbolditalic h
        {"isansbolditalic", "\xF0\x9D\x99\x9E"},   // U+1D65E sansbolditalic i
        {"jsansbolditalic", "\xF0\x9D\x99\x9F"},   // U+1D65F sansbolditalic j
        {"ksansbolditalic", "\xF0\x9D\x99\xA0"},   // U+1D660 sansbolditalic k
        {"lsansbolditalic", "\xF0\x9D\x99\xA1"},   // U+1D661 sansbolditalic l
        {"msansbolditalic", "\xF0\x9D\x99\xA2"},   // U+1D662 sansbolditalic m
        {"nsansbolditalic", "\xF0\x9D\x99\xA3"},   // U+1D663 sansbolditalic n
        {"osansbolditalic", "\xF0\x9D\x99\xA4"},   // U+1D664 sansbolditalic o
        {"psansbolditalic", "\xF0\x9D\x99\xA5"},   // U+1D665 sansbolditalic p
        {"qsansbolditalic", "\xF0\x9D\x99\xA6"},   // U+1D666 sansbolditalic q
        {"rsansbolditalic", "\xF0\x9D\x99\xA7"},   // U+1D667 sansbolditalic r
        {"ssansbolditalic", "\xF0\x9D\x99\xA8"},   // U+1D668 sansbolditalic s
        {"tsansbolditalic", "\xF0\x9D\x99\xA9"},   // U+1D669 sansbolditalic t
        {"usansbolditalic", "\xF0\x9D\x99\xAA"},   // U+1D66A sansbolditalic u
        {"vsansbolditalic", "\xF0\x9D\x99\xAB"},   // U+1D66B sansbolditalic v
        {"wsansbolditalic", "\xF0\x9D\x99\xAC"},   // U+1D66C sansbolditalic w
        {"xsansbolditalic", "\xF0\x9D\x99\xAD"},   // U+1D66D sansbolditalic x
        {"ysansbolditalic", "\xF0\x9D\x99\xAE"},   // U+1D66E sansbolditalic y
        {"zsansbolditalic", "\xF0\x9D\x99\xAF"},   // U+1D66F sansbolditalic z
        {"Asansbolditalic", "\xF0\x9D\x98\xBC"},   // U+1D63C sansbolditalic A
        {"Bsansbolditalic", "\xF0\x9D\x98\xBD"},   // U+1D63D sansbolditalic B
        {"Csansbolditalic", "\xF0\x9D\x98\xBE"},   // U+1D63E sansbolditalic C
        {"Dsansbolditalic", "\xF0\x9D\x98\xBF"},   // U+1D63F sansbolditalic D
        {"Esansbolditalic", "\xF0\x9D\x99\x80"},   // U+1D640 sansbolditalic E
        {"Fsansbolditalic", "\xF0\x9D\x99\x81"},   // U+1D641 sansbolditalic F
        {"Gsansbolditalic", "\xF0\x9D\x99\x82"},   // U+1D642 sansbolditalic G
        {"Hsansbolditalic", "\xF0\x9D\x99\x83"},   // U+1D643 sansbolditalic H
        {"Isansbolditalic", "\xF0\x9D\x99\x84"},   // U+1D644 sansbolditalic I
        {"Jsansbolditalic", "\xF0\x9D\x99\x85"},   // U+1D645 sansbolditalic J
        {"Ksansbolditalic", "\xF0\x9D\x99\x86"},   // U+1D646 sansbolditalic K
        {"Lsansbolditalic", "\xF0\x9D\x99\x87"},   // U+1D647 sansbolditalic L
        {"Msansbolditalic", "\xF0\x9D\x99\x88"},   // U+1D648 sansbolditalic M
        {"Nsansbolditalic", "\xF0\x9D\x99\x89"},   // U+1D649 sansbolditalic N
        {"Osansbolditalic", "\xF0\x9D\x99\x8A"},   // U+1D64A sansbolditalic O
        {"Psansbolditalic", "\xF0\x9D\x99\x8B"},   // U+1D64B sansbolditalic P
        {"Qsansbolditalic", "\xF0\x9D\x99\x8C"},   // U+1D64C sansbolditalic Q
        {"Rsansbolditalic", "\xF0\x9D\x99\x8D"},   // U+1D64D sansbolditalic R
        {"Ssansbolditalic", "\xF0\x9D\x99\x8E"},   // U+1D64E sansbolditalic S
        {"Tsansbolditalic", "\xF0\x9D\x99\x8F"},   // U+1D64F sansbolditalic T
        {"Usansbolditalic", "\xF0\x9D\x99\x90"},   // U+1D650 sansbolditalic U
        {"Vsansbolditalic", "\xF0\x9D\x99\x91"},   // U+1D651 sansbolditalic V
        {"Wsansbolditalic", "\xF0\x9D\x99\x92"},   // U+1D652 sansbolditalic W
        {"Xsansbolditalic", "\xF0\x9D\x99\x93"},   // U+1D653 sansbolditalic X
        {"Ysansbolditalic", "\xF0\x9D\x99\x94"},   // U+1D654 sansbolditalic Y
        {"Zsansbolditalic", "\xF0\x9D\x99\x95"},   // U+1D655 sansbolditalic Z
        {"amono", "\xF0\x9D\x9A\x8A"},   // U+1D68A mono a
        {"bmono", "\xF0\x9D\x9A\x8B"},   // U+1D68B mono b
        {"cmono", "\xF0\x9D\x9A\x8C"},   // U+1D68C mono c
        {"dmono", "\xF0\x9D\x9A\x8D"},   // U+1D68D mono d
        {"emono", "\xF0\x9D\x9A\x8E"},   // U+1D68E mono e
        {"fmono", "\xF0\x9D\x9A\x8F"},   // U+1D68F mono f
        {"gmono", "\xF0\x9D\x9A\x90"},   // U+1D690 mono g
        {"hmono", "\xF0\x9D\x9A\x91"},   // U+1D691 mono h
        {"imono", "\xF0\x9D\x9A\x92"},   // U+1D692 mono i
        {"jmono", "\xF0\x9D\x9A\x93"},   // U+1D693 mono j
        {"kmono", "\xF0\x9D\x9A\x94"},   // U+1D694 mono k
        {"lmono", "\xF0\x9D\x9A\x95"},   // U+1D695 mono l
        {"mmono", "\xF0\x9D\x9A\x96"},   // U+1D696 mono m
        {"nmono", "\xF0\x9D\x9A\x97"},   // U+1D697 mono n
        {"omono", "\xF0\x9D\x9A\x98"},   // U+1D698 mono o
        {"pmono", "\xF0\x9D\x9A\x99"},   // U+1D699 mono p
        {"qmono", "\xF0\x9D\x9A\x9A"},   // U+1D69A mono q
        {"rmono", "\xF0\x9D\x9A\x9B"},   // U+1D69B mono r
        {"smono", "\xF0\x9D\x9A\x9C"},   // U+1D69C mono s
        {"tmono", "\xF0\x9D\x9A\x9D"},   // U+1D69D mono t
        {"umono", "\xF0\x9D\x9A\x9E"},   // U+1D69E mono u
        {"vmono", "\xF0\x9D\x9A\x9F"},   // U+1D69F mono v
        {"wmono", "\xF0\x9D\x9A\xA0"},   // U+1D6A0 mono w
        {"xmono", "\xF0\x9D\x9A\xA1"},   // U+1D6A1 mono x
        {"ymono", "\xF0\x9D\x9A\xA2"},   // U+1D6A2 mono y
        {"zmono", "\xF0\x9D\x9A\xA3"},   // U+1D6A3 mono z
        {"Amono", "\xF0\x9D\x99\xB0"},   // U+1D670 mono A
        {"Bmono", "\xF0\x9D\x99\xB1"},   // U+1D671 mono B
        {"Cmono", "\xF0\x9D\x99\xB2"},   // U+1D672 mono C
        {"Dmono", "\xF0\x9D\x99\xB3"},   // U+1D673 mono D
        {"Emono", "\xF0\x9D\x99\xB4"},   // U+1D674 mono E
        {"Fmono", "\xF0\x9D\x99\xB5"},   // U+1D675 mono F
        {"Gmono", "\xF0\x9D\x99\xB6"},   // U+1D676 mono G
        {"Hmono", "\xF0\x9D\x99\xB7"},   // U+1D677 mono H
        {"Imono", "\xF0\x9D\x99\xB8"},   // U+1D678 mono I
        {"Jmono", "\xF0\x9D\x99\xB9"},   // U+1D679 mono J
        {"Kmono", "\xF0\x9D\x99\xBA"},   // U+1D67A mono K
        {"Lmono", "\xF0\x9D\x99\xBB"},   // U+1D67B mono L
        {"Mmono", "\xF0\x9D\x99\xBC"},   // U+1D67C mono M
        {"Nmono", "\xF0\x9D\x99\xBD"},   // U+1D67D mono N
        {"Omono", "\xF0\x9D\x99\xBE"},   // U+1D67E mono O
        {"Pmono", "\xF0\x9D\x99\xBF"},   // U+1D67F mono P
        {"Qmono", "\xF0\x9D\x9A\x80"},   // U+1D680 mono Q
        {"Rmono", "\xF0\x9D\x9A\x81"},   // U+1D681 mono R
        {"Smono", "\xF0\x9D\x9A\x82"},   // U+1D682 mono S
        {"Tmono", "\xF0\x9D\x9A\x83"},   // U+1D683 mono T
        {"Umono", "\xF0\x9D\x9A\x84"},   // U+1D684 mono U
        {"Vmono", "\xF0\x9D\x9A\x85"},   // U+1D685 mono V
        {"Wmono", "\xF0\x9D\x9A\x86"},   // U+1D686 mono W
        {"Xmono", "\xF0\x9D\x9A\x87"},   // U+1D687 mono X
        {"Ymono", "\xF0\x9D\x9A\x88"},   // U+1D688 mono Y
        {"Zmono", "\xF0\x9D\x9A\x89"},   // U+1D689 mono Z
        {"longrarr", "\xE2\x9F\xB6"},   // U+27F6 long rightwards arrow
        {"longlArr", "\xE2\x9F\xB8"},   // U+27F8 long leftwards double arrow
        {"longrArr", "\xE2\x9F\xB9"},   // U+27F9 long rightwards double arrow
        {"setminus", "\xE2\x88\x96"},   // U+2216 set minus
        {"smallsetminus", "\xE2\x88\x96"},   // U+2216 small set minus
        {"ltimes", "\xE2\x8B\x89"},   // U+22C9 left times
        {"rtimes", "\xE2\x8B\x8A"},   // U+22CA right times
        {"notapprox", "\xE2\x89\x87"},   // U+2247 neither approximately nor actually equal to
        {"nappro", "\xE2\x89\x89"},   // U+2249 not almost equal to
        {"notequiv", "\xE2\x89\xA2"},   // U+2262 not identical to
        {"nottilde", "\xE2\x89\x81"},   // U+2241 not tilde
        {"updownarrows", "\xE2\x87\x95"},   // U+21D5 up down double arrow
        {"leftrightsquigarrow", "\xE2\x86\xAD"},   // U+21AD left right wave arrow
        {"curlyvee", "\xE2\x8B\x8E"},   // U+22CE curly logical or
        {"curlywedge", "\xE2\x8B\x8F"},   // U+22CF curly logical and
        {"therefore", "\xE2\x88\xB4"},   // U+2234 therefore
        {"because", "\xE2\x88\xB5"},   // U+2235 because
        {"measuredangle", "\xE2\x88\xA1"},   // U+2221 measured angle

        // === Additional Missing HTML5 Entities ===
        {"Ccy", "\xd0\xa6"},    // U+0426
        {"Digamma", "\xcf\x9c"},    // U+03DC
        {"Goppaw", "\xcf\x9c"},    // U+03DC
        {"Hcy", "\xd0\xa5"},    // U+0425
        {"Koppa", "\xcf\x98"},    // U+03D8
        {"Sampi", "\xcf\xa0"},    // U+03E0
        {"Stigma", "\xcf\x9a"},    // U+03DA
        {"Xcy", "\xd0\xa5"},    // U+0425
        {"abslate", "\xe2\x88\xa3"},    // U+2223
        {"aftersymbol", "\xe2\x88\x8d"},    // U+220D
        {"backcong", "\xe2\x89\x8c"},    // U+224C
        {"backsimeq", "\xe2\x8b\x8d"},    // U+22CD
        {"barsemicircle", "\xe2\x8c\x86"},    // U+2306
        {"barvee", "\xe2\x8a\xbd"},    // U+22BD
        {"barwedge", "\xe2\x8a\xbc"},    // U+22BC
        {"barwedgecirc", "\xe2\x8c\x85"},    // U+2305
        {"batik", "\xe2\x88\xbd"},    // U+223D
        {"bemptyv", "\xe2\xa6\xb0"},    // U+29B0
        {"bepsi", "\xe2\x88\x8b"},    // U+220B
        {"bfpartial", "\xe2\x88\x82"},    // U+2202
        {"boxbowtie", "\xe2\xa7\x88"},    // U+29C8
        {"boxbslash", "\xe2\xa7\x85"},    // U+29C5
        {"boxhd", "\xe2\x95\xa6"},    // U+2566
        {"boxhu", "\xe2\x95\xa9"},    // U+2569
        {"boxvl", "\xe2\x95\xa1"},    // U+2561
        {"boxvr", "\xe2\x95\x9e"},    // U+255E
        {"bsemi", "\xe2\x81\x8f"},    // U+204F
        {"btimes", "\xe2\x8a\xa2"},    // U+22A2
        {"bumpeq", "\xe2\x89\x8f"},    // U+224F
        {"bumpnoteq", "\xe2\x89\x8e"},    // U+224E
        {"butwedge", "\xe2\x8a\xa5"},    // U+22A5
        {"ccy", "\xd1\x86"},    // U+0446
        {"circeq", "\xe2\x89\x97"},    // U+2257
        {"circleast", "\xe2\x8a\x9b"},    // U+229B
        {"circledX", "\xe2\x8a\x98"},    // U+2298
        {"circledash", "\xe2\x8a\x9d"},    // U+229D
        {"circledot", "\xe2\x8a\x99"},    // U+2299
        {"circledown", "\xe2\xa6\xa3"},    // U+29A3
        {"circletimes", "\xe2\x8a\x97"},    // U+2297
        {"circnearequal", "\xe2\x89\x97"},    // U+2257
        {"goppaw", "\xcf\x9d"},    // U+03DD
        {"hbar", "\xe2\x84\x8f"},    // U+210F
        {"hcy", "\xd1\x85"},    // U+0445
        {"hslash", "\xe2\x84\x8f"},    // U+210F
        {"romnumeralC", "\xe2\x85\xbd"},    // U+217D
        {"romnumeralD", "\xe2\x85\xbe"},    // U+217E
        {"romnumeralL", "\xe2\x85\xbc"},    // U+217C
        {"romnumeralM", "\xe2\x85\xbf"},    // U+217F
        {"romnumerali", "\xe2\x85\xb0"},    // U+2170
        {"romnumeralii", "\xe2\x85\xb1"},    // U+2171
        {"romnumeraliii", "\xe2\x85\xb2"},    // U+2172
        {"romnumeraliv", "\xe2\x85\xb3"},    // U+2173
        {"romnumeralix", "\xe2\x85\xb8"},    // U+2178
        {"romnumeralv", "\xe2\x85\xb4"},    // U+2174
        {"romnumeralvi", "\xe2\x85\xb5"},    // U+2175
        {"romnumeralvii", "\xe2\x85\xb6"},    // U+2176
        {"romnumeralviii", "\xe2\x85\xb7"},    // U+2177
        {"romnumeralx", "\xe2\x85\xb9"},    // U+2179
        {"romnumeralxi", "\xe2\x85\xba"},    // U+217A
        {"romnumeralxii", "\xe2\x85\xbb"},    // U+217B
        {"sampi", "\xcf\xa1"},    // U+03E1
        {"stigma", "\xcf\x9b"},    // U+03DB
        {"xcy", "\xd1\x85"},    // U+0445

        // === Hebrew Letters ===
        {"aleph", "\xd7\x90"},    // U+05D0
        {"beth", "\xd7\x91"},    // U+05D1
        {"gimel", "\xd7\x92"},    // U+05D2
        {"daleth", "\xd7\x93"},    // U+05D3
        {"he", "\xd7\x94"},    // U+05D4
        {"waw", "\xd7\x95"},    // U+05D5
        {"zayin", "\xd7\x96"},    // U+05D6
        {"het", "\xd7\x97"},    // U+05D7
        {"teth", "\xd7\x98"},    // U+05D8
        {"yod", "\xd7\x99"},    // U+05D9
        {"kaph", "\xd7\x9b"},    // U+05DB
        {"lamed", "\xd7\x9c"},    // U+05DC
        {"mem", "\xd7\x9e"},    // U+05DE
        {"nun", "\xd7\xa0"},    // U+05E0
        {"samekh", "\xd7\xa1"},    // U+05E1
        {"ayin", "\xd7\xa2"},    // U+05E2
        {"pe", "\xd7\xa4"},    // U+05E4
        {"tsade", "\xd7\xa6"},    // U+05E6
        {"qoph", "\xd7\xa7"},    // U+05E7
        {"resh", "\xd7\xa8"},    // U+05E8
        {"shin", "\xd7\xa9"},    // U+05E9
        {"tav", "\xd7\xaa"},    // U+05EA

        // === Double-Struck Digits ===
        {"bopf0", "\xf0\x9d\x9f\xac"},    // U+1D7EC
        {"bopf1", "\xf0\x9d\x9f\xad"},    // U+1D7ED
        {"bopf2", "\xf0\x9d\x9f\xae"},    // U+1D7EE
        {"bopf3", "\xf0\x9d\x9f\xaf"},    // U+1D7EF
        {"bopf4", "\xf0\x9d\x9f\xb0"},    // U+1D7F0
        {"bopf5", "\xf0\x9d\x9f\xb1"},    // U+1D7F1
        {"bopf6", "\xf0\x9d\x9f\xb2"},    // U+1D7F2
        {"bopf7", "\xf0\x9d\x9f\xb3"},    // U+1D7F3
        {"bopf8", "\xf0\x9d\x9f\xb4"},    // U+1D7F4
        {"bopf9", "\xf0\x9d\x9f\xb5"},    // U+1D7F5

        // === Geometric Shapes ===
        {"blacksquare", "\xe2\x96\xa0"},    // U+25A0
        {"whitesquare", "\xe2\x96\xa1"},    // U+25A1
        {"blackcircle", "\xe2\x97\x8f"},    // U+25CF
        {"whitecircle", "\xe2\x97\x8b"},    // U+25CB
        {"blacktriangle", "\xe2\x96\xb2"},    // U+25B2
        {"whitetriangle", "\xe2\x96\xb3"},    // U+25B3
        {"blacktriangledown", "\xe2\x96\xbc"},    // U+25BC
        {"whitetriangledown", "\xe2\x96\xbd"},    // U+25BD

        // === Additional Arrows ===
        {"uparrowbarred", "\xe2\xa4\x92"},    // U+2912
        {"downarrowbarred", "\xe2\xa4\x93"},    // U+2913
        {"arrowup", "\xe2\x86\x91"},    // U+2191
        {"arrowdown", "\xe2\x86\x93"},    // U+2193
        {"arrowleft", "\xe2\x86\x90"},    // U+2190
        {"arrowright", "\xe2\x86\x92"},    // U+2192

        // === Additional Math Symbols ===
        {"circlering", "\xe2\x8a\x9a"},    // U+229A
        {"circlemultiply", "\xe2\x8a\x97"},    // U+2297
        {"circleplus", "\xe2\x8a\x95"},    // U+2295
        {"circleminus", "\xe2\x8a\x96"},    // U+2296
        {"squareplus", "\xe2\x8a\x9e"},    // U+229E
        {"squareminus", "\xe2\x8a\x9f"},    // U+229F
        {"squaretimes", "\xe2\x8a\xa0"},    // U+22A0

        // === Additional Punctuation and Marks ===
        {"ellipsis", "\xe2\x80\xa6"},    // U+2026
        {"middledot", "\xc2\xb7"},    // U+00B7
        {"pilcrow", "\xc2\xb6"},    // U+00B6
        {"section", "\xc2\xa7"},    // U+00A7
        {"dagger", "\xe2\x80\xa0"},    // U+2020
        {"doubledagger", "\xe2\x80\xa1"},    // U+2021
        {"trademark", "\xe2\x84\xa2"},    // U+2122
        {"registered", "\xc2\xae"},    // U+00AE
        {"copyright", "\xc2\xa9"},    // U+00A9

        // === Temperature Symbols ===
        {"degC", "\xe2\x84\x83"},    // U+2103
        {"degF", "\xe2\x84\x89"},    // U+2109

        // === Music Notes ===
        {"musicalnote", "\xe2\x99\xaa"},    // U+266A
        {"twonotes", "\xe2\x99\xab"},    // U+266B

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
