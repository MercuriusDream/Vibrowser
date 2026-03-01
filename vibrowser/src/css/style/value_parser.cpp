#include <clever/css/style/style_resolver.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace clever::css {

namespace {

// Trim whitespace from both ends
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// Convert string to lowercase
std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

// Parse a hex digit
int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Named color table
const std::unordered_map<std::string, Color>& named_colors() {
    static const std::unordered_map<std::string, Color> colors = {
        {"black",       {0, 0, 0, 255}},
        {"white",       {255, 255, 255, 255}},
        {"red",         {255, 0, 0, 255}},
        {"green",       {0, 128, 0, 255}},
        {"blue",        {0, 0, 255, 255}},
        {"yellow",      {255, 255, 0, 255}},
        {"orange",      {255, 165, 0, 255}},
        {"purple",      {128, 0, 128, 255}},
        {"gray",        {128, 128, 128, 255}},
        {"grey",        {128, 128, 128, 255}},
        {"transparent", {0, 0, 0, 0}},
        {"cyan",        {0, 255, 255, 255}},
        {"magenta",     {255, 0, 255, 255}},
        {"lime",        {0, 255, 0, 255}},
        {"maroon",      {128, 0, 0, 255}},
        {"navy",        {0, 0, 128, 255}},
        {"olive",       {128, 128, 0, 255}},
        {"teal",        {0, 128, 128, 255}},
        {"silver",      {192, 192, 192, 255}},
        {"aqua",        {0, 255, 255, 255}},
        {"fuchsia",     {255, 0, 255, 255}},
    };
    return colors;
}

// ---- calc() expression parser ----

// Tokenize a calc expression string into a flat list of tokens
struct CalcToken {
    enum Type { Number, Plus, Minus, Star, Slash, LParen, RParen };
    Type type;
    float num_value = 0;
    Length::Unit num_unit = Length::Unit::Px;
};

// Try to parse a number+unit at position pos, advancing pos past it
bool parse_calc_number(const std::string& s, size_t& pos, float& val, Length::Unit& u) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;
    if (pos >= s.size()) return false;

    char* end = nullptr;
    float num = std::strtof(s.c_str() + pos, &end);
    if (end == s.c_str() + pos) return false;

    pos = static_cast<size_t>(end - s.c_str());

    // Skip whitespace between number and unit
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) pos++;

    // Try to read unit
    std::string unit_candidate;
    size_t unit_start = pos;
    while (pos < s.size() && std::isalpha(static_cast<unsigned char>(s[pos]))) {
        unit_candidate += static_cast<char>(std::tolower(static_cast<unsigned char>(s[pos])));
        pos++;
    }
    // Check for '%'
    if (pos < s.size() && s[pos] == '%') {
        unit_candidate = "%";
        pos++;
    }

    val = num;
    if (unit_candidate.empty() || unit_candidate == "px") {
        u = Length::Unit::Px;
    } else if (unit_candidate == "em") {
        u = Length::Unit::Em;
    } else if (unit_candidate == "rem") {
        u = Length::Unit::Rem;
    } else if (unit_candidate == "%") {
        u = Length::Unit::Percent;
    } else if (unit_candidate == "vw" || unit_candidate == "dvw" || unit_candidate == "svw" || unit_candidate == "lvw") {
        u = Length::Unit::Vw;
    } else if (unit_candidate == "vh" || unit_candidate == "dvh" || unit_candidate == "svh" || unit_candidate == "lvh") {
        u = Length::Unit::Vh;
    } else if (unit_candidate == "vmin") {
        u = Length::Unit::Vmin;
    } else if (unit_candidate == "vmax") {
        u = Length::Unit::Vmax;
    } else if (unit_candidate == "cqw") {
        u = Length::Unit::Cqw;
    } else if (unit_candidate == "cqh") {
        u = Length::Unit::Cqh;
    } else if (unit_candidate == "cqi") {
        u = Length::Unit::Cqi;
    } else if (unit_candidate == "cqb") {
        u = Length::Unit::Cqb;
    } else if (unit_candidate == "cqmin") {
        u = Length::Unit::Cqmin;
    } else if (unit_candidate == "cqmax") {
        u = Length::Unit::Cqmax;
    } else if (unit_candidate == "ch") {
        u = Length::Unit::Ch;
    } else if (unit_candidate == "lh") {
        u = Length::Unit::Lh;
    } else if (unit_candidate == "deg") {
        val = num * 3.14159265358979f / 180.0f;
        u = Length::Unit::Px;
    } else if (unit_candidate == "rad") {
        u = Length::Unit::Px;
    } else if (unit_candidate == "grad") {
        val = num * 3.14159265358979f / 200.0f;
        u = Length::Unit::Px;
    } else if (unit_candidate == "turn") {
        val = num * 2.0f * 3.14159265358979f;
        u = Length::Unit::Px;
    } else {
        // Unknown unit; rewind and treat as unitless (px)
        pos = unit_start;
        u = Length::Unit::Px;
    }
    return true;
}

// Recursion depth guard for mutually-recursive parse_math_arg ↔ tokenize_calc
static thread_local int s_calc_recursion_depth = 0;
static constexpr int kMaxCalcRecursion = 32;

// Forward declaration for function call evaluation inside calc tokenizer
std::shared_ptr<CalcExpr> parse_math_arg(const std::string& raw);

std::vector<CalcToken> tokenize_calc(const std::string& expr) {
    if (s_calc_recursion_depth >= kMaxCalcRecursion) return {};
    std::vector<CalcToken> tokens;
    size_t pos = 0;
    while (pos < expr.size()) {
        char c = expr[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            pos++;
            continue;
        }
        if (c == '(') {
            tokens.push_back({CalcToken::LParen});
            pos++;
        } else if (c == ')') {
            tokens.push_back({CalcToken::RParen});
            pos++;
        } else if (c == '+') {
            tokens.push_back({CalcToken::Plus});
            pos++;
        } else if (c == '*') {
            tokens.push_back({CalcToken::Star});
            pos++;
        } else if (c == '/') {
            tokens.push_back({CalcToken::Slash});
            pos++;
        } else if (c == '-') {
            // Determine if this is a unary minus (part of number) or binary minus
            // It's binary minus if the previous token is a Number or RParen
            bool is_binary = false;
            if (!tokens.empty()) {
                auto last = tokens.back().type;
                if (last == CalcToken::Number || last == CalcToken::RParen) {
                    is_binary = true;
                }
            }
            if (is_binary) {
                tokens.push_back({CalcToken::Minus});
                pos++;
            } else {
                // Unary minus — parse as negative number
                float val;
                Length::Unit u;
                if (parse_calc_number(expr, pos, val, u)) {
                    tokens.push_back({CalcToken::Number, val, u});
                } else {
                    pos++; // skip invalid char
                }
            }
        } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            float val;
            Length::Unit u;
            if (parse_calc_number(expr, pos, val, u)) {
                tokens.push_back({CalcToken::Number, val, u});
            } else {
                pos++; // skip
            }
        } else if (std::isalpha(static_cast<unsigned char>(c))) {
            // Check for CSS math constants and functions
            std::string word;
            size_t wstart = pos;
            while (pos < expr.size() && (std::isalpha(static_cast<unsigned char>(expr[pos])) || expr[pos] == '-'))
                word += static_cast<char>(std::tolower(static_cast<unsigned char>(expr[pos++])));
            if (word == "pi") {
                tokens.push_back({CalcToken::Number, 3.14159265358979f, Length::Unit::Px});
            } else if (word == "e" && (pos >= expr.size() || expr[pos] != 'x')) {
                tokens.push_back({CalcToken::Number, 2.71828182845905f, Length::Unit::Px});
            } else if (word == "infinity") {
                tokens.push_back({CalcToken::Number, std::numeric_limits<float>::infinity(), Length::Unit::Px});
            } else if (pos < expr.size() && expr[pos] == '(') {
                // This is a function call — extract function+args and evaluate via parse_math_arg
                std::string func_str = word + "(";
                int depth = 0;
                for (size_t i = pos; i < expr.size(); i++) {
                    func_str += expr[i];
                    if (expr[i] == '(') depth++;
                    else if (expr[i] == ')') { depth--; if (depth == 0) { pos = i + 1; break; } }
                }
                auto result = parse_math_arg(func_str);
                if (result) {
                    float val = result->evaluate(0, 16);
                    tokens.push_back({CalcToken::Number, val, Length::Unit::Px});
                }
            } else {
                pos = wstart + 1; // skip unknown char
            }
        } else {
            pos++; // skip unknown
        }
    }
    return tokens;
}

// Recursive descent parser for calc expressions
// Grammar:
//   expr       -> term (('+' | '-') term)*
//   term       -> factor (('*' | '/') factor)*
//   factor     -> NUMBER | '(' expr ')'
class CalcParser {
public:
    explicit CalcParser(const std::vector<CalcToken>& tokens) : tokens_(tokens), pos_(0) {}

    std::shared_ptr<CalcExpr> parse() {
        auto result = parse_expr();
        return result;
    }

private:
    const std::vector<CalcToken>& tokens_;
    size_t pos_;

    bool at_end() const { return pos_ >= tokens_.size(); }

    const CalcToken& current() const { return tokens_[pos_]; }

    std::shared_ptr<CalcExpr> parse_expr() {
        auto left = parse_term();
        if (!left) return nullptr;

        while (!at_end() && (current().type == CalcToken::Plus || current().type == CalcToken::Minus)) {
            auto op = (current().type == CalcToken::Plus) ? CalcExpr::Op::Add : CalcExpr::Op::Sub;
            pos_++;
            auto right = parse_term();
            if (!right) return left;
            left = CalcExpr::make_binary(op, left, right);
        }
        return left;
    }

    std::shared_ptr<CalcExpr> parse_term() {
        auto left = parse_factor();
        if (!left) return nullptr;

        while (!at_end() && (current().type == CalcToken::Star || current().type == CalcToken::Slash)) {
            auto op = (current().type == CalcToken::Star) ? CalcExpr::Op::Mul : CalcExpr::Op::Div;
            pos_++;
            auto right = parse_factor();
            if (!right) return left;
            left = CalcExpr::make_binary(op, left, right);
        }
        return left;
    }

    std::shared_ptr<CalcExpr> parse_factor() {
        if (at_end()) return nullptr;

        if (current().type == CalcToken::LParen) {
            pos_++; // consume '('
            auto inner = parse_expr();
            if (!at_end() && current().type == CalcToken::RParen) {
                pos_++; // consume ')'
            }
            return inner;
        }

        if (current().type == CalcToken::Number) {
            Length leaf;
            leaf.value = current().num_value;
            leaf.unit = current().num_unit;
            pos_++;
            return CalcExpr::make_value(leaf);
        }

        return nullptr;
    }
};

// Parse a calc() expression string and return a Length with Calc unit
std::optional<Length> parse_calc_expr(const std::string& inner) {
    auto tokens = tokenize_calc(inner);
    if (tokens.empty()) return std::nullopt;

    CalcParser parser(tokens);
    auto expr = parser.parse();
    if (!expr) return std::nullopt;

    return Length::calc(expr);
}

// Split a CSS function's inner arguments on commas, respecting nested parens.
// Falls back to splitting on spaces if no commas found (tokenizer strips commas).
std::vector<std::string> split_css_args(const std::string& inner) {
    std::vector<std::string> args;
    int depth = 0;
    size_t start = 0;
    bool has_commas = false;
    for (size_t i = 0; i < inner.size(); i++) {
        if (inner[i] == '(') depth++;
        else if (inner[i] == ')') depth--;
        else if (inner[i] == ',' && depth == 0) {
            has_commas = true;
            args.push_back(inner.substr(start, i - start));
            start = i + 1;
        }
    }
    args.push_back(inner.substr(start));

    // If no commas found, try splitting on spaces (tokenizer-reconstructed values)
    if (!has_commas && args.size() == 1) {
        args.clear();
        std::string s = inner;
        // Trim leading/trailing spaces
        while (!s.empty() && s.front() == ' ') s.erase(0, 1);
        while (!s.empty() && s.back() == ' ') s.pop_back();
        // Split on spaces respecting parens
        depth = 0;
        start = 0;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '(') depth++;
            else if (s[i] == ')') depth--;
            else if (s[i] == ' ' && depth == 0) {
                std::string token = s.substr(start, i - start);
                if (!token.empty()) args.push_back(token);
                start = i + 1;
            }
        }
        std::string last = s.substr(start);
        if (!last.empty()) args.push_back(last);
    }

    return args;
}

// Parse a single CSS math argument (which can itself be a calc expression)
std::shared_ptr<CalcExpr> parse_math_arg(const std::string& raw) {
    if (s_calc_recursion_depth >= kMaxCalcRecursion) return nullptr;
    ++s_calc_recursion_depth;
    struct DepthGuard { ~DepthGuard() { --s_calc_recursion_depth; } } guard;

    std::string arg = trim(raw);
    if (arg.empty()) return nullptr;

    // Check for nested min()/max()/calc()/clamp()
    std::string lower = to_lower(arg);
    auto extract_inner = [&](size_t prefix_len) -> std::string {
        size_t open = prefix_len;
        int depth = 0;
        size_t close = std::string::npos;
        for (size_t i = open; i < arg.size(); i++) {
            if (arg[i] == '(') depth++;
            else if (arg[i] == ')') { depth--; if (depth == 0) { close = i; break; } }
        }
        if (close != std::string::npos)
            return arg.substr(open + 1, close - open - 1);
        return arg.substr(open + 1);
    };

    if (lower.size() >= 5 && lower.substr(0, 4) == "min(") {
        auto inner_str = extract_inner(3);
        auto parts = split_css_args(inner_str);
        if (parts.empty()) return nullptr;
        auto result = parse_math_arg(parts[0]);
        for (size_t i = 1; i < parts.size(); i++) {
            auto rhs = parse_math_arg(parts[i]);
            if (rhs) result = CalcExpr::make_binary(CalcExpr::Op::Min, result, rhs);
        }
        return result;
    }
    if (lower.size() >= 5 && lower.substr(0, 4) == "max(") {
        auto inner_str = extract_inner(3);
        auto parts = split_css_args(inner_str);
        if (parts.empty()) return nullptr;
        auto result = parse_math_arg(parts[0]);
        for (size_t i = 1; i < parts.size(); i++) {
            auto rhs = parse_math_arg(parts[i]);
            if (rhs) result = CalcExpr::make_binary(CalcExpr::Op::Max, result, rhs);
        }
        return result;
    }
    if (lower.size() >= 7 && lower.substr(0, 6) == "clamp(") {
        auto inner_str = extract_inner(5);
        auto parts = split_css_args(inner_str);
        if (parts.size() < 3) return nullptr;
        auto min_arg = parse_math_arg(parts[0]);
        auto pref_arg = parse_math_arg(parts[1]);
        auto max_arg = parse_math_arg(parts[2]);
        if (!min_arg || !pref_arg || !max_arg) return nullptr;
        // clamp(min, pref, max) = max(min, min(pref, max))
        auto inner_min = CalcExpr::make_binary(CalcExpr::Op::Min, pref_arg, max_arg);
        return CalcExpr::make_binary(CalcExpr::Op::Max, min_arg, inner_min);
    }
    if (lower.size() >= 6 && lower.substr(0, 5) == "calc(") {
        auto inner_str = extract_inner(4);
        auto tokens = tokenize_calc(inner_str);
        if (tokens.empty()) return nullptr;
        CalcParser parser(tokens);
        return parser.parse();
    }
    // CSS abs() — unary absolute value
    if (lower.size() >= 5 && lower.substr(0, 4) == "abs(") {
        auto inner_str = extract_inner(3);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Abs, operand);
    }
    // CSS sign() — unary sign function (-1, 0, 1)
    if (lower.size() >= 6 && lower.substr(0, 5) == "sign(") {
        auto inner_str = extract_inner(4);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Sign, operand);
    }
    // CSS mod(a, b) — modulus with sign of divisor
    if (lower.size() >= 5 && lower.substr(0, 4) == "mod(") {
        auto inner_str = extract_inner(3);
        auto parts = split_css_args(inner_str);
        if (parts.size() < 2) return nullptr;
        auto lhs = parse_math_arg(parts[0]);
        auto rhs = parse_math_arg(parts[1]);
        if (!lhs || !rhs) return nullptr;
        return CalcExpr::make_binary(CalcExpr::Op::Mod, lhs, rhs);
    }
    // CSS rem(a, b) — remainder with sign of dividend
    if (lower.size() >= 5 && lower.substr(0, 4) == "rem(") {
        auto inner_str = extract_inner(3);
        auto parts = split_css_args(inner_str);
        if (parts.size() < 2) return nullptr;
        auto lhs = parse_math_arg(parts[0]);
        auto rhs = parse_math_arg(parts[1]);
        if (!lhs || !rhs) return nullptr;
        return CalcExpr::make_binary(CalcExpr::Op::Rem, lhs, rhs);
    }
    // CSS round([strategy,] value, interval)
    if (lower.size() >= 7 && lower.substr(0, 6) == "round(") {
        auto inner_str = extract_inner(5);
        auto parts = split_css_args(inner_str);
        if (parts.empty()) return nullptr;
        CalcExpr::Op round_op = CalcExpr::Op::RoundNearest;
        size_t val_idx = 0;
        // Check if first arg is a strategy keyword
        std::string first = trim(parts[0]);
        std::string first_lower = to_lower(first);
        if (first_lower == "nearest" || first_lower == "up" ||
            first_lower == "down" || first_lower == "to-zero") {
            if (first_lower == "up") round_op = CalcExpr::Op::RoundUp;
            else if (first_lower == "down") round_op = CalcExpr::Op::RoundDown;
            else if (first_lower == "to-zero") round_op = CalcExpr::Op::RoundToZero;
            val_idx = 1;
        }
        if (val_idx + 1 >= parts.size()) return nullptr;
        auto val_arg = parse_math_arg(parts[val_idx]);
        auto int_arg = parse_math_arg(parts[val_idx + 1]);
        if (!val_arg || !int_arg) return nullptr;
        return CalcExpr::make_binary(round_op, val_arg, int_arg);
    }

    // CSS trigonometric functions — sin(), cos(), tan() (unary, angle input)
    if (lower.size() >= 5 && lower.substr(0, 4) == "sin(") {
        auto inner_str = extract_inner(3);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Sin, operand);
    }
    if (lower.size() >= 5 && lower.substr(0, 4) == "cos(") {
        auto inner_str = extract_inner(3);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Cos, operand);
    }
    if (lower.size() >= 5 && lower.substr(0, 4) == "tan(") {
        auto inner_str = extract_inner(3);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Tan, operand);
    }
    // CSS inverse trig — asin(), acos(), atan() (unary, return radians)
    if (lower.size() >= 6 && lower.substr(0, 5) == "asin(") {
        auto inner_str = extract_inner(4);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Asin, operand);
    }
    if (lower.size() >= 6 && lower.substr(0, 5) == "acos(") {
        auto inner_str = extract_inner(4);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Acos, operand);
    }
    if (lower.size() >= 6 && lower.substr(0, 5) == "atan(") {
        auto inner_str = extract_inner(4);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Atan, operand);
    }
    // CSS atan2(y, x) — binary, returns radians
    if (lower.size() >= 7 && lower.substr(0, 6) == "atan2(") {
        auto inner_str = extract_inner(5);
        auto parts = split_css_args(inner_str);
        if (parts.size() < 2) return nullptr;
        auto lhs = parse_math_arg(parts[0]);
        auto rhs = parse_math_arg(parts[1]);
        if (!lhs || !rhs) return nullptr;
        return CalcExpr::make_binary(CalcExpr::Op::Atan2, lhs, rhs);
    }
    // CSS exponential functions — sqrt(), exp(), log() (unary)
    if (lower.size() >= 6 && lower.substr(0, 5) == "sqrt(") {
        auto inner_str = extract_inner(4);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Sqrt, operand);
    }
    if (lower.size() >= 5 && lower.substr(0, 4) == "exp(") {
        auto inner_str = extract_inner(3);
        auto operand = parse_math_arg(inner_str);
        if (!operand) return nullptr;
        return CalcExpr::make_unary(CalcExpr::Op::Exp, operand);
    }
    if (lower.size() >= 5 && lower.substr(0, 4) == "log(") {
        auto inner_str = extract_inner(3);
        auto parts = split_css_args(inner_str);
        if (parts.empty()) return nullptr;
        auto operand = parse_math_arg(parts[0]);
        if (!operand) return nullptr;
        if (parts.size() >= 2) {
            // log(value, base) = ln(value) / ln(base)
            auto base = parse_math_arg(parts[1]);
            if (!base) return CalcExpr::make_unary(CalcExpr::Op::Log, operand);
            // Implement as Log(value) / Log(base)
            auto log_val = CalcExpr::make_unary(CalcExpr::Op::Log, operand);
            auto log_base = CalcExpr::make_unary(CalcExpr::Op::Log, base);
            return CalcExpr::make_binary(CalcExpr::Op::Div, log_val, log_base);
        }
        return CalcExpr::make_unary(CalcExpr::Op::Log, operand);
    }
    // CSS pow(base, exponent) — binary
    if (lower.size() >= 5 && lower.substr(0, 4) == "pow(") {
        auto inner_str = extract_inner(3);
        auto parts = split_css_args(inner_str);
        if (parts.size() < 2) return nullptr;
        auto lhs = parse_math_arg(parts[0]);
        auto rhs = parse_math_arg(parts[1]);
        if (!lhs || !rhs) return nullptr;
        return CalcExpr::make_binary(CalcExpr::Op::Pow, lhs, rhs);
    }
    // CSS hypot(a, b, ...) — euclidean distance
    if (lower.size() >= 7 && lower.substr(0, 6) == "hypot(") {
        auto inner_str = extract_inner(5);
        auto parts = split_css_args(inner_str);
        if (parts.size() < 2) return nullptr;
        auto result = parse_math_arg(parts[0]);
        for (size_t i = 1; i < parts.size(); i++) {
            auto rhs = parse_math_arg(parts[i]);
            if (rhs) result = CalcExpr::make_binary(CalcExpr::Op::Hypot, result, rhs);
        }
        return result;
    }

    // Plain value — parse as a calc expression (supports "100% - 20px" etc.)
    auto tokens = tokenize_calc(arg);
    if (tokens.empty()) return nullptr;
    CalcParser parser(tokens);
    return parser.parse();
}

// Parse min()/max()/clamp() functions and return a Length with Calc unit
std::optional<Length> parse_math_func(const std::string& func_name, const std::string& inner) {
    if (func_name == "min" || func_name == "max") {
        auto args = split_css_args(inner);
        if (args.empty()) return std::nullopt;
        auto result = parse_math_arg(args[0]);
        if (!result) return std::nullopt;
        auto op = (func_name == "min") ? CalcExpr::Op::Min : CalcExpr::Op::Max;
        for (size_t i = 1; i < args.size(); i++) {
            auto rhs = parse_math_arg(args[i]);
            if (rhs) result = CalcExpr::make_binary(op, result, rhs);
        }
        return Length::calc(result);
    }
    if (func_name == "clamp") {
        auto args = split_css_args(inner);
        if (args.size() < 3) return std::nullopt;
        auto min_arg = parse_math_arg(args[0]);
        auto pref_arg = parse_math_arg(args[1]);
        auto max_arg = parse_math_arg(args[2]);
        if (!min_arg || !pref_arg || !max_arg) return std::nullopt;
        auto inner_min = CalcExpr::make_binary(CalcExpr::Op::Min, pref_arg, max_arg);
        auto result = CalcExpr::make_binary(CalcExpr::Op::Max, min_arg, inner_min);
        return Length::calc(result);
    }
    if (func_name == "abs") {
        auto operand = parse_math_arg(inner);
        if (!operand) return std::nullopt;
        return Length::calc(CalcExpr::make_unary(CalcExpr::Op::Abs, operand));
    }
    if (func_name == "sign") {
        auto operand = parse_math_arg(inner);
        if (!operand) return std::nullopt;
        return Length::calc(CalcExpr::make_unary(CalcExpr::Op::Sign, operand));
    }
    if (func_name == "mod") {
        auto args = split_css_args(inner);
        if (args.size() < 2) return std::nullopt;
        auto lhs = parse_math_arg(args[0]);
        auto rhs = parse_math_arg(args[1]);
        if (!lhs || !rhs) return std::nullopt;
        return Length::calc(CalcExpr::make_binary(CalcExpr::Op::Mod, lhs, rhs));
    }
    if (func_name == "rem") {
        auto args = split_css_args(inner);
        if (args.size() < 2) return std::nullopt;
        auto lhs = parse_math_arg(args[0]);
        auto rhs = parse_math_arg(args[1]);
        if (!lhs || !rhs) return std::nullopt;
        return Length::calc(CalcExpr::make_binary(CalcExpr::Op::Rem, lhs, rhs));
    }
    if (func_name == "round") {
        auto args = split_css_args(inner);
        if (args.empty()) return std::nullopt;
        CalcExpr::Op round_op = CalcExpr::Op::RoundNearest;
        size_t val_idx = 0;
        std::string first = trim(args[0]);
        std::string first_lower = to_lower(first);
        if (first_lower == "nearest" || first_lower == "up" ||
            first_lower == "down" || first_lower == "to-zero") {
            if (first_lower == "up") round_op = CalcExpr::Op::RoundUp;
            else if (first_lower == "down") round_op = CalcExpr::Op::RoundDown;
            else if (first_lower == "to-zero") round_op = CalcExpr::Op::RoundToZero;
            val_idx = 1;
        }
        if (val_idx + 1 >= args.size()) return std::nullopt;
        auto val_arg = parse_math_arg(args[val_idx]);
        auto int_arg = parse_math_arg(args[val_idx + 1]);
        if (!val_arg || !int_arg) return std::nullopt;
        return Length::calc(CalcExpr::make_binary(round_op, val_arg, int_arg));
    }
    // Trigonometric functions (unary)
    if (func_name == "sin" || func_name == "cos" || func_name == "tan" ||
        func_name == "asin" || func_name == "acos" || func_name == "atan" ||
        func_name == "sqrt" || func_name == "exp" || func_name == "log") {
        CalcExpr::Op op;
        if (func_name == "sin") op = CalcExpr::Op::Sin;
        else if (func_name == "cos") op = CalcExpr::Op::Cos;
        else if (func_name == "tan") op = CalcExpr::Op::Tan;
        else if (func_name == "asin") op = CalcExpr::Op::Asin;
        else if (func_name == "acos") op = CalcExpr::Op::Acos;
        else if (func_name == "atan") op = CalcExpr::Op::Atan;
        else if (func_name == "sqrt") op = CalcExpr::Op::Sqrt;
        else if (func_name == "exp") op = CalcExpr::Op::Exp;
        else op = CalcExpr::Op::Log;
        auto operand = parse_math_arg(inner);
        if (!operand) return std::nullopt;
        return Length::calc(CalcExpr::make_unary(op, operand));
    }
    // Binary math functions
    if (func_name == "pow" || func_name == "atan2" || func_name == "hypot") {
        auto args = split_css_args(inner);
        if (args.size() < 2) return std::nullopt;
        CalcExpr::Op op;
        if (func_name == "pow") op = CalcExpr::Op::Pow;
        else if (func_name == "atan2") op = CalcExpr::Op::Atan2;
        else op = CalcExpr::Op::Hypot;
        auto lhs = parse_math_arg(args[0]);
        auto rhs = parse_math_arg(args[1]);
        if (!lhs || !rhs) return std::nullopt;
        if (func_name == "hypot") {
            auto result = CalcExpr::make_binary(op, lhs, rhs);
            for (size_t i = 2; i < args.size(); i++) {
                auto next = parse_math_arg(args[i]);
                if (next) result = CalcExpr::make_binary(op, result, next);
            }
            return Length::calc(result);
        }
        return Length::calc(CalcExpr::make_binary(op, lhs, rhs));
    }
    return std::nullopt;
}

} // anonymous namespace

// Global dark mode flag for light-dark() CSS function
static bool g_dark_mode_flag = false;
static int g_dark_mode_override = -1; // -1 = no override

void set_dark_mode(bool dark) {
    g_dark_mode_flag = dark;
}

bool is_dark_mode() {
    if (g_dark_mode_override >= 0) return g_dark_mode_override != 0;
    return g_dark_mode_flag;
}

void set_dark_mode_override(int override_val) {
    g_dark_mode_override = override_val;
}

int get_dark_mode_override() {
    return g_dark_mode_override;
}

std::string component_value_to_string(const ComponentValue& cv);

std::string component_values_to_string(const std::vector<ComponentValue>& values) {
    std::string result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) result += ' ';
        result += component_value_to_string(values[i]);
    }
    return result;
}

std::string component_value_to_string(const ComponentValue& cv) {
    if (cv.type == ComponentValue::Function) {
        std::string result = cv.value;
        result += '(';
        for (size_t j = 0; j < cv.children.size(); ++j) {
            const auto& child = cv.children[j];
            if (child.type == ComponentValue::Token && child.value == ",") {
                result += ",";
                if (j + 1 < cv.children.size()) result += " ";
                continue;
            }
            if (j > 0) {
                const auto& prev = cv.children[j - 1];
                if (!(prev.type == ComponentValue::Token && prev.value == ",")) {
                    result += ' ';
                }
            }
            result += component_value_to_string(child);
        }
        result += ')';
        return result;
    }
    if (cv.type == ComponentValue::Block) {
        char close = ')';
        if (cv.value == "[") close = ']';
        else if (cv.value == "{") close = '}';

        std::string result = cv.value;
        for (size_t j = 0; j < cv.children.size(); ++j) {
            const auto& child = cv.children[j];
            if (child.type == ComponentValue::Token && child.value == ",") {
                result += ",";
                if (j + 1 < cv.children.size()) result += " ";
                continue;
            }
            if (j > 0) {
                const auto& prev = cv.children[j - 1];
                if (!(prev.type == ComponentValue::Token && prev.value == ",")) {
                    result += ' ';
                }
            }
            result += component_value_to_string(child);
        }
        result += close;
        return result;
    }
    return cv.value;
}

std::optional<Color> parse_color(const std::string& input) {
    std::string value = trim(to_lower(input));

    if (value.empty()) return std::nullopt;

    // Named colors
    auto& colors = named_colors();
    auto it = colors.find(value);
    if (it != colors.end()) {
        return it->second;
    }

    // Hex colors
    if (value[0] == '#') {
        std::string hex = value.substr(1);

        if (hex.length() == 3) {
            // #RGB -> #RRGGBB
            int r = hex_digit(hex[0]);
            int g = hex_digit(hex[1]);
            int b = hex_digit(hex[2]);
            if (r < 0 || g < 0 || b < 0) return std::nullopt;
            return Color{
                static_cast<uint8_t>(r * 17),
                static_cast<uint8_t>(g * 17),
                static_cast<uint8_t>(b * 17),
                255
            };
        }

        if (hex.length() == 4) {
            // #RGBA -> #RRGGBBAA
            int r = hex_digit(hex[0]);
            int g = hex_digit(hex[1]);
            int b = hex_digit(hex[2]);
            int a = hex_digit(hex[3]);
            if (r < 0 || g < 0 || b < 0 || a < 0) return std::nullopt;
            return Color{
                static_cast<uint8_t>(r * 17),
                static_cast<uint8_t>(g * 17),
                static_cast<uint8_t>(b * 17),
                static_cast<uint8_t>(a * 17)
            };
        }

        if (hex.length() == 6) {
            int r1 = hex_digit(hex[0]), r2 = hex_digit(hex[1]);
            int g1 = hex_digit(hex[2]), g2 = hex_digit(hex[3]);
            int b1 = hex_digit(hex[4]), b2 = hex_digit(hex[5]);
            if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) return std::nullopt;
            return Color{
                static_cast<uint8_t>(r1 * 16 + r2),
                static_cast<uint8_t>(g1 * 16 + g2),
                static_cast<uint8_t>(b1 * 16 + b2),
                255
            };
        }

        if (hex.length() == 8) {
            int r1 = hex_digit(hex[0]), r2 = hex_digit(hex[1]);
            int g1 = hex_digit(hex[2]), g2 = hex_digit(hex[3]);
            int b1 = hex_digit(hex[4]), b2 = hex_digit(hex[5]);
            int a1 = hex_digit(hex[6]), a2 = hex_digit(hex[7]);
            if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 ||
                b1 < 0 || b2 < 0 || a1 < 0 || a2 < 0) return std::nullopt;
            return Color{
                static_cast<uint8_t>(r1 * 16 + r2),
                static_cast<uint8_t>(g1 * 16 + g2),
                static_cast<uint8_t>(b1 * 16 + b2),
                static_cast<uint8_t>(a1 * 16 + a2)
            };
        }

        return std::nullopt;
    }

    // currentcolor keyword — resolved at use-site; represent as sentinel
    if (value == "currentcolor") {
        // Return a special sentinel color (r=0,g=0,b=0,a=1) that callers
        // can check for. In practice this is the same as black, which is the
        // most common currentColor resolution (text color defaults to black).
        return Color{0, 0, 0, 255};
    }

    // Helper: extract content between parentheses
    auto extract_func_args = [](const std::string& v) -> std::optional<std::string> {
        auto open = v.find('(');
        auto close = v.rfind(')');
        if (open == std::string::npos || close == std::string::npos || close <= open)
            return std::nullopt;
        return v.substr(open + 1, close - open - 1);
    };

    // Helper: parse comma/space/slash-separated numeric values from function args
    auto parse_func_values = [](const std::string& content, float* out, int max_count) -> int {
        std::string cleaned = content;
        for (auto& c : cleaned) {
            if (c == ',') c = ' ';
            if (c == '/') c = ' ';
            if (c == '%') c = ' ';
        }
        // Strip "deg" "turn" "rad" "grad" suffixes
        for (const auto& suffix : {"deg", "turn", "rad", "grad"}) {
            size_t pos;
            while ((pos = cleaned.find(suffix)) != std::string::npos) {
                cleaned.replace(pos, strlen(suffix), " ");
            }
        }
        std::istringstream iss(cleaned);
        int count = 0;
        float v;
        while (iss >> v && count < max_count) {
            out[count++] = v;
        }
        return count;
    };

    // rgb() and rgba() functions (including relative color syntax)
    if (value.substr(0, 4) == "rgb(" || value.substr(0, 5) == "rgba(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        std::string args_lower = to_lower(*args);
        if (args_lower.substr(0, 5) == "from ") {
            // Relative color syntax: rgb(from <ref-color> <r-expr> <g-expr> <b-expr> [/ <alpha-expr>])
            size_t from_pos = args_lower.find("from ");
            if (from_pos == std::string::npos) return std::nullopt;

            std::string rest = args->substr(from_pos + 5);
            rest = trim(rest);

            // Find the reference color (first token before space or paren)
            size_t ref_end = 0;
            int paren_depth = 0;
            for (size_t i = 0; i < rest.size(); i++) {
                if (rest[i] == '(') paren_depth++;
                else if (rest[i] == ')') paren_depth--;
                else if ((rest[i] == ' ' || rest[i] == '/') && paren_depth == 0) {
                    ref_end = i;
                    break;
                }
            }
            if (ref_end == 0) ref_end = rest.size();

            std::string ref_color_str = trim(rest.substr(0, ref_end));
            auto ref_color = parse_color(ref_color_str);
            if (!ref_color) return std::nullopt;

            // Extract channel expressions
            std::string channel_str = trim(rest.substr(ref_end));

            float ref_r = ref_color->r, ref_g = ref_color->g, ref_b = ref_color->b;
            float ref_a = ref_color->a / 255.0f;

            // Parse channels: space/comma/slash separated
            std::vector<std::string> channel_tokens;
            std::string current_token;
            for (char c : channel_str) {
                if (c == ' ' || c == ',' || c == '/') {
                    if (!current_token.empty()) {
                        channel_tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
            }
            if (!current_token.empty()) {
                channel_tokens.push_back(current_token);
            }

            float out_r = 0, out_g = 0, out_b = 0, out_a = 255;

            for (size_t i = 0; i < channel_tokens.size(); i++) {
                std::string tok = to_lower(channel_tokens[i]);
                tok = trim(tok);
                if (tok.empty()) continue;

                float* target = nullptr;
                if (i == 0) target = &out_r;
                else if (i == 1) target = &out_g;
                else if (i == 2) target = &out_b;
                else if (i == 3) target = &out_a;

                if (!target) break;

                if (tok == "r") {
                    *target = ref_r;
                } else if (tok == "g") {
                    *target = ref_g;
                } else if (tok == "b") {
                    *target = ref_b;
                } else if (tok == "alpha") {
                    *target = ref_a * 255.0f;
                } else {
                    // Try to parse as number with optional %
                    try {
                        if (tok.back() == '%') {
                            float pct = std::stof(tok.substr(0, tok.size() - 1));
                            *target = (pct / 100.0f) * 255.0f;
                        } else {
                            *target = std::stof(tok);
                        }
                    } catch (...) {
                        *target = 0;
                    }
                }
            }

            return Color{
                static_cast<uint8_t>(std::clamp(out_r, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(out_g, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(out_b, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(out_a, 0.0f, 255.0f))
            };
        }

        float vals[4] = {0, 0, 0, 255};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        // If alpha is provided as 0-1 range (for rgba)
        if (count == 4 && vals[3] <= 1.0f) {
            vals[3] = vals[3] * 255.0f;
        }

        return Color{
            static_cast<uint8_t>(std::clamp(vals[0], 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(vals[1], 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(vals[2], 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(vals[3], 0.0f, 255.0f))
        };
    }

    // hsl() and hsla() functions (including relative color syntax)
    // hsl(hue, saturation%, lightness% [, alpha])
    if (value.substr(0, 4) == "hsl(" || value.substr(0, 5) == "hsla(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        std::string args_lower = to_lower(*args);
        if (args_lower.substr(0, 5) == "from ") {
            // Relative color syntax: hsl(from <ref-color> <h-expr> <s-expr> <l-expr> [/ <alpha-expr>])
            size_t from_pos = args_lower.find("from ");
            if (from_pos == std::string::npos) return std::nullopt;

            std::string rest = args->substr(from_pos + 5);
            rest = trim(rest);

            // Find the reference color
            size_t ref_end = 0;
            int paren_depth = 0;
            for (size_t i = 0; i < rest.size(); i++) {
                if (rest[i] == '(') paren_depth++;
                else if (rest[i] == ')') paren_depth--;
                else if ((rest[i] == ' ' || rest[i] == '/') && paren_depth == 0) {
                    ref_end = i;
                    break;
                }
            }
            if (ref_end == 0) ref_end = rest.size();

            std::string ref_color_str = trim(rest.substr(0, ref_end));
            auto ref_color = parse_color(ref_color_str);
            if (!ref_color) return std::nullopt;

            // Decompose reference color to HSL
            float r = ref_color->r / 255.0f, g = ref_color->g / 255.0f, b = ref_color->b / 255.0f;
            float max_c = std::max({r, g, b});
            float min_c = std::min({r, g, b});
            float delta = max_c - min_c;
            float ref_h = 0, ref_s = 0, ref_l = (max_c + min_c) / 2.0f;

            if (delta != 0) {
                if (max_c == r) ref_h = std::fmod(60.0f * ((g - b) / delta) + 360.0f, 360.0f);
                else if (max_c == g) ref_h = std::fmod(60.0f * ((b - r) / delta) + 120.0f, 360.0f);
                else ref_h = std::fmod(60.0f * ((r - g) / delta) + 240.0f, 360.0f);
                ref_s = delta / (1.0f - std::abs(2.0f * ref_l - 1.0f));
            }
            float ref_a = ref_color->a / 255.0f;

            // Parse channels
            std::string channel_str = trim(rest.substr(ref_end));
            std::vector<std::string> channel_tokens;
            std::string current_token;
            for (char c : channel_str) {
                if (c == ' ' || c == ',' || c == '/') {
                    if (!current_token.empty()) {
                        channel_tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
            }
            if (!current_token.empty()) {
                channel_tokens.push_back(current_token);
            }

            float out_h = 0, out_s = 0, out_l = 0, out_a = 1.0f;

            for (size_t i = 0; i < channel_tokens.size(); i++) {
                std::string tok = to_lower(channel_tokens[i]);
                tok = trim(tok);
                if (tok.empty()) continue;

                float* target = nullptr;
                if (i == 0) target = &out_h;
                else if (i == 1) target = &out_s;
                else if (i == 2) target = &out_l;
                else if (i == 3) target = &out_a;

                if (!target) break;

                if (tok == "h") {
                    *target = ref_h;
                } else if (tok == "s") {
                    *target = ref_s * 100.0f;
                } else if (tok == "l") {
                    *target = ref_l * 100.0f;
                } else if (tok == "alpha") {
                    *target = ref_a;
                } else {
                    // Try to parse as number with optional %
                    try {
                        if (tok.back() == '%') {
                            *target = std::stof(tok.substr(0, tok.size() - 1));
                        } else {
                            *target = std::stof(tok);
                        }
                    } catch (...) {
                        *target = 0;
                    }
                }
            }

            // Convert HSL to RGB
            float h = std::fmod(out_h, 360.0f);
            if (h < 0) h += 360.0f;
            float s = std::clamp(out_s, 0.0f, 100.0f) / 100.0f;
            float l = std::clamp(out_l, 0.0f, 100.0f) / 100.0f;
            float a = std::clamp(out_a, 0.0f, 1.0f);

            auto hue2rgb = [](float p, float q, float t) -> float {
                if (t < 0) t += 1;
                if (t > 1) t -= 1;
                if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
                if (t < 1.0f / 2.0f) return q;
                if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
                return p;
            };

            float r_out, g_out, b_out;
            if (s == 0) {
                r_out = g_out = b_out = l;
            } else {
                float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
                float p = 2 * l - q;
                r_out = hue2rgb(p, q, h / 360.0f + 1.0f / 3.0f);
                g_out = hue2rgb(p, q, h / 360.0f);
                b_out = hue2rgb(p, q, h / 360.0f - 1.0f / 3.0f);
            }

            return Color{
                static_cast<uint8_t>(std::clamp(r_out * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g_out * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b_out * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(a * 255.0f, 0.0f, 255.0f))
            };
        }

        float vals[4] = {0, 0, 0, 1.0f};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        float h = std::fmod(vals[0], 360.0f);
        if (h < 0) h += 360.0f;
        float s = std::clamp(vals[1], 0.0f, 100.0f) / 100.0f;
        float l = std::clamp(vals[2], 0.0f, 100.0f) / 100.0f;
        float a = (count >= 4) ? std::clamp(vals[3], 0.0f, 1.0f) : 1.0f;

        // HSL to RGB conversion
        auto hue2rgb = [](float p, float q, float t) -> float {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
            if (t < 1.0f / 2.0f) return q;
            if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
            return p;
        };

        float r, g, b;
        if (s == 0) {
            r = g = b = l;
        } else {
            float q = l < 0.5f ? l * (1 + s) : l + s - l * s;
            float p = 2 * l - q;
            r = hue2rgb(p, q, h / 360.0f + 1.0f / 3.0f);
            g = hue2rgb(p, q, h / 360.0f);
            b = hue2rgb(p, q, h / 360.0f - 1.0f / 3.0f);
        }

        return Color{
            static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(a * 255.0f, 0.0f, 255.0f))
        };
    }

    // oklch() function — CSS Color Level 4 (including relative color syntax)
    // oklch(lightness chroma hue [/ alpha])
    if (value.substr(0, 6) == "oklch(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        std::string args_lower = to_lower(*args);
        if (args_lower.substr(0, 5) == "from ") {
            // Relative color syntax: oklch(from <ref-color> <l-expr> <c-expr> <h-expr> [/ <alpha-expr>])
            size_t from_pos = args_lower.find("from ");
            if (from_pos == std::string::npos) return std::nullopt;

            std::string rest = args->substr(from_pos + 5);
            rest = trim(rest);

            // Find the reference color
            size_t ref_end = 0;
            int paren_depth = 0;
            for (size_t i = 0; i < rest.size(); i++) {
                if (rest[i] == '(') paren_depth++;
                else if (rest[i] == ')') paren_depth--;
                else if ((rest[i] == ' ' || rest[i] == '/') && paren_depth == 0) {
                    ref_end = i;
                    break;
                }
            }
            if (ref_end == 0) ref_end = rest.size();

            std::string ref_color_str = trim(rest.substr(0, ref_end));
            auto ref_color = parse_color(ref_color_str);
            if (!ref_color) return std::nullopt;

            // Decompose reference color to OKLch (via sRGB → OKLab)
            float r = ref_color->r / 255.0f, g = ref_color->g / 255.0f, b = ref_color->b / 255.0f;

            auto srgb_to_linear = [](float c) -> float {
                if (c <= 0.04045f) return c / 12.92f;
                return std::pow((c + 0.055f) / 1.055f, 2.4f);
            };
            float r_lin = srgb_to_linear(r);
            float g_lin = srgb_to_linear(g);
            float b_lin = srgb_to_linear(b);

            float l_val = 0.4122214708f * r_lin + 0.5363325363f * g_lin + 0.0514459929f * b_lin;
            float m_val = 0.2119034982f * r_lin + 0.6806995451f * g_lin + 0.1073969566f * b_lin;
            float s_val = 0.0883024619f * r_lin + 0.0817845529f * g_lin + 0.8943868922f * b_lin;

            float l_cbrt = std::cbrt(l_val), m_cbrt = std::cbrt(m_val), s_cbrt = std::cbrt(s_val);

            float ref_L = 0.2104542553f * l_cbrt + 0.7936177850f * m_cbrt - 0.0040720468f * s_cbrt;
            float ref_a = 1.9779984951f * l_cbrt - 2.4285922050f * m_cbrt + 0.4505937099f * s_cbrt;
            float ref_b = 0.0259040371f * l_cbrt + 0.7827717662f * m_cbrt - 0.8086757660f * s_cbrt;

            float ref_C = std::sqrt(ref_a * ref_a + ref_b * ref_b);
            float ref_h = 0;
            if (ref_C != 0) {
                ref_h = std::atan2(ref_b, ref_a) * 180.0f / 3.14159265f;
                if (ref_h < 0) ref_h += 360.0f;
            }
            float ref_a_alpha = ref_color->a / 255.0f;

            // Parse channels
            std::string channel_str = trim(rest.substr(ref_end));
            std::vector<std::string> channel_tokens;
            std::string current_token;
            for (char c : channel_str) {
                if (c == ' ' || c == ',' || c == '/') {
                    if (!current_token.empty()) {
                        channel_tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
            }
            if (!current_token.empty()) {
                channel_tokens.push_back(current_token);
            }

            float out_L = 0, out_C = 0, out_h = 0, out_alpha = 1.0f;

            for (size_t i = 0; i < channel_tokens.size(); i++) {
                std::string tok = to_lower(channel_tokens[i]);
                tok = trim(tok);
                if (tok.empty()) continue;

                float* target = nullptr;
                if (i == 0) target = &out_L;
                else if (i == 1) target = &out_C;
                else if (i == 2) target = &out_h;
                else if (i == 3) target = &out_alpha;

                if (!target) break;

                if (tok == "l") {
                    *target = ref_L;
                } else if (tok == "c") {
                    *target = ref_C;
                } else if (tok == "h") {
                    *target = ref_h;
                } else if (tok == "alpha") {
                    *target = ref_a_alpha;
                } else {
                    // Try to parse as number with optional %
                    try {
                        if (tok.back() == '%') {
                            *target = std::stof(tok.substr(0, tok.size() - 1));
                        } else {
                            *target = std::stof(tok);
                        }
                    } catch (...) {
                        *target = 0;
                    }
                }
            }

            // Convert OKLch to OKLab to sRGB
            float h_rad = out_h * 3.14159265f / 180.0f;
            float a_lab = out_C * std::cos(h_rad);
            float b_lab = out_C * std::sin(h_rad);

            float l_ = out_L + 0.3963377774f * a_lab + 0.2158037573f * b_lab;
            float m_ = out_L - 0.1055613458f * a_lab - 0.0638541728f * b_lab;
            float s_ = out_L - 0.0894841775f * a_lab - 1.2914855480f * b_lab;

            float l3 = l_ * l_ * l_, m3 = m_ * m_ * m_, s3 = s_ * s_ * s_;

            float r_lin_out = +4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
            float g_lin_out = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
            float b_lin_out = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;

            auto linear_to_srgb = [](float c) -> float {
                if (c <= 0.0031308f) return 12.92f * c;
                return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
            };

            return Color{
                static_cast<uint8_t>(std::clamp(linear_to_srgb(r_lin_out) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(g_lin_out) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(b_lin_out) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(out_alpha * 255.0f, 0.0f, 255.0f))
            };
        }

        float vals[4] = {0, 0, 0, 1.0f};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        float L = std::clamp(vals[0], 0.0f, 1.0f);
        float C = std::clamp(vals[1], 0.0f, 0.4f);
        float h = std::fmod(vals[2], 360.0f);
        if (h < 0) h += 360.0f;
        float alpha = (count >= 4) ? std::clamp(vals[3], 0.0f, 1.0f) : 1.0f;

        // OKLCh → OKLab
        float h_rad = h * 3.14159265f / 180.0f;
        float a_lab = C * std::cos(h_rad);
        float b_lab = C * std::sin(h_rad);

        // OKLab → linear sRGB (via LMS)
        float l_ = L + 0.3963377774f * a_lab + 0.2158037573f * b_lab;
        float m_ = L - 0.1055613458f * a_lab - 0.0638541728f * b_lab;
        float s_ = L - 0.0894841775f * a_lab - 1.2914855480f * b_lab;

        float l3 = l_ * l_ * l_;
        float m3 = m_ * m_ * m_;
        float s3 = s_ * s_ * s_;

        float r_lin = +4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
        float g_lin = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
        float b_lin = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;

        // Linear sRGB → sRGB (gamma)
        auto linear_to_srgb = [](float c) -> float {
            if (c <= 0.0031308f) return 12.92f * c;
            return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        };

        return Color{
            static_cast<uint8_t>(std::clamp(linear_to_srgb(r_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(g_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(b_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))
        };
    }

    // oklab() function — CSS Color Level 4 (including relative color syntax)
    // oklab(lightness a-axis b-axis [/ alpha])
    if (value.substr(0, 6) == "oklab(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        std::string args_lower = to_lower(*args);
        if (args_lower.substr(0, 5) == "from ") {
            // Relative color syntax: oklab(from <ref-color> <l-expr> <a-expr> <b-expr> [/ <alpha-expr>])
            size_t from_pos = args_lower.find("from ");
            if (from_pos == std::string::npos) return std::nullopt;

            std::string rest = args->substr(from_pos + 5);
            rest = trim(rest);

            // Find the reference color
            size_t ref_end = 0;
            int paren_depth = 0;
            for (size_t i = 0; i < rest.size(); i++) {
                if (rest[i] == '(') paren_depth++;
                else if (rest[i] == ')') paren_depth--;
                else if ((rest[i] == ' ' || rest[i] == '/') && paren_depth == 0) {
                    ref_end = i;
                    break;
                }
            }
            if (ref_end == 0) ref_end = rest.size();

            std::string ref_color_str = trim(rest.substr(0, ref_end));
            auto ref_color = parse_color(ref_color_str);
            if (!ref_color) return std::nullopt;

            // Decompose reference color to OKLab (sRGB → OKLab)
            float r = ref_color->r / 255.0f, g = ref_color->g / 255.0f, b = ref_color->b / 255.0f;

            auto srgb_to_linear = [](float c) -> float {
                if (c <= 0.04045f) return c / 12.92f;
                return std::pow((c + 0.055f) / 1.055f, 2.4f);
            };
            float r_lin = srgb_to_linear(r);
            float g_lin = srgb_to_linear(g);
            float b_lin = srgb_to_linear(b);

            float l_val = 0.4122214708f * r_lin + 0.5363325363f * g_lin + 0.0514459929f * b_lin;
            float m_val = 0.2119034982f * r_lin + 0.6806995451f * g_lin + 0.1073969566f * b_lin;
            float s_val = 0.0883024619f * r_lin + 0.0817845529f * g_lin + 0.8943868922f * b_lin;

            float l_cbrt = std::cbrt(l_val), m_cbrt = std::cbrt(m_val), s_cbrt = std::cbrt(s_val);

            float ref_L = 0.2104542553f * l_cbrt + 0.7936177850f * m_cbrt - 0.0040720468f * s_cbrt;
            float ref_a = 1.9779984951f * l_cbrt - 2.4285922050f * m_cbrt + 0.4505937099f * s_cbrt;
            float ref_b = 0.0259040371f * l_cbrt + 0.7827717662f * m_cbrt - 0.8086757660f * s_cbrt;
            float ref_a_alpha = ref_color->a / 255.0f;

            // Parse channels
            std::string channel_str = trim(rest.substr(ref_end));
            std::vector<std::string> channel_tokens;
            std::string current_token;
            for (char c : channel_str) {
                if (c == ' ' || c == ',' || c == '/') {
                    if (!current_token.empty()) {
                        channel_tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
            }
            if (!current_token.empty()) {
                channel_tokens.push_back(current_token);
            }

            float out_L = 0, out_a = 0, out_b = 0, out_alpha = 1.0f;

            for (size_t i = 0; i < channel_tokens.size(); i++) {
                std::string tok = to_lower(channel_tokens[i]);
                tok = trim(tok);
                if (tok.empty()) continue;

                float* target = nullptr;
                if (i == 0) target = &out_L;
                else if (i == 1) target = &out_a;
                else if (i == 2) target = &out_b;
                else if (i == 3) target = &out_alpha;

                if (!target) break;

                if (tok == "l") {
                    *target = ref_L;
                } else if (tok == "a") {
                    *target = ref_a;
                } else if (tok == "b") {
                    *target = ref_b;
                } else if (tok == "alpha") {
                    *target = ref_a_alpha;
                } else {
                    // Try to parse as number with optional %
                    try {
                        if (tok.back() == '%') {
                            *target = std::stof(tok.substr(0, tok.size() - 1));
                        } else {
                            *target = std::stof(tok);
                        }
                    } catch (...) {
                        *target = 0;
                    }
                }
            }

            // Convert OKLab to sRGB
            float l_ = out_L + 0.3963377774f * out_a + 0.2158037573f * out_b;
            float m_ = out_L - 0.1055613458f * out_a - 0.0638541728f * out_b;
            float s_ = out_L - 0.0894841775f * out_a - 1.2914855480f * out_b;

            float l3 = l_ * l_ * l_, m3 = m_ * m_ * m_, s3 = s_ * s_ * s_;

            float r_lin_out = +4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
            float g_lin_out = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
            float b_lin_out = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;

            auto linear_to_srgb = [](float c) -> float {
                if (c <= 0.0031308f) return 12.92f * c;
                return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
            };

            return Color{
                static_cast<uint8_t>(std::clamp(linear_to_srgb(r_lin_out) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(g_lin_out) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(b_lin_out) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(out_alpha * 255.0f, 0.0f, 255.0f))
            };
        }

        float vals[4] = {0, 0, 0, 1.0f};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        float L = std::clamp(vals[0], 0.0f, 1.0f);
        float a_lab = vals[1]; // typically -0.4 to 0.4
        float b_lab = vals[2];
        float alpha = (count >= 4) ? std::clamp(vals[3], 0.0f, 1.0f) : 1.0f;

        // OKLab → linear sRGB (via LMS)
        float l_ = L + 0.3963377774f * a_lab + 0.2158037573f * b_lab;
        float m_ = L - 0.1055613458f * a_lab - 0.0638541728f * b_lab;
        float s_ = L - 0.0894841775f * a_lab - 1.2914855480f * b_lab;

        float l3 = l_ * l_ * l_;
        float m3 = m_ * m_ * m_;
        float s3 = s_ * s_ * s_;

        float r_lin = +4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
        float g_lin = -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
        float b_lin = -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;

        auto linear_to_srgb = [](float c) -> float {
            if (c <= 0.0031308f) return 12.92f * c;
            return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        };

        return Color{
            static_cast<uint8_t>(std::clamp(linear_to_srgb(r_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(g_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(b_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))
        };
    }

    // hwb() function — CSS Color Level 4 (including relative color syntax)
    // hwb(hue whiteness% blackness% [/ alpha])
    if (value.substr(0, 4) == "hwb(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        std::string args_lower = to_lower(*args);
        if (args_lower.substr(0, 5) == "from ") {
            // Relative color syntax: hwb(from <ref-color> <h-expr> <w-expr> <b-expr> [/ <alpha-expr>])
            size_t from_pos = args_lower.find("from ");
            if (from_pos == std::string::npos) return std::nullopt;

            std::string rest = args->substr(from_pos + 5);
            rest = trim(rest);

            // Find the reference color
            size_t ref_end = 0;
            int paren_depth = 0;
            for (size_t i = 0; i < rest.size(); i++) {
                if (rest[i] == '(') paren_depth++;
                else if (rest[i] == ')') paren_depth--;
                else if ((rest[i] == ' ' || rest[i] == '/') && paren_depth == 0) {
                    ref_end = i;
                    break;
                }
            }
            if (ref_end == 0) ref_end = rest.size();

            std::string ref_color_str = trim(rest.substr(0, ref_end));
            auto ref_color = parse_color(ref_color_str);
            if (!ref_color) return std::nullopt;

            // Decompose reference color to HWB
            float r = ref_color->r / 255.0f, g = ref_color->g / 255.0f, b = ref_color->b / 255.0f;
            float max_c = std::max({r, g, b});
            float min_c = std::min({r, g, b});
            float ref_h = 0;

            if (max_c != min_c) {
                if (max_c == r) ref_h = std::fmod(60.0f * ((g - b) / (max_c - min_c)) + 360.0f, 360.0f);
                else if (max_c == g) ref_h = 60.0f * ((b - r) / (max_c - min_c)) + 120.0f;
                else ref_h = 60.0f * ((r - g) / (max_c - min_c)) + 240.0f;
            }
            float ref_w = min_c;
            float ref_b = 1.0f - max_c;
            float ref_a = ref_color->a / 255.0f;

            // Parse channels
            std::string channel_str = trim(rest.substr(ref_end));
            std::vector<std::string> channel_tokens;
            std::string current_token;
            for (char c : channel_str) {
                if (c == ' ' || c == ',' || c == '/') {
                    if (!current_token.empty()) {
                        channel_tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
            }
            if (!current_token.empty()) {
                channel_tokens.push_back(current_token);
            }

            float out_h = 0, out_w = 0, out_b = 0, out_a = 1.0f;

            for (size_t i = 0; i < channel_tokens.size(); i++) {
                std::string tok = to_lower(channel_tokens[i]);
                tok = trim(tok);
                if (tok.empty()) continue;

                float* target = nullptr;
                if (i == 0) target = &out_h;
                else if (i == 1) target = &out_w;
                else if (i == 2) target = &out_b;
                else if (i == 3) target = &out_a;

                if (!target) break;

                if (tok == "h") {
                    *target = ref_h;
                } else if (tok == "w") {
                    *target = ref_w * 100.0f;
                } else if (tok == "b") {
                    *target = ref_b * 100.0f;
                } else if (tok == "alpha") {
                    *target = ref_a;
                } else {
                    // Try to parse as number with optional %
                    try {
                        if (tok.back() == '%') {
                            *target = std::stof(tok.substr(0, tok.size() - 1));
                        } else {
                            *target = std::stof(tok);
                        }
                    } catch (...) {
                        *target = 0;
                    }
                }
            }

            // Convert HWB to RGB
            float h = std::fmod(out_h, 360.0f);
            if (h < 0) h += 360.0f;
            float w = std::clamp(out_w, 0.0f, 100.0f) / 100.0f;
            float b_val = std::clamp(out_b, 0.0f, 100.0f) / 100.0f;
            float alpha = std::clamp(out_a, 0.0f, 1.0f);

            if (w + b_val > 1) {
                float sum = w + b_val;
                w /= sum;
                b_val /= sum;
            }

            auto hue2rgb = [](float p, float q, float t) -> float {
                if (t < 0) t += 1;
                if (t > 1) t -= 1;
                if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
                if (t < 1.0f / 2.0f) return q;
                if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
                return p;
            };

            float r_pure = hue2rgb(0, 1, h / 360.0f + 1.0f / 3.0f);
            float g_pure = hue2rgb(0, 1, h / 360.0f);
            float b_pure = hue2rgb(0, 1, h / 360.0f - 1.0f / 3.0f);

            float r_out = r_pure * (1 - w - b_val) + w;
            float g_out = g_pure * (1 - w - b_val) + w;
            float b_out = b_pure * (1 - w - b_val) + w;

            return Color{
                static_cast<uint8_t>(std::clamp(r_out * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(g_out * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(b_out * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))
            };
        }

        float vals[4] = {0, 0, 0, 1.0f};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        float h = std::fmod(vals[0], 360.0f);
        if (h < 0) h += 360.0f;
        float w = std::clamp(vals[1], 0.0f, 100.0f) / 100.0f;
        float b = std::clamp(vals[2], 0.0f, 100.0f) / 100.0f;
        float alpha = (count >= 4) ? std::clamp(vals[3], 0.0f, 1.0f) : 1.0f;

        // Normalize if w + b > 1
        if (w + b > 1) {
            float sum = w + b;
            w /= sum;
            b /= sum;
        }

        // HWB → RGB: first get the pure hue color via HSL with S=100% L=50%
        auto hue2rgb = [](float p, float q, float t) -> float {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
            if (t < 1.0f / 2.0f) return q;
            if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
            return p;
        };

        float r_pure = hue2rgb(0, 1, h / 360.0f + 1.0f / 3.0f);
        float g_pure = hue2rgb(0, 1, h / 360.0f);
        float b_pure = hue2rgb(0, 1, h / 360.0f - 1.0f / 3.0f);

        // Apply whiteness and blackness
        float r = r_pure * (1 - w - b) + w;
        float g = g_pure * (1 - w - b) + w;
        float bl = b_pure * (1 - w - b) + w;

        return Color{
            static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(bl * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))
        };
    }

    // lab() function — CSS Color Level 4 (CIE Lab, D65 illuminant)
    // lab(lightness a-axis b-axis [/ alpha])
    if (value.substr(0, 4) == "lab(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        float vals[4] = {0, 0, 0, 1.0f};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        float L = std::clamp(vals[0], 0.0f, 100.0f);
        float a_lab = vals[1]; // typically -125 to 125
        float b_lab = vals[2];
        float alpha = (count >= 4) ? std::clamp(vals[3], 0.0f, 1.0f) : 1.0f;

        // CIE Lab → CIE XYZ (D65 white point)
        float fy = (L + 16.0f) / 116.0f;
        float fx = a_lab / 500.0f + fy;
        float fz = fy - b_lab / 200.0f;

        auto lab_f_inv = [](float t) -> float {
            float delta = 6.0f / 29.0f;
            if (t > delta) return t * t * t;
            return 3.0f * delta * delta * (t - 4.0f / 29.0f);
        };

        // D65 white point
        float X = 0.95047f * lab_f_inv(fx);
        float Y = 1.00000f * lab_f_inv(fy);
        float Z = 1.08883f * lab_f_inv(fz);

        // XYZ → linear sRGB
        float r_lin = +3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
        float g_lin = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
        float b_lin = +0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

        auto linear_to_srgb = [](float c) -> float {
            if (c <= 0.0031308f) return 12.92f * c;
            return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        };

        return Color{
            static_cast<uint8_t>(std::clamp(linear_to_srgb(r_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(g_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(b_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))
        };
    }

    // lch() function — CSS Color Level 4 (CIE LCH, polar form of Lab)
    // lch(lightness chroma hue [/ alpha])
    if (value.substr(0, 4) == "lch(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        float vals[4] = {0, 0, 0, 1.0f};
        int count = parse_func_values(*args, vals, 4);
        if (count < 3) return std::nullopt;

        float L = std::clamp(vals[0], 0.0f, 100.0f);
        float C = std::max(0.0f, vals[1]);
        float h = std::fmod(vals[2], 360.0f);
        if (h < 0) h += 360.0f;
        float alpha = (count >= 4) ? std::clamp(vals[3], 0.0f, 1.0f) : 1.0f;

        // LCH → Lab
        float h_rad = h * 3.14159265f / 180.0f;
        float a_lab = C * std::cos(h_rad);
        float b_lab = C * std::sin(h_rad);

        // Lab → XYZ (same as lab() above)
        float fy = (L + 16.0f) / 116.0f;
        float fx = a_lab / 500.0f + fy;
        float fz = fy - b_lab / 200.0f;

        auto lab_f_inv = [](float t) -> float {
            float delta = 6.0f / 29.0f;
            if (t > delta) return t * t * t;
            return 3.0f * delta * delta * (t - 4.0f / 29.0f);
        };

        float X = 0.95047f * lab_f_inv(fx);
        float Y = 1.00000f * lab_f_inv(fy);
        float Z = 1.08883f * lab_f_inv(fz);

        float r_lin = +3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
        float g_lin = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
        float b_lin = +0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

        auto linear_to_srgb = [](float c) -> float {
            if (c <= 0.0031308f) return 12.92f * c;
            return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        };

        return Color{
            static_cast<uint8_t>(std::clamp(linear_to_srgb(r_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(g_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(linear_to_srgb(b_lin) * 255.0f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))
        };
    }

    // color-mix() function — CSS Color Level 5
    // color-mix(in srgb, color1 [percentage], color2 [percentage])
    if (value.substr(0, 10) == "color-mix(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        // Split on commas respecting parens
        std::vector<std::string> parts;
        std::string current;
        int depth = 0;
        for (char c : *args) {
            if (c == '(') { depth++; current += c; }
            else if (c == ')') { depth--; current += c; }
            else if (c == ',' && depth == 0) {
                parts.push_back(trim(current)); current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) parts.push_back(trim(current));

        // Need 3 parts: "in srgb", "color1 [%]", "color2 [%]"
        // Fallback: if only 1 part (tokenizer stripped commas), try space-based parsing
        if (parts.size() == 1) {
            // Tokenizer-reconstructed: "in srgb red 75% blue 25%" or "in srgb red blue"
            auto tokens = std::vector<std::string>();
            std::istringstream tss(*args);
            std::string tok;
            while (tss >> tok) tokens.push_back(tok);
            // Expected: "in" <space> [color1] [pct%] [color2] [pct%]
            if (tokens.size() >= 4 && to_lower(tokens[0]) == "in") {
                // tokens[1] is color space, tokens[2..] are colors with optional percentages
                size_t ci = 2;
                std::string c1_str, c2_str;
                float p1 = -1, p2 = -1;
                // Parse color1 + optional %
                c1_str = tokens[ci++];
                if (ci < tokens.size() && tokens[ci].back() == '%') {
                    try { p1 = std::stof(tokens[ci].substr(0, tokens[ci].size() - 1)); } catch (...) {}
                    ci++;
                }
                // Parse color2 + optional %
                if (ci < tokens.size()) {
                    c2_str = tokens[ci++];
                    if (ci < tokens.size() && tokens[ci].back() == '%') {
                        try { p2 = std::stof(tokens[ci].substr(0, tokens[ci].size() - 1)); } catch (...) {}
                    }
                }
                auto color1 = parse_color(c1_str);
                auto color2 = parse_color(c2_str);
                if (color1 && color2) {
                    if (p1 < 0 && p2 < 0) { p1 = 50; p2 = 50; }
                    else if (p1 < 0) p1 = 100 - p2;
                    else if (p2 < 0) p2 = 100 - p1;
                    float t = p1 + p2;
                    if (t > 0) {
                        float f1 = p1 / t, f2 = p2 / t;
                        return Color{
                            static_cast<uint8_t>(std::clamp(color1->r * f1 + color2->r * f2, 0.0f, 255.0f)),
                            static_cast<uint8_t>(std::clamp(color1->g * f1 + color2->g * f2, 0.0f, 255.0f)),
                            static_cast<uint8_t>(std::clamp(color1->b * f1 + color2->b * f2, 0.0f, 255.0f)),
                            static_cast<uint8_t>(std::clamp(color1->a * f1 + color2->a * f2, 0.0f, 255.0f))
                        };
                    }
                }
            }
            return std::nullopt;
        }
        if (parts.size() < 3) return std::nullopt;

        // parts[0] is the color space (srgb, oklch, oklab, lab, hsl)
        // parts[1] is color1 + optional percentage
        // parts[2] is color2 + optional percentage
        // Interpolation happens in the requested color space

        auto parse_color_with_pct = [](const std::string& s) -> std::pair<std::optional<Color>, float> {
            std::string trimmed = s;
            // Remove leading/trailing whitespace
            auto start = trimmed.find_first_not_of(" \t");
            auto end = trimmed.find_last_not_of(" \t");
            if (start == std::string::npos) return {std::nullopt, 50.0f};
            trimmed = trimmed.substr(start, end - start + 1);

            // Check for trailing percentage
            float pct = -1;
            auto pct_pos = trimmed.rfind('%');
            if (pct_pos != std::string::npos) {
                auto space_before = trimmed.rfind(' ', pct_pos);
                if (space_before != std::string::npos) {
                    std::string pct_str = trimmed.substr(space_before + 1, pct_pos - space_before - 1);
                    try { pct = std::stof(pct_str); } catch (...) {}
                    trimmed = trimmed.substr(0, space_before);
                    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
                }
            }

            auto c = parse_color(trimmed);
            return {c, pct >= 0 ? pct : -1};
        };

        auto [c1, pct1] = parse_color_with_pct(parts[1]);
        auto [c2, pct2] = parse_color_with_pct(parts[2]);

        if (!c1 || !c2) return std::nullopt;

        // Resolve percentages (default: 50% each)
        if (pct1 < 0 && pct2 < 0) { pct1 = 50; pct2 = 50; }
        else if (pct1 < 0) { pct1 = 100 - pct2; }
        else if (pct2 < 0) { pct2 = 100 - pct1; }

        float total = pct1 + pct2;
        if (total <= 0) return std::nullopt;
        float p1 = pct1 / total;
        float p2 = pct2 / total;

        return Color{
            static_cast<uint8_t>(std::clamp(c1->r * p1 + c2->r * p2, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(c1->g * p1 + c2->g * p2, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(c1->b * p1 + c2->b * p2, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp(c1->a * p1 + c2->a * p2, 0.0f, 255.0f))
        };
    }

    // light-dark() function — CSS Color Level 5
    // light-dark(light-color, dark-color) — returns light or dark color based on system dark mode
    if (value.substr(0, 11) == "light-dark(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        bool dark = is_dark_mode();

        // Split on first comma respecting parens
        int depth = 0;
        size_t split_pos = std::string::npos;
        for (size_t i = 0; i < args->size(); i++) {
            if ((*args)[i] == '(') depth++;
            else if ((*args)[i] == ')') depth--;
            else if ((*args)[i] == ',' && depth == 0) { split_pos = i; break; }
        }
        if (split_pos == std::string::npos) {
            // Fallback: tokenizer stripped commas, try space-separated parsing
            // "green red" → first token is light, second is dark
            auto sp = args->find(' ');
            if (sp != std::string::npos) {
                std::string light_str = trim(args->substr(0, sp));
                std::string dark_str = trim(args->substr(sp + 1));
                return parse_color(dark ? dark_str : light_str);
            }
            return std::nullopt;
        }

        std::string light_str = trim(args->substr(0, split_pos));
        std::string dark_str = trim(args->substr(split_pos + 1));
        return parse_color(dark ? dark_str : light_str);
    }

    // color() function — CSS Color Level 4
    // color(display-p3 r g b [/ alpha]) or color(srgb r g b [/ alpha])
    if (value.substr(0, 6) == "color(") {
        auto args = extract_func_args(value);
        if (!args) return std::nullopt;

        // Parse: colorspace r g b [/ alpha]
        // First strip the colorspace name
        std::string cleaned = *args;
        // Replace / with space for alpha separator
        for (auto& c : cleaned) {
            if (c == '/') c = ' ';
            if (c == ',') c = ' ';
        }

        std::istringstream iss(cleaned);
        std::string colorspace;
        iss >> colorspace;

        float r = 0, g = 0, b = 0, alpha = 1.0f;
        int count = 0;
        float v;
        while (iss >> v && count < 4) {
            if (count == 0) r = v;
            else if (count == 1) g = v;
            else if (count == 2) b = v;
            else alpha = v;
            count++;
        }
        if (count < 3) return std::nullopt;

        // Color values are in [0, 1] range
        auto gamma_to_srgb = [](float c) -> uint8_t {
            return static_cast<uint8_t>(std::clamp(c * 255.0f, 0.0f, 255.0f));
        };

        auto linear_to_srgb = [](float c) -> float {
            if (c <= 0.0031308f) return 12.92f * c;
            return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        };

        colorspace = to_lower(colorspace);

        if (colorspace == "srgb") {
            return Color{gamma_to_srgb(r), gamma_to_srgb(g), gamma_to_srgb(b),
                         static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))};
        } else if (colorspace == "srgb-linear") {
            return Color{
                static_cast<uint8_t>(std::clamp(linear_to_srgb(r) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(g) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(b) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))};
        } else if (colorspace == "display-p3") {
            // display-p3 → linear sRGB (approximate: P3 gamut is wider but we clamp)
            // P3 to XYZ D65
            float X = 0.4865709f * r + 0.2656677f * g + 0.1982173f * b;
            float Y = 0.2289746f * r + 0.6917385f * g + 0.0792869f * b;
            float Z = 0.0000000f * r + 0.0451134f * g + 1.0439444f * b;
            // XYZ to linear sRGB
            float lr = +3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
            float lg = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
            float lb = +0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;
            return Color{
                static_cast<uint8_t>(std::clamp(linear_to_srgb(lr) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(lg) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(lb) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))};
        } else if (colorspace == "a98-rgb") {
            // Adobe RGB (1998) to sRGB — approximate via XYZ
            // A98-RGB → linear with gamma 563/256
            auto a98_to_linear = [](float c) -> float {
                return (c < 0) ? -std::pow(-c, 563.0f / 256.0f) : std::pow(c, 563.0f / 256.0f);
            };
            float lr = a98_to_linear(r);
            float lg = a98_to_linear(g);
            float lb = a98_to_linear(b);
            // A98-RGB linear → XYZ
            float X = 0.5767309f * lr + 0.1855540f * lg + 0.1881852f * lb;
            float Y = 0.2973769f * lr + 0.6273491f * lg + 0.0752741f * lb;
            float Z = 0.0270343f * lr + 0.0706872f * lg + 0.9911085f * lb;
            // XYZ → linear sRGB
            float sr = +3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
            float sg = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
            float sb = +0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;
            return Color{
                static_cast<uint8_t>(std::clamp(linear_to_srgb(sr) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(sg) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(linear_to_srgb(sb) * 255.0f, 0.0f, 255.0f)),
                static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))};
        } else {
            // Unknown colorspace — treat as sRGB
            return Color{gamma_to_srgb(r), gamma_to_srgb(g), gamma_to_srgb(b),
                         static_cast<uint8_t>(std::clamp(alpha * 255.0f, 0.0f, 255.0f))};
        }
    }

    return std::nullopt;
}

std::vector<std::pair<std::string, int>> parse_font_feature_settings(const std::string& value) {
    std::string trimmed = trim(value);
    if (trimmed.empty()) return {};

    if (to_lower(trimmed) == "normal") {
        return {};
    }

    std::vector<std::pair<std::string, int>> settings;
    std::vector<std::string> segments;
    std::string current;
    bool in_quotes = false;

    for (char ch : trimmed) {
        if (ch == '"') {
            in_quotes = !in_quotes;
            current += ch;
            continue;
        }
        if (ch == ',' && !in_quotes) {
            segments.push_back(trim(current));
            current.clear();
            continue;
        }
        current += ch;
    }
    segments.push_back(trim(current));

    for (const auto& seg_raw : segments) {
        std::string seg = trim(seg_raw);
        if (seg.empty()) continue;

        std::string tag;
        int value = 1;
        std::string rest;
        size_t quote_start = seg.find('"');

        if (quote_start != std::string::npos) {
            size_t quote_end = seg.find('"', quote_start + 1);
            if (quote_end == std::string::npos) continue;
            tag = trim(seg.substr(quote_start + 1, quote_end - quote_start - 1));
            if (tag.size() != 4) continue;
            rest = trim(seg.substr(quote_end + 1));
        } else {
            size_t tag_end = seg.find_first_of(" \t");
            if (tag_end == std::string::npos) {
                tag = seg;
            } else {
                tag = seg.substr(0, tag_end);
                rest = trim(seg.substr(tag_end + 1));
            }
        }

        tag = to_lower(trim(tag));
        if (tag.size() != 4) continue;

        if (!rest.empty()) {
            int parsed = 1;
            std::istringstream iss(rest);
            if (iss >> parsed && (parsed == 0 || parsed == 1)) {
                value = parsed;
            }
        }
        settings.push_back({tag, value});
    }

    return settings;
}

std::optional<Length> parse_length(const std::string& input, const std::string& /*unit_hint*/) {
    std::string value = trim(input);

    if (value.empty()) return std::nullopt;

    if (value == "auto") {
        return Length::auto_val();
    }

    if (value == "0") {
        return Length::zero();
    }

    // CSS math constants
    if (value == "pi") return Length::px(3.14159265358979f);
    if (value == "e") return Length::px(2.71828182845905f);
    if (value == "infinity") return Length::px(std::numeric_limits<float>::infinity());
    if (value == "-infinity") return Length::px(-std::numeric_limits<float>::infinity());

    // Check for CSS math functions: calc(), min(), max(), clamp()
    {
        std::string lower = to_lower(value);

        // Helper: extract inner content between matching parens
        auto extract_func_inner = [&](size_t paren_pos) -> std::optional<std::string> {
            int depth = 0;
            size_t close = std::string::npos;
            for (size_t i = paren_pos; i < value.size(); i++) {
                if (value[i] == '(') depth++;
                else if (value[i] == ')') {
                    depth--;
                    if (depth == 0) { close = i; break; }
                }
            }
            if (close != std::string::npos)
                return value.substr(paren_pos + 1, close - paren_pos - 1);
            return std::nullopt;
        };

        if (lower.size() >= 6 && lower.substr(0, 5) == "calc(") {
            auto inner = extract_func_inner(4);
            if (inner) return parse_calc_expr(*inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "min(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("min", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "max(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("max", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 7 && lower.substr(0, 6) == "clamp(") {
            auto inner = extract_func_inner(5);
            if (inner) return parse_math_func("clamp", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 13 && lower.substr(0, 11) == "fit-content(") {
            auto inner = extract_func_inner(11);
            if (!inner) return std::nullopt;
            auto inner_len = parse_length(trim(*inner));
            if (!inner_len) return std::nullopt;
            return inner_len;
        }
        // CSS abs() function
        if (lower.size() >= 5 && lower.substr(0, 4) == "abs(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("abs", *inner);
            return std::nullopt;
        }
        // CSS sign() function
        if (lower.size() >= 6 && lower.substr(0, 5) == "sign(") {
            auto inner = extract_func_inner(4);
            if (inner) return parse_math_func("sign", *inner);
            return std::nullopt;
        }
        // CSS mod() function
        if (lower.size() >= 5 && lower.substr(0, 4) == "mod(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("mod", *inner);
            return std::nullopt;
        }
        // CSS rem() function
        if (lower.size() >= 5 && lower.substr(0, 4) == "rem(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("rem", *inner);
            return std::nullopt;
        }
        // CSS round() function
        if (lower.size() >= 7 && lower.substr(0, 6) == "round(") {
            auto inner = extract_func_inner(5);
            if (inner) return parse_math_func("round", *inner);
            return std::nullopt;
        }
        // CSS trigonometric functions
        if (lower.size() >= 5 && lower.substr(0, 4) == "sin(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("sin", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "cos(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("cos", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "tan(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("tan", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 6 && lower.substr(0, 5) == "asin(") {
            auto inner = extract_func_inner(4);
            if (inner) return parse_math_func("asin", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 6 && lower.substr(0, 5) == "acos(") {
            auto inner = extract_func_inner(4);
            if (inner) return parse_math_func("acos", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 6 && lower.substr(0, 5) == "atan(") {
            auto inner = extract_func_inner(4);
            if (inner) return parse_math_func("atan", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 7 && lower.substr(0, 6) == "atan2(") {
            auto inner = extract_func_inner(5);
            if (inner) return parse_math_func("atan2", *inner);
            return std::nullopt;
        }
        // CSS exponential functions
        if (lower.size() >= 6 && lower.substr(0, 5) == "sqrt(") {
            auto inner = extract_func_inner(4);
            if (inner) return parse_math_func("sqrt", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "pow(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("pow", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "exp(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("exp", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 5 && lower.substr(0, 4) == "log(") {
            auto inner = extract_func_inner(3);
            if (inner) return parse_math_func("log", *inner);
            return std::nullopt;
        }
        if (lower.size() >= 7 && lower.substr(0, 6) == "hypot(") {
            auto inner = extract_func_inner(5);
            if (inner) return parse_math_func("hypot", *inner);
            return std::nullopt;
        }
        // CSS env() function — safe-area-inset-* and others
        // On desktop, all env() values resolve to 0px (no safe area insets)
        // env(name) or env(name, fallback)
        if (lower.size() >= 5 && lower.substr(0, 4) == "env(") {
            auto inner = extract_func_inner(3);
            if (inner) {
                // Check for fallback value: env(name, fallback)
                // Comma-separated (original CSS) or space-separated (tokenizer-reconstructed)
                auto comma = inner->find(',');
                if (comma != std::string::npos) {
                    std::string fallback = trim(inner->substr(comma + 1));
                    auto fb_len = parse_length(fallback);
                    if (fb_len) return fb_len;
                }
                // Space-separated fallback: env(name 20px) — tokenizer strips commas
                auto space = inner->find(' ');
                if (space != std::string::npos) {
                    std::string fallback = trim(inner->substr(space + 1));
                    auto fb_len = parse_length(fallback);
                    if (fb_len) return fb_len;
                }
                // No fallback or unparseable — return 0px (desktop default)
                return Length::zero();
            }
            return Length::zero();
        }
    }

    // Try to parse number + unit
    char* end = nullptr;
    float num = std::strtof(value.c_str(), &end);

    if (end == value.c_str()) {
        // No number parsed
        return std::nullopt;
    }

    std::string unit_str = trim(std::string(end));

    if (unit_str.empty() || unit_str == "px") {
        return Length::px(num);
    } else if (unit_str == "em") {
        return Length::em(num);
    } else if (unit_str == "rem") {
        return Length::rem(num);
    } else if (unit_str == "%") {
        return Length::percent(num);
    } else if (unit_str == "vw" || unit_str == "dvw" || unit_str == "svw" || unit_str == "lvw") {
        return Length::vw(num);
    } else if (unit_str == "vh" || unit_str == "dvh" || unit_str == "svh" || unit_str == "lvh") {
        return Length::vh(num);
    } else if (unit_str == "vmin") {
        return Length::vmin(num);
    } else if (unit_str == "vmax") {
        return Length::vmax(num);
    } else if (unit_str == "cqw") {
        return Length::cqw(num);
    } else if (unit_str == "cqh") {
        return Length::cqh(num);
    } else if (unit_str == "cqi") {
        return Length::cqi(num);
    } else if (unit_str == "cqb") {
        return Length::cqb(num);
    } else if (unit_str == "cqmin") {
        return Length::cqmin(num);
    } else if (unit_str == "cqmax") {
        return Length::cqmax(num);
    } else if (unit_str == "ch") {
        return Length::ch(num);
    } else if (unit_str == "lh") {
        return Length::lh(num);
    } else if (unit_str == "deg") {
        return Length::px(num * 3.14159265358979f / 180.0f);
    } else if (unit_str == "rad") {
        return Length::px(num);
    } else if (unit_str == "grad") {
        return Length::px(num * 3.14159265358979f / 200.0f);
    } else if (unit_str == "turn") {
        return Length::px(num * 2.0f * 3.14159265358979f);
    }

    return std::nullopt;
}

} // namespace clever::css
