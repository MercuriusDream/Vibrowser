#include <clever/css/style/selector_matcher.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace clever::css {

// Parse an+b expression from :nth-child() argument
// Supports: "odd", "even", "3", "2n", "2n+1", "-n+3", "n", etc.
static bool parse_an_plus_b(const std::string& arg, int& a, int& b) {
    std::string s;
    for (char c : arg) {
        if (c != ' ') s += c;
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
        if (end == s.c_str()) return false;
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
        if (end == a_str.c_str()) return false;
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
    if (end == b_str.c_str()) return false;
    b = static_cast<int>(val);
    return true;
}

// Check if position (1-based) matches an+b expression
static bool matches_an_plus_b(int a, int b, int position) {
    if (a == 0) {
        return position == b;
    }
    // position = a*n + b  =>  n = (position - b) / a
    int diff = position - b;
    if (diff == 0) return true;
    if ((diff > 0) != (a > 0)) return false;
    return diff % a == 0;
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
            // Find the attribute in the element
            const std::string& attr_name = simple.attr_name.empty() ? simple.value : simple.attr_name;

            for (const auto& [name, val] : element.attributes) {
                if (name != attr_name) continue;

                switch (simple.attr_match) {
                    case AttributeMatch::Exists:
                        return true;
                    case AttributeMatch::Exact:
                        return val == simple.attr_value;
                    case AttributeMatch::Includes: {
                        // Space-separated list contains the value
                        std::string token;
                        for (size_t j = 0; j <= val.size(); ++j) {
                            if (j == val.size() || val[j] == ' ') {
                                if (token == simple.attr_value) return true;
                                token.clear();
                            } else {
                                token += val[j];
                            }
                        }
                        return false;
                    }
                    case AttributeMatch::DashMatch:
                        return val == simple.attr_value ||
                               (val.length() > simple.attr_value.length() &&
                                val.substr(0, simple.attr_value.length()) == simple.attr_value &&
                                val[simple.attr_value.length()] == '-');
                    case AttributeMatch::Prefix:
                        return val.substr(0, simple.attr_value.length()) == simple.attr_value;
                    case AttributeMatch::Suffix:
                        return val.length() >= simple.attr_value.length() &&
                               val.substr(val.length() - simple.attr_value.length()) == simple.attr_value;
                    case AttributeMatch::Substring:
                        return val.find(simple.attr_value) != std::string::npos;
                }
            }
            // Attribute not found
            return false;
        }

        case SimpleSelectorType::PseudoClass: {
            const auto& name = simple.value;
            if (name == "first-child") {
                return element.child_index == 0;
            } else if (name == "last-child") {
                return element.sibling_count > 0 && element.child_index == element.sibling_count - 1;
            } else if (name == "only-child") {
                return element.sibling_count == 1;
            } else if (name == "empty") {
                return element.child_element_count == 0 && !element.has_text_children;
            } else if (name == "root" || name == "scope") {
                return element.parent == nullptr;
            } else if (name == "first-of-type") {
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
                if (element.same_type_count > 0) {
                    return element.same_type_index == element.same_type_count - 1;
                }
                // Without pre-computed data, can only be sure if last child
                return element.sibling_count > 0 &&
                       element.child_index == element.sibling_count - 1;
            } else if (name == "nth-child") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                int position = static_cast<int>(element.child_index) + 1; // 1-based
                return matches_an_plus_b(a, b, position);
            } else if (name == "nth-last-child") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                int position = static_cast<int>(element.sibling_count - element.child_index); // 1-based from end
                return matches_an_plus_b(a, b, position);
            } else if (name == "not") {
                // Parse the argument as a selector and check it does NOT match
                auto inner_list = parse_selector_list(simple.argument);
                for (const auto& sel : inner_list.selectors) {
                    if (matches(element, sel)) return false;
                }
                return true;
            } else if (name == "is" || name == "where" || name == "matches" || name == "-webkit-any") {
                // :is() / :where() / :matches() — match if ANY argument selector matches
                // :where() is identical but with 0 specificity (handled at specificity calc, not here)
                auto inner_list = parse_selector_list(simple.argument);
                for (const auto& sel : inner_list.selectors) {
                    if (matches(element, sel)) return true;
                }
                return false;
            } else if (name == "nth-of-type") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                if (element.same_type_count > 0) {
                    int position = static_cast<int>(element.same_type_index) + 1;
                    return matches_an_plus_b(a, b, position);
                }
                // Fallback: walk prev_sibling
                int position = 1;
                const ElementView* sib = element.prev_sibling;
                while (sib) {
                    if (sib->tag_name == element.tag_name) position++;
                    sib = sib->prev_sibling;
                }
                return matches_an_plus_b(a, b, position);
            } else if (name == "nth-last-of-type") {
                int a = 0, b = 0;
                if (!parse_an_plus_b(simple.argument, a, b)) return false;
                if (element.same_type_count > 0) {
                    int position = static_cast<int>(element.same_type_count - element.same_type_index);
                    return matches_an_plus_b(a, b, position);
                }
                // Without pre-computed data, conservative: only handle last-child case
                if (element.sibling_count > 0 && element.child_index == element.sibling_count - 1) {
                    return matches_an_plus_b(a, b, 1);
                }
                return false;
            } else if (name == "only-of-type") {
                if (element.same_type_count > 0) {
                    return element.same_type_count == 1;
                }
                // Fallback: check prev_sibling only, conservative
                const ElementView* sib = element.prev_sibling;
                while (sib) {
                    if (sib->tag_name == element.tag_name) return false;
                    sib = sib->prev_sibling;
                }
                if (element.sibling_count > 0 && element.child_index == element.sibling_count - 1) {
                    return true;
                }
                return false;
            } else if (name == "has") {
                // :has() — match if ANY child/descendant matches the argument selector
                auto inner_list = parse_selector_list(simple.argument);
                for (const auto& sel : inner_list.selectors) {
                    // Check direct children and all descendants
                    std::vector<const ElementView*> stack;
                    for (auto* child : element.children) {
                        stack.push_back(child);
                    }
                    while (!stack.empty()) {
                        const ElementView* desc = stack.back();
                        stack.pop_back();
                        if (matches(*desc, sel)) return true;
                        for (auto* grandchild : desc->children) {
                            stack.push_back(grandchild);
                        }
                    }
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
                // Without runtime URL context, match elements with id attribute (conservative)
                return !element.id.empty();
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
            } else if (name == "any-link") {
                // :any-link matches <a>, <area>, <link> with href attribute
                if (element.tag_name != "a" && element.tag_name != "area" && element.tag_name != "link") return false;
                for (const auto& [n, v] : element.attributes) {
                    if (n == "href") return true;
                }
                return false;
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
                // Check for data-clever-focus attribute set by the UI layer
                for (const auto& [n, v] : element.attributes) {
                    if (n == "data-clever-focus") return true;
                }
                return false;
            } else if (name == "focus-within") {
                // :focus-within — matches if element or any descendant has focus
                struct FocusChecker {
                    static bool has_focus(const ElementView& n) {
                        for (const auto& [an, av] : n.attributes) {
                            if (an == "data-clever-focus") return true;
                        }
                        for (const auto* child : n.children) {
                            if (has_focus(*child)) return true;
                        }
                        return false;
                    }
                };
                return FocusChecker::has_focus(element);
            } else if (name == "hover") {
                // Check for data-clever-hover attribute set by the UI layer
                for (const auto& [n, v] : element.attributes) {
                    if (n == "data-clever-hover") return true;
                }
                return false;
            } else if (name == "active" || name == "visited") {
                // Active/visited pseudo-classes not yet tracked
                return false;
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
            } else if (name == "valid" || name == "invalid") {
                // Form validation pseudo-classes — without validation, all inputs are valid
                if (element.tag_name != "input" && element.tag_name != "select" &&
                    element.tag_name != "textarea" && element.tag_name != "form") return false;
                return name == "valid"; // all elements are valid by default
            } else if (name == "in-range" || name == "out-of-range") {
                // Range pseudo-classes for number/range inputs
                return name == "in-range"; // all in-range by default
            }
            // Other pseudo-classes (hover, focus, etc.) require state, return false for now
            return false;
        }

        case SimpleSelectorType::PseudoElement:
            // Pseudo-elements are handled specially in layout, always match here
            return true;
    }

    return false;
}

} // namespace clever::css
