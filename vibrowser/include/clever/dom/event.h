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

// ---------------------------------------------------------------------------
// MouseEvent — extends Event with mouse-specific properties.
// Per the W3C UIEvents spec, MouseEvent inherits from UIEvent which
// inherits from Event.  We collapse UIEvent into MouseEvent for simplicity.
// ---------------------------------------------------------------------------

class MouseEvent : public Event {
public:
    explicit MouseEvent(const std::string& type,
                        bool bubbles = true,
                        bool cancelable = true)
        : Event(type, bubbles, cancelable) {}

    // Coordinate properties
    double client_x() const { return client_x_; }
    double client_y() const { return client_y_; }
    double screen_x() const { return screen_x_; }
    double screen_y() const { return screen_y_; }
    double page_x() const { return page_x_; }
    double page_y() const { return page_y_; }
    double offset_x() const { return offset_x_; }
    double offset_y() const { return offset_y_; }
    double movement_x() const { return movement_x_; }
    double movement_y() const { return movement_y_; }

    void set_client_x(double v) { client_x_ = v; }
    void set_client_y(double v) { client_y_ = v; }
    void set_screen_x(double v) { screen_x_ = v; }
    void set_screen_y(double v) { screen_y_ = v; }
    void set_page_x(double v) { page_x_ = v; }
    void set_page_y(double v) { page_y_ = v; }
    void set_offset_x(double v) { offset_x_ = v; }
    void set_offset_y(double v) { offset_y_ = v; }
    void set_movement_x(double v) { movement_x_ = v; }
    void set_movement_y(double v) { movement_y_ = v; }

    // Button properties
    int button() const { return button_; }
    int buttons() const { return buttons_; }
    void set_button(int v) { button_ = v; }
    void set_buttons(int v) { buttons_ = v; }

    // Modifier keys
    bool alt_key() const { return alt_key_; }
    bool ctrl_key() const { return ctrl_key_; }
    bool meta_key() const { return meta_key_; }
    bool shift_key() const { return shift_key_; }
    void set_alt_key(bool v) { alt_key_ = v; }
    void set_ctrl_key(bool v) { ctrl_key_ = v; }
    void set_meta_key(bool v) { meta_key_ = v; }
    void set_shift_key(bool v) { shift_key_ = v; }

    // UIEvent detail (click count for click events)
    int detail() const { return detail_; }
    void set_detail(int v) { detail_ = v; }

    // Related target (for mouseenter/mouseleave/mouseover/mouseout)
    Node* related_target() const { return related_target_; }
    void set_related_target(Node* t) { related_target_ = t; }

    // getModifierState helper
    bool get_modifier_state(const std::string& key) const {
        if (key == "Control") return ctrl_key_;
        if (key == "Shift") return shift_key_;
        if (key == "Alt") return alt_key_;
        if (key == "Meta") return meta_key_;
        return false;
    }

private:
    double client_x_ = 0;
    double client_y_ = 0;
    double screen_x_ = 0;
    double screen_y_ = 0;
    double page_x_ = 0;
    double page_y_ = 0;
    double offset_x_ = 0;
    double offset_y_ = 0;
    double movement_x_ = 0;
    double movement_y_ = 0;
    int button_ = 0;    // 0=primary, 1=middle, 2=secondary
    int buttons_ = 0;   // bitmask of pressed buttons
    bool alt_key_ = false;
    bool ctrl_key_ = false;
    bool meta_key_ = false;
    bool shift_key_ = false;
    int detail_ = 0;
    Node* related_target_ = nullptr;
};

// Dispatch event through DOM tree (capture -> target -> bubble)
void dispatch_event_to_tree(Event& event, Node& target);

} // namespace clever::dom
