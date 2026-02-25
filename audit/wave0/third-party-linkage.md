# Wave0-C Third-Party Inventory and Linkage Map

## Scope
- `clever/third_party/*` inventory
- External deps declared in root `CMakeLists.txt` and `clever` CMake files
- Include usage scan across: `src/`, `tests/`, `clever/src/`, `clever/include/`, `clever/tests/unit/`

## Vendored Inventory (`clever/third_party`)

| Dependency | Location | Type | Version Signal | Build Integration |
| --- | --- | --- | --- | --- |
| QuickJS | `clever/third_party/quickjs` | Vendored source library | `CONFIG_VERSION="2025-09-13"` in `clever/third_party/quickjs/CMakeLists.txt` | Built as `quickjs` static target (`add_library(quickjs STATIC ...)`) and exposed with `target_include_directories(... SYSTEM PUBLIC ...)` |
| stb_image | `clever/third_party/stb/stb_image.h` | Vendored header-only | Header banner: `stb_image - v2.30` | Consumed via include path injected by `clever_paint` |
| stb_image_write | `clever/third_party/stb/stb_image_write.h` | Vendored header-only | Header banner: `stb_image_write - v1.16` | Consumed via include path injected by `clever_paint` |

## External Dependency Declarations (CMake)

### Root project (`/CMakeLists.txt`)
- OpenSSL (optional):
  - `find_package(OpenSSL QUIET)` at line 93
  - Linked to app at line 95: `OpenSSL::SSL OpenSSL::Crypto`
  - Linked to each root test target at line 126 when found
  - Compile define toggled: `BROWSER_USE_OPENSSL=1`

### Clever project (`/clever`)
- ICU (optional): `find_package(ICU COMPONENTS uc i18n data QUIET)` in `clever/CMakeLists.txt:46`
- ICU fallback: `pkg_check_modules(ICU IMPORTED_TARGET icu-uc icu-i18n)` in `clever/CMakeLists.txt:51`
- GoogleTest (fetched): `FetchContent_Declare(googletest ... GIT_TAG v1.15.2)` in `clever/CMakeLists.txt:57-63`
- QuickJS (vendored): `add_subdirectory(third_party/quickjs)` in `clever/CMakeLists.txt:70`
- ZLIB (required for `clever_net`): `find_package(ZLIB REQUIRED)` in `clever/src/net/CMakeLists.txt:15`
- Threads (required for `clever_platform`): `find_package(Threads REQUIRED)` in `clever/src/platform/CMakeLists.txt:11`
- macOS frameworks (where applicable):
  - `clever_net`: `Security`, `CoreFoundation` (`clever/src/net/CMakeLists.txt:20`)
  - `clever_paint`: `CoreText`, `CoreGraphics`, `CoreFoundation`, `ImageIO` (`clever/src/paint/CMakeLists.txt:25-29`)
  - `clever_browser`: `Cocoa`, `AppKit`, `QuartzCore`, `UniformTypeIdentifiers` (`clever/src/shell/CMakeLists.txt:32-35`)

## Linkage + Include Call-Sites by Component

### Component: Root networking (`from_scratch_browser`)
- Linked deps:
  - `OpenSSL::SSL`, `OpenSSL::Crypto` (optional via `OpenSSL_FOUND`)
- Include call-sites:
  - `src/net/http_client.cpp:26` -> `<openssl/ssl.h>`
- Notes:
  - Code paths are gated with `#ifdef BROWSER_USE_OPENSSL`.

### Component: `clever_url`
- Linked deps:
  - `ICU::uc`, `ICU::i18n` (or `PkgConfig::ICU` fallback)
- Include call-sites:
  - No direct ICU header includes found in `clever/src/url/*` or `clever/include/clever/url/*`.
- Notes:
  - `idna.cpp` currently uses ASCII-only fallback logic and documents future ICU integration.

### Component: `clever_net`
- Linked deps:
  - `ZLIB::ZLIB`
  - macOS frameworks: `Security`, `CoreFoundation`
- Include call-sites:
  - `clever/src/net/response.cpp:7` -> `<zlib.h>`
  - `clever/include/clever/net/tls_socket.h:13` -> `<Security/Security.h>`
  - `clever/include/clever/net/tls_socket.h:14` -> `<Security/SecureTransport.h>`

### Component: `clever_js`
- Linked deps:
  - `quickjs` (vendored)
- Include call-sites:
  - `clever/src/js/js_engine.cpp:4` -> `<quickjs.h>`
  - `clever/src/js/js_dom_bindings.cpp:5` -> `<quickjs.h>`
  - `clever/src/js/js_fetch_bindings.cpp:4` -> `<quickjs.h>`
  - `clever/src/js/js_timers.cpp:4` -> `<quickjs.h>`
  - `clever/src/js/js_window.cpp:4` -> `<quickjs.h>`
- Additional external include usage in this component:
  - `clever/src/js/js_dom_bindings.cpp:23` -> `<CommonCrypto/CommonDigest.h>`

### Component: `clever_paint`
- Linked deps:
  - Header include path to vendored STB: `target_include_directories(clever_paint PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)`
  - macOS frameworks: `CoreText`, `CoreGraphics`, `CoreFoundation`, `ImageIO`
- Include call-sites:
  - `clever/src/paint/render_pipeline.cpp:20` -> `<stb_image.h>`
  - `clever/src/paint/stb_image_impl.cpp:7` -> `"stb_image.h"`
  - `clever/src/paint/software_renderer.cpp:3` -> `<stb_image_write.h>`
  - `clever/src/paint/stb_image_write_impl.cpp:2` -> `"stb_image_write.h"`
  - `clever/src/paint/render_pipeline.cpp:22` -> `<CoreFoundation/CoreFoundation.h>`
  - `clever/src/paint/render_pipeline.cpp:23` -> `<CoreGraphics/CoreGraphics.h>`
  - `clever/src/paint/render_pipeline.cpp:24` -> `<ImageIO/ImageIO.h>`
  - `clever/src/paint/text_renderer.mm:7` -> `<CoreFoundation/CoreFoundation.h>`
  - `clever/src/paint/text_renderer.mm:8` -> `<CoreGraphics/CoreGraphics.h>`
  - `clever/src/paint/text_renderer.mm:9` -> `<CoreText/CoreText.h>`

### Component: `clever_browser` (shell app)
- Linked deps:
  - macOS frameworks: `Cocoa`, `AppKit`, `QuartzCore`, `UniformTypeIdentifiers`
- Include/import call-sites:
  - `clever/src/shell/main.mm:1` -> `<Cocoa/Cocoa.h>`
  - `clever/src/shell/browser_window.h:2` -> `<Cocoa/Cocoa.h>`
  - `clever/src/shell/render_view.h:2` -> `<Cocoa/Cocoa.h>`
  - `clever/src/shell/browser_window.mm:3` -> `<UniformTypeIdentifiers/UniformTypeIdentifiers.h>`
  - `clever/src/shell/browser_window.mm:4` -> `<QuartzCore/QuartzCore.h>`
  - `clever/src/shell/render_view.mm:3` -> `<QuartzCore/CABase.h>`

### Component: Clever unit tests (`clever/tests/unit`)
- Linked deps:
  - `GTest::gtest_main` across all unit test targets
  - `quickjs` additionally linked by `clever_js_tests`
  - macOS frameworks (`CoreFoundation`, `CoreGraphics`, `ImageIO`) linked by `clever_native_image_tests`
- Include call-sites:
  - `<gtest/gtest.h>` appears in:
    - `clever/tests/unit/css_parser_test.cpp`
    - `clever/tests/unit/css_style_test.cpp`
    - `clever/tests/unit/dom_test.cpp`
    - `clever/tests/unit/event_loop_test.cpp`
    - `clever/tests/unit/html_parser_test.cpp`
    - `clever/tests/unit/http_client_test.cpp`
    - `clever/tests/unit/js_engine_test.cpp`
    - `clever/tests/unit/layout_test.cpp`
    - `clever/tests/unit/message_channel_test.cpp`
    - `clever/tests/unit/message_pipe_test.cpp`
    - `clever/tests/unit/native_image_test.mm`
    - `clever/tests/unit/paint_test.cpp`
    - `clever/tests/unit/percent_encoding_test.cpp`
    - `clever/tests/unit/serializer_test.cpp`
    - `clever/tests/unit/thread_pool_test.cpp`
    - `clever/tests/unit/url_parser_test.cpp`
  - `clever/tests/unit/http_client_test.cpp:10` -> `<zlib.h>`
  - `clever/tests/unit/native_image_test.mm:4` -> `<CoreFoundation/CoreFoundation.h>`
  - `clever/tests/unit/native_image_test.mm:5` -> `<CoreGraphics/CoreGraphics.h>`
  - `clever/tests/unit/native_image_test.mm:6` -> `<ImageIO/ImageIO.h>`

## Gaps / Observations
- `clever_url` is CMake-linked to ICU, but no direct ICU header usage is currently present in source/header call-sites.
- Root tests inherit OpenSSL linkage conditionally via CMake, but OpenSSL include usage is centralized in `src/net/http_client.cpp`.
- `stb` is vendored header-only (no standalone CMake target); linkage is via include path scoping on `clever_paint`.
