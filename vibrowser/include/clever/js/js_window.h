#pragma once
#include <string>

struct JSContext;

namespace clever::js {

// Install window global with basic properties.
void install_window_bindings(JSContext* ctx, const std::string& url,
                             int width, int height,
                             float device_pixel_ratio = 1.0f);

} // namespace clever::js
