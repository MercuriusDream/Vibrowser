#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "browser/html/dom.h"

namespace browser::js {

struct ScriptResult {
  bool ok = false;
  std::string message;
};

ScriptResult execute_script(browser::html::Node& document, const std::string& script_source);

struct BridgeElement {
    bool found = false;
    std::string tag_name;
    std::string text_content;
    std::map<std::string, std::string> attributes;
    std::size_t child_count = 0;
};

struct QueryResult {
    bool ok = false;
    std::string message;
    std::vector<BridgeElement> elements;
};

QueryResult query_by_id(const browser::html::Node& document, const std::string& id);
QueryResult query_selector(const browser::html::Node& document, const std::string& selector);
QueryResult query_selector_all(const browser::html::Node& document, const std::string& selector);

struct MutationResult {
    bool ok = false;
    std::string message;
};

MutationResult set_attribute_by_id(browser::html::Node& document,
                                    const std::string& id,
                                    const std::string& attribute,
                                    const std::string& value);

MutationResult remove_attribute_by_id(browser::html::Node& document,
                                       const std::string& id,
                                       const std::string& attribute);

MutationResult set_style_by_id(browser::html::Node& document,
                                const std::string& id,
                                const std::string& property,
                                const std::string& value);

MutationResult set_text_by_id(browser::html::Node& document,
                               const std::string& id,
                               const std::string& text);

enum class EventType {
    Click,
    Input,
    Change,
};

const char* event_type_name(EventType type);

struct DomEvent {
    EventType type;
    std::string target_id;
    std::string value;
};

using EventHandler = std::function<void(browser::html::Node& document, const DomEvent& event)>;

struct EventBinding {
    std::string target_id;
    EventType type;
    EventHandler handler;
};

class EventRegistry {
public:
    void add_listener(const std::string& target_id, EventType type, EventHandler handler);
    MutationResult dispatch(browser::html::Node& document, const DomEvent& event) const;
    std::size_t listener_count() const;
    void clear();

private:
    std::vector<EventBinding> bindings_;
};

}  // namespace browser::js
