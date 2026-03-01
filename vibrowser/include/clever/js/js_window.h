#pragma once
#include <string>
#include <utility>

struct JSContext;

namespace clever::js {

// Install window global with basic properties.
void install_window_bindings(JSContext* ctx, const std::string& url,
                             int width, int height,
                             float device_pixel_ratio = 1.0f);

// Execute all pending requestAnimationFrame callbacks with a DOMHighResTimeStamp.
// Callbacks registered during the flush are deferred to the next call.
void flush_animation_frames(JSContext* ctx);

// Free all pending rAF callback references without executing them.
// Call once when tearing down the JS context to avoid GC assertion failures.
void cleanup_animation_frames(JSContext* ctx);

// Look up a blob: URL in the blob registry.
// Returns a pointer to (mime_type, data) pair, or nullptr if not found.
// The pointer is valid until the blob is revoked or the registry is modified.
const std::pair<std::string, std::string>* get_blob_data(const std::string& url);

} // namespace clever::js
