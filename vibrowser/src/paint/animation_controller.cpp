#include <clever/paint/animation_controller.h>
#include <clever/layout/box.h>
#include <cmath>
#include <algorithm>
#include <set>
#include <sstream>
#include <cctype>

namespace clever::paint {

// Forward declarations for easing functions (defined in render_pipeline.cpp)
extern float cubic_bezier_sample(float p1x, float p1y, float p2x, float p2y, float t);
extern float ease_linear(float t);
extern float ease_ease(float t);
extern float ease_in(float t);
extern float ease_out(float t);
extern float ease_in_out(float t);
extern float apply_easing(int timing_function, float t);
extern float apply_easing_custom(int timing_function, float t,
                                  float bx1, float by1, float bx2, float by2,
                                  int steps_count);
extern float interpolate_float(float from, float to, float t);
extern clever::css::Color interpolate_color(const clever::css::Color& from,
                                            const clever::css::Color& to, float t);
extern clever::css::Transform interpolate_transform(const clever::css::Transform& from,
                                                     const clever::css::Transform& to, float t);

static float parse_length_value(const std::string& css_value) {
    if (css_value.empty()) return 0.0f;

    size_t i = 0;
    bool has_minus = false;
    if (css_value[0] == '-') {
        has_minus = true;
        i = 1;
    }

    float value = 0.0f;
    bool has_decimal = false;

    while (i < css_value.length() && (std::isdigit(css_value[i]) || css_value[i] == '.')) {
        if (css_value[i] == '.') {
            if (has_decimal) break;
            has_decimal = true;
        }
        value = value * 10.0f + (css_value[i] - '0');
        if (has_decimal) value /= 10.0f;
        i++;
    }

    if (has_minus) value = -value;
    return value;
}

static bool parse_color_value(const std::string& css_value, clever::css::Color& out_color) {
    if (css_value.empty()) return false;

    std::string lower = css_value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower[0] == '#') {
        std::string hex = lower.substr(1);
        if (hex.length() == 6 || hex.length() == 8) {
            unsigned long val = std::stoul(hex, nullptr, 16);
            if (hex.length() == 6) {
                out_color.r = (val >> 16) & 0xFF;
                out_color.g = (val >> 8) & 0xFF;
                out_color.b = val & 0xFF;
                out_color.a = 255;
            } else {
                out_color.r = (val >> 24) & 0xFF;
                out_color.g = (val >> 16) & 0xFF;
                out_color.b = (val >> 8) & 0xFF;
                out_color.a = val & 0xFF;
            }
            return true;
        }
    }

    if (lower.find("rgb") == 0) {
        size_t start = lower.find('(');
        size_t end = lower.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string content = lower.substr(start + 1, end - start - 1);
            std::istringstream iss(content);
            std::string part;
            int rgb_idx = 0;
            std::vector<int> rgb_vals;

            while (std::getline(iss, part, ',') && rgb_idx < 4) {
                part.erase(0, part.find_first_not_of(" \t"));
                part.erase(part.find_last_not_of(" \t") + 1);
                part.erase(std::remove_if(part.begin(), part.end(), [](char c) { return std::isalpha(c); }), part.end());
                if (!part.empty()) {
                    try {
                        rgb_vals.push_back(std::stoi(part));
                    } catch (...) {}
                }
                rgb_idx++;
            }

            if (rgb_vals.size() >= 3) {
                out_color.r = std::clamp(rgb_vals[0], 0, 255);
                out_color.g = std::clamp(rgb_vals[1], 0, 255);
                out_color.b = std::clamp(rgb_vals[2], 0, 255);
                out_color.a = rgb_vals.size() > 3 ? std::clamp(rgb_vals[3], 0, 255) : 255;
                return true;
            }
        }
    }

    return false;
}

void AnimationController::tick(double delta_ms) {
    active_updates_.clear();

    // Advance all animations and collect updates
    std::vector<size_t> to_remove;
    for (size_t i = 0; i < animations_.size(); ++i) {
        AnimationInstance& anim = animations_[i];

        if (anim.is_paused) {
            continue;
        }

        // Advance start time if this is the first tick
        if (anim.start_time_ms < 0) {
            anim.start_time_ms = 0;  // First tick initializes
        }

        // Calculate current elapsed time (accounting for delay)
        double total_elapsed = anim.start_time_ms + delta_ms;
        double delay_elapsed = std::max(0.0, total_elapsed - anim.delay_ms);

        // Check if animation is finished
        if (delay_elapsed >= anim.duration_ms && anim.iteration_count > 0) {
            // Animation complete — check fill mode
            if (anim.fill_mode == 1 || anim.fill_mode == 3) {  // forwards or both
                interpolate_animation(anim, anim.duration_ms);
            }
            to_remove.push_back(i);
            continue;
        }

        // Still within delay period
        if (delay_elapsed < 0) {
            anim.start_time_ms = anim.start_time_ms + delta_ms;
            continue;
        }

        // Interpolate animation
        interpolate_animation(anim, delay_elapsed);
        anim.start_time_ms = anim.start_time_ms + delta_ms;
    }

    // Remove completed animations
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
        const AnimationInstance& removed = animations_[*it];
        auto elem_it = element_animations_.find(removed.target);
        if (elem_it != element_animations_.end()) {
            auto& indices = elem_it->second;
            indices.erase(
                std::remove(indices.begin(), indices.end(), *it),
                indices.end()
            );
        }
        animations_.erase(animations_.begin() + *it);
    }
}

void AnimationController::interpolate_animation(AnimationInstance& anim, double elapsed_ms) {
    if (anim.duration_ms <= 0) {
        return;
    }

    // Calculate progress (0.0 to 1.0)
    double progress = elapsed_ms / anim.duration_ms;

    // Handle iteration count and direction
    if (anim.iteration_count > 0) {
        int completed_iterations = static_cast<int>(progress);
        float iteration_progress = progress - completed_iterations;

        // Check if we've completed all iterations
        if (completed_iterations >= static_cast<int>(anim.iteration_count)) {
            iteration_progress = 1.0f;
            anim.current_iteration = static_cast<int>(anim.iteration_count) - 1;
        } else {
            anim.current_iteration = completed_iterations;
        }

        // Handle direction (reverse, alternate, etc.)
        if (anim.direction == 1) {  // reverse
            iteration_progress = 1.0f - iteration_progress;
        } else if (anim.direction == 2) {  // alternate
            if (anim.current_iteration % 2 == 1) {
                iteration_progress = 1.0f - iteration_progress;
            }
        } else if (anim.direction == 3) {  // alternate-reverse
            if (anim.current_iteration % 2 == 0) {
                iteration_progress = 1.0f - iteration_progress;
            }
        }

        progress = iteration_progress;
    } else {
        progress = std::min(progress, 1.0);
    }

    progress = std::max(0.0, std::min(1.0, progress));

    // Apply timing function
    float eased_progress = apply_timing_function(anim, static_cast<float>(progress));

    // Create property update
    PropertyUpdate update;
    update.element = anim.target;
    update.property_name = anim.property;

    // Interpolate based on animation type
    if (anim.has_float) {
        update.float_value = interpolate_float(anim.from_value, anim.to_value, eased_progress);
        update.has_float = true;
    } else if (anim.has_color) {
        update.color_value = interpolate_color(anim.from_color, anim.to_color, eased_progress);
        update.has_color = true;
    } else if (anim.has_transform) {
        update.transform_value = interpolate_transform(anim.from_transform, anim.to_transform, eased_progress);
        update.has_transform = true;
    } else if (anim.has_length) {
        update.length_value = interpolate_float(anim.from_length, anim.to_length, eased_progress);
        update.has_length = true;
    }

    active_updates_.push_back(update);
}

float AnimationController::apply_timing_function(const AnimationInstance& anim, float progress) const {
    if (anim.timing_function == 5) {  // cubic-bezier
        return apply_easing_custom(anim.timing_function, progress,
                                   anim.bezier_x1, anim.bezier_y1,
                                   anim.bezier_x2, anim.bezier_y2,
                                   anim.steps_count);
    } else if (anim.timing_function == 6 || anim.timing_function == 7) {  // steps
        return apply_easing_custom(anim.timing_function, progress,
                                   0, 0, 1, 1,
                                   anim.steps_count);
    } else {
        return apply_easing(anim.timing_function, progress);
    }
}

void AnimationController::find_keyframe_range(const std::vector<clever::css::KeyframeStop>& steps,
                                             float progress,
                                             size_t& out_from_idx,
                                             size_t& out_to_idx,
                                             float& out_local_t) {
    out_from_idx = 0;
    out_to_idx = 0;
    out_local_t = 0.0f;

    if (steps.empty()) {
        return;
    }

    // Find the keyframe pair that bounds the current progress
    for (size_t i = 0; i < steps.size(); ++i) {
        if (steps[i].offset <= progress) {
            out_from_idx = i;
        }
        if (steps[i].offset >= progress && (i == 0 || out_to_idx == 0)) {
            out_to_idx = i;
            break;
        }
    }

    // If no upper bound found, use last keyframe
    if (out_to_idx == 0 && !steps.empty()) {
        out_to_idx = steps.size() - 1;
    }

    // Calculate local progress between keyframes
    if (out_from_idx == out_to_idx) {
        out_local_t = 0.0f;
    } else {
        float from_offset = steps[out_from_idx].offset;
        float to_offset = steps[out_to_idx].offset;
        if (to_offset > from_offset) {
            out_local_t = (progress - from_offset) / (to_offset - from_offset);
        } else {
            out_local_t = 0.0f;
        }
    }
}

bool AnimationController::extract_property_value(const clever::css::ComputedStyle& style,
                                                const std::string& property,
                                                float& out_value,
                                                clever::css::Color& out_color,
                                                clever::css::Transform& out_transform) {
    if (property == "opacity") {
        out_value = style.opacity;
        return true;
    } else if (property == "color") {
        out_color = style.color;
        return true;
    } else if (property == "background-color") {
        out_color = style.background_color;
        return true;
    } else if (property == "font-size") {
        out_value = style.font_size.to_px();
        return true;
    } else if (property == "border-radius") {
        out_value = style.border_radius;
        return true;
    } else if (property == "transform") {
        if (!style.transforms.empty()) {
            out_transform = style.transforms[0];
        }
        return true;
    }
    return false;
}

void AnimationController::start_animation(clever::layout::LayoutNode* element,
                                         const std::string& animation_name,
                                         const clever::css::KeyframesDefinition& keyframes,
                                         const clever::css::ComputedStyle& style) {
    if (!element || keyframes.rules.empty()) {
        return;
    }

    std::set<std::string> properties_to_animate;
    for (const auto& step : keyframes.rules) {
        for (const auto& prop_pair : step.declarations) {
            properties_to_animate.insert(prop_pair.first);
        }
    }

    for (const auto& property : properties_to_animate) {
        AnimationInstance anim;
        anim.target = element;
        anim.property = property;
        anim.animation_name = animation_name;
        anim.start_time_ms = 0;
        anim.duration_ms = style.animation_duration * 1000.0;
        anim.delay_ms = style.animation_delay * 1000.0;
        anim.timing_function = style.animation_timing;
        anim.bezier_x1 = style.animation_bezier_x1;
        anim.bezier_y1 = style.animation_bezier_y1;
        anim.bezier_x2 = style.animation_bezier_x2;
        anim.bezier_y2 = style.animation_bezier_y2;
        anim.steps_count = style.animation_steps_count;
        anim.fill_mode = style.animation_fill_mode;
        anim.direction = style.animation_direction;
        anim.iteration_count = style.animation_iteration_count;
        anim.is_transition = false;

        if (keyframes.rules.size() >= 2) {
            const auto& first_step = keyframes.rules.front();
            const auto& last_step = keyframes.rules.back();

            auto first_it = std::find_if(first_step.declarations.begin(), first_step.declarations.end(),
                                        [&property](const auto& p) { return p.first == property; });
            auto last_it = std::find_if(last_step.declarations.begin(), last_step.declarations.end(),
                                       [&property](const auto& p) { return p.first == property; });

            if (first_it != first_step.declarations.end() && last_it != last_step.declarations.end()) {
                const std::string& from_css = first_it->second;
                const std::string& to_css = last_it->second;

                if (property == "opacity") {
                    anim.from_value = parse_length_value(from_css);
                    anim.to_value = parse_length_value(to_css);
                    anim.has_float = true;
                } else if (property == "color" || property == "background-color") {
                    if (parse_color_value(from_css, anim.from_color) &&
                        parse_color_value(to_css, anim.to_color)) {
                        anim.has_color = true;
                    } else {
                        anim.has_float = true;
                        anim.from_value = 0.0f;
                        anim.to_value = 1.0f;
                    }
                } else if (property == "font-size" || property == "border-radius" ||
                           property == "width" || property == "height" ||
                           property == "margin-top" || property == "margin-right" ||
                           property == "margin-bottom" || property == "margin-left" ||
                           property == "padding-top" || property == "padding-right" ||
                           property == "padding-bottom" || property == "padding-left") {
                    anim.from_length = parse_length_value(from_css);
                    anim.to_length = parse_length_value(to_css);
                    anim.has_length = true;
                } else if (property == "transform") {
                    anim.from_transform = clever::css::Transform();
                    anim.to_transform = clever::css::Transform();
                    anim.has_transform = true;
                } else {
                    anim.from_value = parse_length_value(from_css);
                    anim.to_value = parse_length_value(to_css);
                    anim.has_float = true;
                }
            } else {
                anim.from_value = 0.0f;
                anim.to_value = 1.0f;
                anim.has_float = true;
            }
        }

        animations_.push_back(anim);
        element_animations_[element].push_back(animations_.size() - 1);
    }
}

void AnimationController::start_transition(clever::layout::LayoutNode* element,
                                          const std::string& property,
                                          float from_value,
                                          float to_value,
                                          const clever::css::TransitionDef& transition) {
    if (!element || transition.duration_ms <= 0) {
        return;
    }

    AnimationInstance anim;
    anim.target = element;
    anim.property = property;
    anim.start_time_ms = 0;
    anim.duration_ms = transition.duration_ms;
    anim.delay_ms = transition.delay_ms;
    anim.timing_function = transition.timing_function;
    anim.bezier_x1 = transition.bezier_x1;
    anim.bezier_y1 = transition.bezier_y1;
    anim.bezier_x2 = transition.bezier_x2;
    anim.bezier_y2 = transition.bezier_y2;
    anim.steps_count = transition.steps_count;
    anim.from_value = from_value;
    anim.to_value = to_value;
    anim.has_float = true;
    anim.is_transition = true;

    animations_.push_back(anim);
    element_animations_[element].push_back(animations_.size() - 1);
}

void AnimationController::start_color_transition(clever::layout::LayoutNode* element,
                                                const std::string& property,
                                                const clever::css::Color& from_color,
                                                const clever::css::Color& to_color,
                                                const clever::css::TransitionDef& transition) {
    if (!element || transition.duration_ms <= 0) {
        return;
    }

    AnimationInstance anim;
    anim.target = element;
    anim.property = property;
    anim.start_time_ms = 0;
    anim.duration_ms = transition.duration_ms;
    anim.delay_ms = transition.delay_ms;
    anim.timing_function = transition.timing_function;
    anim.bezier_x1 = transition.bezier_x1;
    anim.bezier_y1 = transition.bezier_y1;
    anim.bezier_x2 = transition.bezier_x2;
    anim.bezier_y2 = transition.bezier_y2;
    anim.steps_count = transition.steps_count;
    anim.from_color = from_color;
    anim.to_color = to_color;
    anim.has_color = true;
    anim.is_transition = true;

    animations_.push_back(anim);
    element_animations_[element].push_back(animations_.size() - 1);
}

void AnimationController::start_transform_transition(clever::layout::LayoutNode* element,
                                                    const std::string& property,
                                                    const clever::css::Transform& from_transform,
                                                    const clever::css::Transform& to_transform,
                                                    const clever::css::TransitionDef& transition) {
    if (!element || transition.duration_ms <= 0) {
        return;
    }

    AnimationInstance anim;
    anim.target = element;
    anim.property = property;
    anim.start_time_ms = 0;
    anim.duration_ms = transition.duration_ms;
    anim.delay_ms = transition.delay_ms;
    anim.timing_function = transition.timing_function;
    anim.bezier_x1 = transition.bezier_x1;
    anim.bezier_y1 = transition.bezier_y1;
    anim.bezier_x2 = transition.bezier_x2;
    anim.bezier_y2 = transition.bezier_y2;
    anim.steps_count = transition.steps_count;
    anim.from_transform = from_transform;
    anim.to_transform = to_transform;
    anim.has_transform = true;
    anim.is_transition = true;

    animations_.push_back(anim);
    element_animations_[element].push_back(animations_.size() - 1);
}

void AnimationController::remove_animations_for_element(clever::layout::LayoutNode* element) {
    auto it = element_animations_.find(element);
    if (it != element_animations_.end()) {
        // Remove animations in reverse order to avoid index shifting
        std::vector<size_t>& indices = it->second;
        std::sort(indices.rbegin(), indices.rend());
        for (size_t idx : indices) {
            if (idx < animations_.size()) {
                animations_.erase(animations_.begin() + idx);
            }
        }
        element_animations_.erase(it);
    }
}

bool AnimationController::is_animating(const clever::layout::LayoutNode* element) const {
    auto it = element_animations_.find(const_cast<clever::layout::LayoutNode*>(element));
    return it != element_animations_.end() && !it->second.empty();
}

} // namespace clever::paint
