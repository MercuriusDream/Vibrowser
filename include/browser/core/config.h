#ifndef BROWSER_CORE_CONFIG_H
#define BROWSER_CORE_CONFIG_H

#include <cstdint>

namespace browser::core::config {

inline constexpr std::uint32_t kDefaultViewportWidth = 1280;
inline constexpr std::uint32_t kDefaultViewportHeight = 720;
inline constexpr const char kDefaultUserAgent[] =
    "from_scratch_browser/0.1 (StaticHTMLCSS)";

}  // namespace browser::core::config

#endif  // BROWSER_CORE_CONFIG_H
