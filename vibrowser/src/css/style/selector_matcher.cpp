#include <clever/css/style/selector_matcher.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace clever::css {

// Parse an+b expression from :nth-child() argument
// Supports: "odd", "even", "3", "2n", "2n+1", "-n+3", "n", etc.
static bool parse_an_plus_b(const std::string& arg, int& a, int& b) {
    std::string s;
    s.reserve(arg.size());
    for (char c : arg) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (s.empty()) return false;

    if (s == "odd") { a = 2; b = 1; return true; }
    if (s == "even") { a = 2; b = 0; return true; }

    // Try to parse as an+b
    auto n_pos = s.find('n');
    if (n_pos == std::string::npos) {
        // Just a number: b
        char* end = nullptr;
        long val = std::strtol(s.c_str(), &end, 10);
        if (end == s.c_str() || *end != '\0') return false;
        a = 0;
        b = static_cast<int>(val);
        return true;
    }

    // Parse 'a' part before 'n'
    if (n_pos == 0) {
        a = 1;
    } else if (n_pos == 1 && s[0] == '-') {
        a = -1;
    } else if (n_pos == 1 && s[0] == '+') {
        a = 1;
    } else {
        std::string a_str = s.substr(0, n_pos);
        char* end = nullptr;
        long val = std::strtol(a_str.c_str(), &end, 10);
        if (end == a_str.c_str() || *end != '\0') return false;
        a = static_cast<int>(val);
    }

    // Parse 'b' part after 'n'
    if (n_pos + 1 >= s.size()) {
        b = 0;
        return true;
    }

    std::string b_str = s.substr(n_pos + 1);
    char* end = nullptr;
    long val = std::strtol(b_str.c_str(), &end, 10);
    if (end == b_str.c_str() || *end != '\0') return false;
    b = static_cast<int>(val);
    return true;
}

// Check if position (1-based) matches an+b expression
static bool matches_an_plus_b(int a, int b, int position) {
    if (position <= 0) return false;

    if (a == 0) {
        return position == b;
    }

    // position = a*n + b with n >= 0
    int diff = position - b;
    if (a > 0 && diff < 0) return false;
    if (a < 0 && diff > 0) return false;
    if (diff % a != 0) return false;

    int n = diff / a;
    return n >= 0;
}

static bool is_link_with_href(const ElementView& element) {
    if (element.tag_name != "a" && element.tag_name != "area" && element.tag_name != "link") {
        return false;
    }
    for (const auto& [n, v] : element.attributes) {
        if (n == "href") {
            return true;
        }
    }
    return false;
}

// Computes 1-based position among siblings of the same tag and total count.
// Returns false when sibling data is unavailable.
static bool compute_same_type_position(const ElementView& element, int& position, int& total) {
    if (element.same_type_count > 0) {
        position = static_cast<int>(element.same_type_index) + 1;
        total = static_cast<int>(element.same_type_count);
        return true;
    }

    if (element.parent && !element.parent->children.empty()) {
        int index = 0;
        int count = 0;
        bool found = false;
        for (const auto* child : element.parent->children) {
            if (!child || child->tag_name != element.tag_name) continue;
            ++count;
            if (child == &element) {
                index = count;
                found = true;
            }
        }
        if (!found || count == 0) return false;
        position = index;
        total = count;
        return true;
    }

    // Last fallback: derive forward position from previous siblings only.
    int index = 1;
    const ElementView* sib = element.prev_sibling;
    while (sib) {
        if (sib->tag_name == element.tag_name) ++index;
        sib = sib->prev_sibling;
    }
    position = index;
    total = index; // Lower bound only; callers needing total should treat this as unknown.
    return false;
}

bool SelectorMatcher::matches(const ElementView& element, const ComplexSelector& selector) const {
    if (selector.parts.empty()) {
        return false;
    }

    // The last part is the subject element (rightmost in CSS selector text)
    // We match right-to-left: first check if subject matches, then walk ancestors
    const auto& subject_part = selector.parts.back();
    if (!matches_compound(element, subject_part.compound)) {
        return false;
    }

    // If only one part, we're done
    if (selector.parts.size() == 1) {
        return true;
    }

    // Walk the remaining parts from right-to-left (index parts.size()-2 down to 0)
    // Each part has a combinator that relates it to the part to its right
    const ElementView* current = &element;

    for (int i = static_cast<int>(selector.parts.size()) - 2; i >= 0; --i) {
        const auto& part = selector.parts[static_cast<size_t>(i)];
        // The combinator on parts[i+1] tells us how parts[i] relates to parts[i+1]
        auto combinator = selector.parts[static_cast<size_t>(i) + 1].combinator;

        if (!combinator.has_value()) {
            // No combinator means this shouldn't happen in a well-formed selector
            // with multiple parts; treat as descendant
            combinator = Combinator::Descendant;
        }

        switch (*combinator) {
            case Combinator::Descendant: {
                // Walk up ancestors to find a match
                const ElementView* ancestor = current->parent;
                bool found = false;
                while (ancestor) {
                    if (matches_compound(*ancestor, part.compound)) {
                        current = ancestor;
                        found = true;
                        break;
                    }
                    ancestor = ancestor->parent;
                }
                if (!found) return false;
                break;
            }
            case Combinator::Child: {
                // Must be direct parent
                if (!current->parent) return false;
                if (!matches_compound(*current->parent, part.compound)) return false;
                current = current->parent;
                break;
            }
            case Combinator::NextSibling: {
                // Must be the immediately preceding sibling
                if (!current->prev_sibling) return false;
                if (!matches_compound(*current->prev_sibling, part.compound)) return false;
                current = current->prev_sibling;
                break;
            }
            case Combinator::SubsequentSibling: {
                // Any preceding sibling
                const ElementView* sibling = current->prev_sibling;
                bool found = false;
                while (sibling) {
                    if (matches_compound(*sibling, part.compound)) {
                        current = sibling;
                        found = true;
                        break;
                    }
                    sibling = sibling->prev_sibling;
                }
                if (!found) return false;
                break;
            }
        }
    }

    return true;
}

bool SelectorMatcher::matches_compound(const ElementView& element, const CompoundSelector& compound) const {
    // All simple selectors in the compound must match
    for (const auto& simple : compound.simple_selectors) {
        if (!matches_simple(element, simple)) {
            return false;
        }
    }
    return true;
}

bool SelectorMatcher::has_selector_matches(const ElementView& element,
                                          const ComplexSelector& selector) const {
    if (selector.parts.empty()) {
        return false;
    }

    const auto add_descendants = [](const ElementView& base,
                                   std::vector<const ElementView*>& out) {
        std::vector<const ElementView*> stack;
        for (const auto* child : base.children) {
            if (child) {
                stack.push_back(child);
            }
        }
        while (!stack.empty()) {
            const auto* current = stack.back();
            stack.pop_back();
            if (!current) continue;
            out.push_back(current);
            for (const auto* child : current->children) {
                if (child) {
                    stack.push_back(child);
                }
            }
        }
    };

    const auto add_next_sibling = [](const ElementView& base,
                                    std::vector<const ElementView*>& out) {
        if (!base.parent || base.parent->children.empty()) {
            return;
        }
        const auto& siblings = base.parent->children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i] == &base) {
                if (i + 1 < siblings.size()) {
                    out.push_back(siblings[i + 1]);
                }
                break;
            }
        }
    };

    const auto add_subsequent_siblings = [](const ElementView& base,
                                           std::vector<const ElementView*>& out) {
        if (!base.parent || base.parent->children.empty()) {
            return;
        }
        const auto& siblings = base.parent->children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i] == &base) {
                for (size_t j = i + 1; j < siblings.size(); ++j) {
                    out.push_back(siblings[j]);
                }
                break;
            }
        }
    };

    auto add_targets_for_combinator = [&](const ElementView& base,
                                         Combinator combinator,
                                         std::vector<const ElementView*>& out) {
        switch (combinator) {
            case Combinator::Descendant:
                add_descendants(base, out);
                break;
            case Combinator::Child:
                for (const auto* child : base.children) {
                    if (child) {
                        out.push_back(child);
                    }
                }
                break;
            case Combinator::NextSibling:
                add_next_sibling(base, out);
                break;
            case Combinator::SubsequentSibling:
                add_subsequent_siblings(base, out);
                break;
        }
    };

    const auto& first_part = selector.parts[0];
    const auto first_combinator =
        first_part.combinator.value_or(Combinator::Descendant); // default is descendant

    auto matches_remaining = [&](const ElementView& base, size_t part_index, auto&& self_ref) -> bool {
        if (part_index >= selector.parts.size()) {
            return false;
        }

        const auto& part = selector.parts[part_index];
        const auto combinator = part.combinator.value_or(Combinator::Descendant);
        std::vector<const ElementView*> candidates;
        add_targets_for_combinator(base, combinator, candidates);

        for (const auto* candidate : candidates) {
            if (!candidate || !matches_compound(*candidate, part.compound)) {
                continue;
            }

            if (part_index + 1 >= selector.parts.size()) {
                return true;
            }

            if (self_ref(*candidate, part_index + 1, self_ref)) {
                return true;
            }
        }

        return false;
    };

    std::vector<const ElementView*> initial_candidates;
    add_targets_for_combinator(element, first_combinator, initial_candidates);
    for (const auto* candidate : initial_candidates) {
        if (!candidate || !matches_compound(*candidate, first_part.compound)) {
            continue;
        }
        if (selector.parts.size() == 1) {
            return true;
        }
        if (matches_remaining(*candidate, 1, matches_remaining)) {
            return true;
        }
    }

    return false;
}

bool SelectorMatcher::matches_simple(const ElementView& element, const SimpleSelector& simple) const {
    switch (simple.type) {
        case SimpleSelectorType::Universal:
            return true;

        case SimpleSelectorType::Type:
            return element.tag_name == simple.value;

        case SimpleSelectorType::Class:
            return std::find(element.classes.begin(), element.classes.end(), simple.value)
                   != element.classes.end();

        case SimpleSelectorType::Id:
            return element.id == simple.value;

        case SimpleSelectorType::Attribute: {
            const std::string& attr_name = simple.attr_name.empty() ? simple.value : simple.attr_name;
            if (attr_name.empty()) return false;

            const std::string* attr_value = nullptr;
            for (const auto& [name, val] : element.attributes) {
                if (name.size() != attr_name.size()) continue;
                bool same_name = true;
                for (size_t i = 0; i < name.size(); ++i) {
                    const unsigned char lhs = static_cast<unsigned char>(name[i]);
                    const unsigned char rhs = static_cast<unsigned char>(attr_name[i]);
                    if (std::tolower(lhs) != std::tolower(rhs)) {
                        same_name = false;
                        break;
                    }
                }
                if (!same_name) continue;
                attr_value = &val;
                break;
            }
            if (!attr_value) return false;

            const std::string& val = *attr_value;
            switch (simple.attr_match) {
                case AttributeMatch::Exists:
                    return true;
                case AttributeMatch::Exact:
                    return val == simple.attr_value;
                case AttributeMatch::Includes: {
                    // Whitespace-separated token list contains the value.
                    if (simple.attr_value.empty()) return false;
                    size_t i = 0;
                    while (i < val.size()) {
                        while (i < val.size() &&
                               std::isspace(static_cast<unsigned char>(val[i]))) {
                            ++i;
                        }
                        const size_t start = i;
                        while (i < val.size() &&
                               !std::isspace(static_cast<unsigned char>(val[i]))) {
                            ++i;
                        }
                        const size_t len = i - start;
                        if (len == simple.attr_value.size() &&
                            val.compare(start, len, simple.attr_value) == 0) {
                            return true;
                        }
                    }
                    return false;
                }
                case AttributeMatch::DashMatch:
                    if (simple.attr_value.empty()) return false;
                    return val == simple.attr_value ||
                           (val.length() > simple.attr_value.length() &&
                            val.compare(0, simple.attr_value.length(), simple.attr_value) == 0 &&
                            val[simple.attr_value.length()] == '-');
                case AttributeMatch::Prefix:
                    if (simple.attr_value.empty() || val.length() < simple.attr_value.length()) {
                        return false;
                    }
                    return val.compare(0, simple.attr_value.length(), simple.attr_value) == 0;
                case AttributeMatch::Suffix:
                    if (simple.attr_value.empty() || val.length() < simple.attr_value.length()) {
                        return false;
                    }
                    return val.compare(
                               val.length() - simple.attr_value.length(),
                               simple.attr_value.length(),
                               simple.attr_value) == 0;
                case AttributeMatch::Substring:
                    if (simple.attr_value.empty()) return false;
                    return val.find(simple.attr_value) != std::string::npos;
            }
        }

        case SimpleSelectorType::PseudoClass: {
            const auto& name = simple.value;
            if (name == "first-child") {
                if (element.sibling_count > 0 && element.child_index < element.sibling_count) {
                    return element.child_index == 0;
                }
                if (element.parent && !element.parent->children.empty()) {
                    return element.parent->children.front() == &element;
                }
                return element.parent != nullptr && element.prev_sibling == nullptr;
            } else if (name == "last-child") {
                if (element.sibling_count > 0 && element.child_index < element.sibling_count) {
                    return element.child_index == element.sibling_count - 1;
                }
                if (element.parent && !element.parent->children.empty()) {
                    return element.parent->children.back() == &element;
                }
                return false;
            } else if (name == "only-child") {
                if (element.sibling_count > 0) {
                    return element.sibling_count == 1;
                }
                if (element.parent && !element.parent->children.empty()) {
                    return element.parent->children.size() == 1 &&
                           element.parent->children.front() == &element;
                }
                return false;
            } else if (name == "empty") {
                if (element.child_element_count > 0 || !element.children.empty()) {
                    return false;
                }
                return !element.has_text_children;
            } else if (name == "root") {
                if (element.parent != nullptr) return false;
                std::string tag = element.tag_name;
                std::transform(
                    tag.begin(),
                    tag.end(),
                    tag.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return tag == "html";
            } else if (name == "scope") {
                return element.parent == nullptr;
            } else if (name == "first-of-type") {
                int position = 0;
                int total = 0;
                if (compute_same_type_position(element, position, total)) {
                    return position == 1;
                }
                if (element.same_type_count > 0) {
                    return element.same_type_index == 0;
                }
                // Fallback: walk prev_sibling
                const ElementView* sib = element.prev_sibling;
                while (sib) {
                    if (sib->tag_name == element.tag_name) return false;
                    sib = sib->prev_sibling;
                }
                return true;
            } else if (name == "last-of-type") {
                int position = 0;
                int total = 0;
                if (!compute_same_type_position(element, position, total)) {
                    return false;
                }
                return position == total;
            } else if (name == "nth-child") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                int position = 0;
                if (element.sibling_count > 0 && element.child_index < element.sibling_count) {
                    position = static_cast<int>(element.child_index) + 1;
                } else if (element.parent && !element.parent->children.empty()) {
                    bool found = false;
                    int idx = 0;
                    for (const auto* child : element.parent->children) {
                        if (!child) continue;
                        ++idx;
                        if (child == &element) {
                            position = idx;
                            found = true;
                            break;
                        }
                    }
                    if (!found) return false;
                } else {
                    // Last fallback: infer from previous siblings only.
                    position = 1;
                    const ElementView* sib = element.prev_sibling;
                    while (sib) {
                        ++position;
                        sib = sib->prev_sibling;
                    }
                }
                return matches_an_plus_b(a, b, position);
            } else if (name == "nth-last-child") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                int position = 0;
                if (element.sibling_count > 0 && element.child_index < element.sibling_count) {
                    position = static_cast<int>(element.sibling_count - element.child_index);
                } else if (element.parent && !element.parent->children.empty()) {
                    bool found = false;
                    size_t idx = 0;
                    for (; idx < element.parent->children.size(); ++idx) {
                        if (element.parent->children[idx] == &element) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) return false;
                    position = static_cast<int>(element.parent->children.size() - idx);
                } else {
                    return false;
                }
                return matches_an_plus_b(a, b, position);
            } else if (name == "not") {
                // :not() receives a comma-separated selector list. It matches only
                // when every selector in that list does NOT match this element.
                auto inner_list = parse_selector_list(simple.argument);
                for (const auto& sel : inner_list.selectors) {
                    if (matches(element, sel)) return false;
                }
                return true;
            } else if (name == "is" || name == "where" || name == "matches" || name == "-webkit-any") {
                // :is() / :where() / :matches() / -webkit-any() accept a comma-separated
                // selector list and match when any listed selector matches.
                // :where() is identical here; specificity is handled elsewhere.
                auto inner_list = parse_selector_list(simple.argument);
                for (const auto& sel : inner_list.selectors) {
                    if (matches(element, sel)) return true;
                }
                return false;
            } else if (name == "nth-of-type") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                int position = 0;
                int total = 0;
                if (compute_same_type_position(element, position, total)) {
                    return matches_an_plus_b(a, b, position);
                }
                // Fallback: walk prev_sibling
                position = 1;
                const ElementView* sib = element.prev_sibling;
                while (sib) {
                    if (sib->tag_name == element.tag_name) position++;
                    sib = sib->prev_sibling;
                }
                return matches_an_plus_b(a, b, position);
            } else if (name == "nth-last-of-type") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                int position = 0;
                int total = 0;
                if (!compute_same_type_position(element, position, total)) {
                    return false;
                }
                return matches_an_plus_b(a, b, total - position + 1);
            } else if (name == "only-of-type") {
                int position = 0;
                int total = 0;
                if (!compute_same_type_position(element, position, total)) {
                    return false;
                }
                return total == 1;
            } else if (name == "has") {
                // :has() takes a comma-separated selector list. It matches when any
                // selector in that list matches via the relative matching path.
                auto inner_list = parse_selector_list(simple.argument);
                for (const auto& sel : inner_list.selectors) {
                    if (has_selector_matches(element, sel)) return true;
                }
                return false;
            } else if (name == "enabled") {
                // Form elements that are enabled (no disabled attribute)
                if (element.tag_name != "input" && element.tag_name != "button" &&
                    element.tag_name != "select" && element.tag_name != "textarea") return false;
                for (const auto& [n, v] : element.attributes) {
                    if (n == "disabled") return false;
                }
                return true;
            } else if (name == "disabled") {
                // Form elements with disabled attribute
                if (element.tag_name != "input" && element.tag_name != "button" &&
                    element.tag_name != "select" && element.tag_name != "textarea") return false;
                for (const auto& [n, v] : element.attributes) {
                    if (n == "disabled") return true;
                }
                return false;
            } else if (name == "checked") {
                // Checked inputs (checkbox, radio) or selected option
                for (const auto& [n, v] : element.attributes) {
                    if (n == "checked" || n == "selected") return true;
                }
                return false;
            } else if (name == "required") {
                for (const auto& [n, v] : element.attributes) {
                    if (n == "required") return true;
                }
                return false;
            } else if (name == "optional") {
                if (element.tag_name != "input" && element.tag_name != "select" &&
                    element.tag_name != "textarea") return false;
                for (const auto& [n, v] : element.attributes) {
                    if (n == "required") return false;
                }
                return true;
            } else if (name == "read-only") {
                for (const auto& [n, v] : element.attributes) {
                    if (n == "readonly") return true;
                }
                // Non-editable elements are read-only by default
                return element.tag_name != "input" && element.tag_name != "textarea";
            } else if (name == "read-write") {
                if (element.tag_name != "input" && element.tag_name != "textarea") return false;
                for (const auto& [n, v] : element.attributes) {
                    if (n == "readonly") return false;
                }
                return true;
            } else if (name == "target") {
                // :target matches element whose ID matches the URL fragment
                // Without URL fragment context, fail closed to avoid broad false matches.
                return false;
            } else if (name == "lang") {
                // :lang(xx) matches element with matching lang attribute
                std::string want = simple.argument;
                // Normalize to lowercase
                for (auto& c : want) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                // Walk up to find lang attribute on element or ancestors
                const ElementView* cur = &element;
                while (cur) {
                    for (const auto& [n, v] : cur->attributes) {
                        if (n == "lang") {
                            std::string have = v;
                            for (auto& c : have) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                            // Match exact or prefix (e.g., "en" matches "en-US")
                            if (have == want || (have.size() > want.size() && have.substr(0, want.size()) == want && have[want.size()] == '-')) {
                                return true;
                            }
                            return false; // lang attr found but doesn't match
                        }
                    }
                    cur = cur->parent;
                }
                return false;
            } else if (name == "link" || name == "any-link") {
                // :link and :any-link match <a>, <area>, or <link> with href.
                return is_link_with_href(element);
            } else if (name == "local-link") {
                // For now this is an alias for :any-link until origin checks are wired in.
                return is_link_with_href(element);
            } else if (name == "defined") {
                // :defined — all standard HTML elements are defined
                return true;
            } else if (name == "placeholder-shown") {
                // :placeholder-shown — matches input/textarea when placeholder is visible
                if (element.tag_name != "input" && element.tag_name != "textarea") return false;
                bool has_placeholder = false;
                bool has_value = false;
                for (const auto& [n, v] : element.attributes) {
                    if (n == "placeholder" && !v.empty()) has_placeholder = true;
                    if (n == "value" && !v.empty()) has_value = true;
                }
                return has_placeholder && !has_value;
            } else if (name == "autofill" || name == "-webkit-autofill") {
                // :autofill / :-webkit-autofill — matches autofilled form elements
                // Without browser autofill state, always false
                return false;
            } else if (name == "focus" || name == "focus-visible") {
                // Check for data-vibrowser-focus attribute set by the UI layer
                for (const auto& [n, v] : element.attributes) {
                    if (n == "data-vibrowser-focus" || n == "data-clever-focus") return true;
                }
                return false;
            } else if (name == "focus-within") {
                // :focus-within — matches if element or any descendant has focus
                struct FocusChecker {
                    static bool has_focus(const ElementView& n) {
                        for (const auto& [an, av] : n.attributes) {
                            if (an == "data-vibrowser-focus" || an == "data-clever-focus") return true;
                        }
                        for (const auto* child : n.children) {
                            if (has_focus(*child)) return true;
                        }
                        return false;
                    }
                };
                return FocusChecker::has_focus(element);
            } else if (name == "hover") {
                // Check for data-vibrowser-hover attribute set by the UI layer
                for (const auto& [n, v] : element.attributes) {
                    if (n == "data-vibrowser-hover" || n == "data-clever-hover") return true;
                }
                return false;
            } else if (name == "active") {
                // Active pseudo-class not yet tracked.
                return false;
            } else if (name == "visited") {
                // Privacy-preserving fallback: without browser history state,
                // :visited behaves like :any-link for broad selector compatibility.
                return is_link_with_href(element);
            } else if (name == "indeterminate") {
                // :indeterminate — matches checkbox/radio in indeterminate state
                // Without runtime state, always false
                return false;
            } else if (name == "default") {
                // :default — matches default button in form or default option
                for (const auto& [n, v] : element.attributes) {
                    if (n == "selected" || n == "checked") return true;
                }
                if (element.tag_name == "button") {
                    for (const auto& [n, v] : element.attributes) {
                        if (n == "type" && v == "submit") return true;
                    }
                }
                return false;
            } else if (name == "popover-open") {
                // :popover-open — matches popovers that are currently showing
                for (const auto& [n, v] : element.attributes) {
                    if (n == "data-popover-showing") return true;
                }
                return false;
            } else if (name == "valid" || name == "invalid") {
                // Form validation pseudo-classes — without validation, all inputs are valid
                if (element.tag_name != "input" && element.tag_name != "select" &&
                    element.tag_name != "textarea" && element.tag_name != "form") return false;
                return name == "valid"; // all elements are valid by default
            } else if (name == "in-range" || name == "out-of-range") {
                // Range pseudo-classes for number/range inputs
                return name == "in-range"; // all in-range by default
            } else if (name == "open") {
                // :open — matches <details> when open attribute is set, <dialog> when shown
                const std::string& tag = element.tag_name;
                if (tag == "details") {
                    for (const auto& [n, v] : element.attributes)
                        if (n == "open") return true;
                    return false;
                }
                if (tag == "dialog") {
                    for (const auto& [n, v] : element.attributes)
                        if (n == "open" || n == "data-dialog-open") return true;
                    return false;
                }
                // <select> is open when it has open dropdown state
                if (tag == "select") {
                    for (const auto& [n, v] : element.attributes)
                        if (n == "data-select-open") return true;
                    return false;
                }
                return false;
            } else if (name == "closed") {
                // :closed — inverse of :open for details/dialog
                const std::string& tag = element.tag_name;
                if (tag == "details") {
                    for (const auto& [n, v] : element.attributes)
                        if (n == "open") return false;
                    return true;
                }
                if (tag == "dialog") {
                    for (const auto& [n, v] : element.attributes)
                        if (n == "open" || n == "data-dialog-open") return false;
                    return true;
                }
                return false;
            } else if (name == "modal") {
                // :modal — matches <dialog> opened via showModal()
                if (element.tag_name == "dialog") {
                    for (const auto& [n, v] : element.attributes)
                        if (n == "data-dialog-modal") return true;
                }
                return false;
            } else if (name == "fullscreen") {
                // :fullscreen — matches element in fullscreen mode
                for (const auto& [n, v] : element.attributes)
                    if (n == "data-fullscreen") return true;
                return false;
            } else if (name == "user-valid") {
                // :user-valid — matches valid form field after user interaction
                if (element.tag_name != "input" && element.tag_name != "select" &&
                    element.tag_name != "textarea") return false;
                for (const auto& [n, v] : element.attributes)
                    if (n == "data-user-interacted") return true;
                return false;
            } else if (name == "user-invalid") {
                // :user-invalid — matches invalid form field after user interaction
                if (element.tag_name != "input" && element.tag_name != "select" &&
                    element.tag_name != "textarea") return false;
                for (const auto& [n, v] : element.attributes)
                    if (n == "data-user-interacted" && v == "invalid") return true;
                return false;
            } else if (name == "paused") {
                // :paused — matches paused media elements
                for (const auto& [n, v] : element.attributes)
                    if (n == "data-media-paused") return true;
                return false;
            } else if (name == "playing") {
                // :playing — matches playing media elements
                for (const auto& [n, v] : element.attributes)
                    if (n == "data-media-playing") return true;
                return false;
            } else if (name == "picture-in-picture") {
                // :picture-in-picture — matches elements in PIP mode
                for (const auto& [n, v] : element.attributes)
                    if (n == "data-pip") return true;
                return false;
            }
            // Other pseudo-classes (hover, focus, etc.) require state, return false for now
            return false;
        }

        case SimpleSelectorType::PseudoElement:
            // Pseudo-elements should never match regular element nodes.
            return false;
    }

    return false;
}

} // namespace clever::css
