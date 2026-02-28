#pragma once

#include <string>

struct JSContext;

namespace clever::js {

// Initialize Web Storage API (localStorage and sessionStorage)
// origin: used as key for localStorage persistence (e.g., "http://localhost:8080")
void install_web_storage_bindings(JSContext* ctx, const std::string& origin);

} // namespace clever::js
