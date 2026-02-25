#include <clever/dom/event.h>
#include <clever/dom/node.h>

namespace clever::dom {

// ---------------------------------------------------------------------------
// Event
// ---------------------------------------------------------------------------

Event::Event(const std::string& type, bool bubbles, bool cancelable)
    : type_(type)
    , bubbles_(bubbles)
    , cancelable_(cancelable) {}

// ---------------------------------------------------------------------------
// EventTarget
// ---------------------------------------------------------------------------

void EventTarget::add_event_listener(const std::string& type, EventListener listener, bool capture) {
    listeners_[type].push_back({std::move(listener), capture});
}

void EventTarget::remove_all_listeners(const std::string& type) {
    listeners_.erase(type);
}

bool EventTarget::dispatch_event(Event& event, Node& /*node*/) {
    auto it = listeners_.find(event.type());
    if (it == listeners_.end()) {
        return !event.default_prevented();
    }

    // Determine which listeners fire based on phase
    bool is_capture = (event.phase() == EventPhase::Capturing);
    bool is_at_target = (event.phase() == EventPhase::AtTarget);

    for (auto& entry : it->second) {
        if (event.immediate_propagation_stopped()) {
            break;
        }

        // At target: all listeners fire regardless of capture flag
        // Capturing phase: only capture listeners fire
        // Bubbling phase: only non-capture listeners fire
        if (is_at_target || (is_capture && entry.capture) || (!is_capture && !is_at_target && !entry.capture)) {
            entry.listener(event);
        }
    }

    return !event.default_prevented();
}

// ---------------------------------------------------------------------------
// dispatch_event_to_tree
// ---------------------------------------------------------------------------

void dispatch_event_to_tree(Event& event, Node& target) {
    // Build ancestor path from target to root
    std::vector<Node*> path;
    Node* current = target.parent();
    while (current) {
        path.push_back(current);
        current = current->parent();
    }

    event.target_ = &target;

    // Capture phase: from root to target's parent
    event.phase_ = EventPhase::Capturing;
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if (event.propagation_stopped()) break;
        event.current_target_ = *it;
        // Note: dispatch_event_to_tree uses Node-embedded EventTarget,
        // but our current design has separate EventTarget objects.
        // This function serves as a reference implementation pattern.
    }

    // Target phase
    if (!event.propagation_stopped()) {
        event.phase_ = EventPhase::AtTarget;
        event.current_target_ = &target;
    }

    // Bubble phase: from target's parent to root
    if (!event.propagation_stopped() && event.bubbles()) {
        event.phase_ = EventPhase::Bubbling;
        for (auto* ancestor : path) {
            if (event.propagation_stopped()) break;
            event.current_target_ = ancestor;
        }
    }

    event.phase_ = EventPhase::None;
    event.current_target_ = nullptr;
}

} // namespace clever::dom
