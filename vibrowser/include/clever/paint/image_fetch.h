#pragma once
#include <memory>
#include <string>
#include <vector>

namespace clever::paint {

// Image data returned by fetch_image_for_js (for JS Image element load events)
struct JSImageData {
    std::shared_ptr<std::vector<uint8_t>> pixels; // RGBA pixel data, nullptr on failure
    int width = 0;
    int height = 0;
    bool success() const { return pixels && !pixels->empty() && width > 0 && height > 0; }
};

// Fetch and decode an image URL (synchronous). Used by JS Image element.
// Returns pixel data with naturalWidth/naturalHeight on success, or empty on failure.
JSImageData fetch_image_for_js(const std::string& url);

} // namespace clever::paint
