#pragma once
#include <clever/css/style/computed_style.h>
#include <clever/layout/box.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <map>

namespace clever::paint {

// Forward declarations
class AnimationController;

// Represents an interpolated property value update for an animated element
struct PropertyUpdate {
    clever::layout::LayoutNode* element = nullptr;
    std::string property_name;
    float float_value = 0.0f;
    clever::css::Color color_value;
    clever::css::Transform transform_value;
    bool has_float = false;
    bool has_color = false;
    bool has_transform = false;
};

// Represents a single running animation or transition instance
struct AnimationInstance {
    clever::layout::LayoutNode* target = nullptr;
    std::string property;
    std::string animation_name;  // non-empty only for keyframe animations
    double start_time_ms = 0.0;
    double duration_ms = 0.0;
    double delay_ms = 0.0;
    int timing_function = 0;    // 0=ease, 1=linear, etc.
    float bezier_x1 = 0.0f, bezier_y1 = 0.0f;
    float bezier_x2 = 1.0f, bezier_y2 = 1.0f;
    int steps_count = 1;

    // Value storage for float/numeric properties
    float from_value = 0.0f;
    float to_value = 0.0f;

    // Value storage for color properties
    clever::css::Color from_color;
    clever::css::Color to_color;

    // Value storage for transform properties
    clever::css::Transform from_transform;
    clever::css::Transform to_transform;

    // Animation-specific parameters
    bool is_paused = false;
    int fill_mode = 0;          // 0=none, 1=forwards, 2=backwards, 3=both
    int direction = 0;           // 0=normal, 1=reverse, 2=alternate, 3=alternate-reverse
    float iteration_count = 1.0f; // -1 = infinite
    int current_iteration = 0;
    bool is_transition = false;   // true for CSS transitions, false for animations

    // Type flags for interpolation
    bool has_float = false;
    bool has_color = false;
    bool has_transform = false;
};

// Manages CSS Animations and Transitions runtime
class AnimationController {
public:
    AnimationController() = default;
    ~AnimationController() = default;

    // Advance all running animations by delta_ms
    void tick(double delta_ms);

    // Start a CSS keyframe animation on an element
    void start_animation(clever::layout::LayoutNode* element,
                         const std::string& animation_name,
                         const clever::css::KeyframesDefinition& keyframes,
                         const clever::css::ComputedStyle& style);

    // Start a CSS transition on an element
    void start_transition(clever::layout::LayoutNode* element,
                          const std::string& property,
                          float from_value,
                          float to_value,
                          const clever::css::TransitionDef& transition);

    // Start a color transition
    void start_color_transition(clever::layout::LayoutNode* element,
                                const std::string& property,
                                const clever::css::Color& from_color,
                                const clever::css::Color& to_color,
                                const clever::css::TransitionDef& transition);

    // Start a transform transition
    void start_transform_transition(clever::layout::LayoutNode* element,
                                    const std::string& property,
                                    const clever::css::Transform& from_transform,
                                    const clever::css::Transform& to_transform,
                                    const clever::css::TransitionDef& transition);

    // Remove all animations/transitions associated with an element
    void remove_animations_for_element(clever::layout::LayoutNode* element);

    // Check if an element has any active animations or transitions
    bool is_animating(const clever::layout::LayoutNode* element) const;

    // Get list of property updates from all running animations
    // Returns a vector of PropertyUpdate structs representing interpolated values
    const std::vector<PropertyUpdate>& get_active_property_updates() const {
        return active_updates_;
    }

    // Clear active updates (called after applying them)
    void clear_updates() {
        active_updates_.clear();
    }

private:
    // Internal: Apply easing function to progress 0.0-1.0
    float apply_timing_function(const AnimationInstance& anim, float progress) const;

    // Internal: Interpolate a single animation and add updates
    void interpolate_animation(AnimationInstance& anim, double current_time_ms);

    // Internal: Find keyframe bounds for current progress
    void find_keyframe_range(const std::vector<clever::css::KeyframeStop>& steps,
                             float progress,
                             size_t& out_from_idx,
                             size_t& out_to_idx,
                             float& out_local_t);

    // Internal: Parse animated property value from keyframe style
    bool extract_property_value(const clever::css::ComputedStyle& style,
                               const std::string& property,
                               float& out_value,
                               clever::css::Color& out_color,
                               clever::css::Transform& out_transform);

    // List of all active animations/transitions
    std::vector<AnimationInstance> animations_;

    // Cached property updates to be applied by render pipeline
    std::vector<PropertyUpdate> active_updates_;

    // Map of element -> animation indices for quick lookup
    std::unordered_map<clever::layout::LayoutNode*, std::vector<size_t>> element_animations_;
};

} // namespace clever::paint
