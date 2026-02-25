#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace clever::dom {

class Node;

enum class EventPhase {
    None = 0,
    Capturing = 1,
    AtTarget = 2,
    Bubbling = 3
};

class Event {
public:
    explicit Event(const std::string& type, bool bubbles = true, bool cancelable = true);

    const std::string& type() const { return type_; }
    Node* target() const { return target_; }
    Node* current_target() const { return current_target_; }
    EventPhase phase() const { return phase_; }
    bool bubbles() const { return bubbles_; }
    bool cancelable() const { return cancelable_; }

    void stop_propagation() { propagation_stopped_ = true; }
    void stop_immediate_propagation() { immediate_propagation_stopped_ = true; propagation_stopped_ = true; }
    void prevent_default() { if (cancelable_) default_prevented_ = true; }

    bool propagation_stopped() const { return propagation_stopped_; }
    bool immediate_propagation_stopped() const { return immediate_propagation_stopped_; }
    bool default_prevented() const { return default_prevented_; }

    // These fields are set by the event dispatch mechanism.
    // They are public to allow dispatch_event_to_tree and custom
    // dispatch code to set them during propagation.
    std::string type_;
    Node* target_ = nullptr;
    Node* current_target_ = nullptr;
    EventPhase phase_ = EventPhase::None;

private:
    friend class EventTarget;
    bool bubbles_;
    bool cancelable_;
    bool propagation_stopped_ = false;
    bool immediate_propagation_stopped_ = false;
    bool default_prevented_ = false;
};

class EventTarget {
public:
    using EventListener = std::function<void(Event&)>;

    void add_event_listener(const std::string& type, EventListener listener, bool capture = false);
    void remove_all_listeners(const std::string& type);
    bool dispatch_event(Event& event, Node& node);

private:
    struct ListenerEntry {
        EventListener listener;
        bool capture;
    };
    std::unordered_map<std::string, std::vector<ListenerEntry>> listeners_;
};

// Dispatch event through DOM tree (capture -> target -> bubble)
void dispatch_event_to_tree(Event& event, Node& target);

} // namespace clever::dom
