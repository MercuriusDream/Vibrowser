# Wave0-B Build Graph (root + clever)

## Scope
- Root project: `CMakeLists.txt` at repository root (`from_scratch_browser`)
- Clever project: `clever/CMakeLists.txt` and subdirectory CMake files

## Root (`/CMakeLists.txt`)

### Source globs and source sets
- `BROWSER_APP_SOURCES`: `src/app/*.cpp` (recursive)
- `BROWSER_CORE_SOURCES`: `src/core/*.cpp` (recursive)
- `BROWSER_HTML_SOURCES`: `src/html/*.cpp` (recursive)
- `BROWSER_CSS_SOURCES`: `src/css/*.cpp` (recursive)
- `BROWSER_JS_SOURCES`: `src/js/*.cpp` (recursive)
- `BROWSER_LAYOUT_SOURCES`: `src/layout/*.cpp` (recursive)
- `BROWSER_RENDER_SOURCES`: `src/render/*.cpp` (recursive)
- `BROWSER_NET_SOURCES`: `src/net/*.cpp` (recursive)
- `BROWSER_BROWSER_SOURCES`: `src/browser/*.cpp` (recursive)
- `BROWSER_ENGINE_SOURCES`: `src/engine/*.cpp` (recursive)
- `BROWSER_UTILS_SOURCES`: `src/utils/*.cpp` (recursive)
- `TEST_SOURCES`: `tests/test_*.cpp`

### Targets
- `from_scratch_browser` (executable)
  - Sources: all `BROWSER_*_SOURCES` above + `src/main.cpp` if present
  - Fallback entrypoint: generated `bootstrap_main.cpp` if `src/main.cpp` is missing
- Test executables (one per `tests/test_*.cpp`; target name = filename stem):
  - `test_bridge_mutations`
  - `test_bridge_queries`
  - `test_cache_policy`
  - `test_cancel_navigation`
  - `test_deterministic_lifecycle`
  - `test_deterministic_parsing`
  - `test_deterministic_styling`
  - `test_event_dispatch`
  - `test_failure_traces`
  - `test_geometry_stability`
  - `test_lifecycle_diagnostics`
  - `test_linked_css`
  - `test_malformed_html_recovery`
  - `test_milestone_evidence`
  - `test_module_contracts`
  - `test_mutation_regression`
  - `test_parse_style_consistency`
  - `test_privacy_controls`
  - `test_recovery_replay`
  - `test_render_artifacts`
  - `test_render_modes`
  - `test_render_trace`
  - `test_request_boundary`
  - `test_request_contracts`
  - `test_request_policy`
  - `test_rerender_mutations`
  - `test_retry_navigation`
  - `test_selector_fallback`
  - `test_stable_layout`
  - `test_structured_diagnostics`
  - Each test links by compilation unit composition (not target link): `test_*.cpp + BROWSER_LIB_SOURCES`.

### Dependencies
- Optional external dependency:
  - `OpenSSL::SSL`, `OpenSSL::Crypto` when `find_package(OpenSSL QUIET)` succeeds.
- Shared compile settings:
  - `cxx_std_17`, `CXX_EXTENSIONS OFF` on app + tests.

### Platform/conditional gates
- `if(MSVC)`:
  - `from_scratch_browser` uses `/W4 /permissive- /EHsc`
  - Non-MSVC tests use `-Wall -Wextra -Wpedantic` (no MSVC branch for test warnings)
- `if(NOT MSVC)`:
  - `from_scratch_browser` and tests use `-Wall -Wextra -Wpedantic`
- `if(OpenSSL_FOUND)`:
  - link OpenSSL and define `BROWSER_USE_OPENSSL=1` for app + tests
- `if(EXISTS src/main.cpp)` / `if(NOT EXISTS src/main.cpp)`:
  - real `main.cpp` vs generated bootstrap entrypoint
- No `enable_testing()` / `add_test(...)` in root CMake (tests are executable targets only)

## Clever (`/clever/CMakeLists.txt` + subdirs)

### Top-level orchestration
- Adds subdirectories (in order):
  - `third_party/quickjs`
  - `src/url`
  - `src/platform`
  - `src/ipc`
  - `src/net`
  - `src/dom`
  - `src/html`
  - `src/css/parser`
  - `src/css/style`
  - `src/js`
  - `src/layout`
  - `src/paint`
  - `src/shell` (APPLE only)
  - `tests`
- Fetches GoogleTest via `FetchContent` (`v1.15.2`) and calls `enable_testing()`.

### Library and app targets
- `quickjs` (STATIC, third-party)
  - Sources: `quickjs.c`, `cutils.c`, `dtoa.c`, `libregexp.c`, `libunicode.c`
- `clever_url` (STATIC)
  - Sources: `url_parser.cpp`, `percent_encoding.cpp`, `idna.cpp`, `url.cpp`
- `clever_platform` (STATIC)
  - Sources: `thread_pool.cpp`, `event_loop.cpp`, `timer.cpp`
- `clever_ipc` (STATIC)
  - Sources: `message_pipe.cpp`, `message_channel.cpp`, `serializer.cpp`
- `clever_net` (STATIC)
  - Sources: `http_client.cpp`, `connection_pool.cpp`, `cookie_jar.cpp`, `header_map.cpp`, `request.cpp`, `response.cpp`, `tls_socket.cpp`
- `clever_dom` (STATIC)
  - Sources: `node.cpp`, `element.cpp`, `document.cpp`, `text.cpp`, `comment.cpp`, `event.cpp`
- `clever_html` (STATIC)
  - Sources: `tokenizer.cpp`, `tree_builder.cpp`, `simple_node.cpp`
- `clever_css_parser` (STATIC)
  - Source glob: `src/css/parser/*.cpp`; fallback `empty.cpp` when glob empty
- `clever_css_style` (STATIC)
  - Source glob: `src/css/style/*.cpp`; fallback `empty.cpp` when glob empty
- `clever_js` (STATIC)
  - Sources: `js_engine.cpp`, `js_dom_bindings.cpp`, `js_fetch_bindings.cpp`, `js_timers.cpp`, `js_window.cpp`
- `clever_layout` (STATIC)
  - Sources: `box.cpp`, `layout_engine.cpp`
- `clever_paint` (STATIC)
  - Source globs: `src/paint/*.cpp` + `src/paint/*.mm`; fallback `empty.cpp` when both empty
  - Current Objective-C++ source present via glob: `text_renderer.mm`
- `clever_browser` (executable, macOS bundle, APPLE only)
  - Sources: `main.mm`, `browser_window.mm`, `render_view.mm`

### Target dependency edges (`target_link_libraries`)
- `clever_url` -> `ICU::uc`, `ICU::i18n` (if `ICU_FOUND`) OR `PkgConfig::ICU` (if `TARGET PkgConfig::ICU`)
- `clever_platform` -> `Threads::Threads`
- `clever_ipc` -> `clever_platform`
- `clever_net` -> `clever_url`, `clever_platform`, `ZLIB::ZLIB`
- `clever_html` -> `clever_dom`
- `clever_css_style` -> `clever_css_parser`, `clever_dom`
- `clever_js` -> `quickjs`, `clever_dom`, `clever_html`, `clever_net`, `clever_css_parser`, `clever_css_style`
- `clever_layout` -> `clever_css_style`
- `clever_paint` -> `clever_layout`, `clever_html`, `clever_css_style`, `clever_net`, `clever_js`
- `clever_browser` -> `clever_paint`, `clever_layout`, `clever_html`, `clever_css_style`, `clever_css_parser`, `clever_net`, `clever_url`

### Test executables and entrypoints (`clever/tests/unit/CMakeLists.txt`)
- `clever_url_tests`
  - Entrypoints: `url_parser_test.cpp`, `percent_encoding_test.cpp`
  - Links: `clever_url`, `GTest::gtest_main`
  - CTest: `add_test(NAME url_tests COMMAND clever_url_tests)`
- `clever_platform_tests`
  - Entrypoints: `thread_pool_test.cpp`, `event_loop_test.cpp`
  - Links: `clever_platform`, `GTest::gtest_main`
  - CTest: `platform_tests`
- `clever_ipc_tests`
  - Entrypoints: `message_pipe_test.cpp`, `message_channel_test.cpp`, `serializer_test.cpp`
  - Links: `clever_ipc`, `GTest::gtest_main`
  - CTest: `ipc_tests`
- `clever_net_tests`
  - Entrypoint: `http_client_test.cpp`
  - Links: `clever_net`, `GTest::gtest_main`
  - CTest: `net_tests`
- `clever_dom_tests` (if `dom_test.cpp` exists)
  - Entrypoint: `dom_test.cpp`
  - Links: `clever_dom`, `GTest::gtest_main`
  - CTest: `dom_tests`
- `clever_html_tests` (if `html_parser_test.cpp` exists)
  - Entrypoint: `html_parser_test.cpp`
  - Links: `clever_html`, `clever_dom`, `GTest::gtest_main`
  - CTest: `html_tests`
- `clever_css_parser_tests` (if `css_parser_test.cpp` exists)
  - Entrypoint: `css_parser_test.cpp`
  - Links: `clever_css_parser`, `GTest::gtest_main`
  - CTest: `css_parser_tests`
- `clever_css_style_tests` (if `css_style_test.cpp` exists)
  - Entrypoint: `css_style_test.cpp`
  - Links: `clever_css_style`, `clever_css_parser`, `GTest::gtest_main`
  - CTest: `css_style_tests`
- `clever_layout_tests` (if `layout_test.cpp` exists)
  - Entrypoint: `layout_test.cpp`
  - Links: `clever_layout`, `GTest::gtest_main`
  - CTest: `layout_tests`
- `clever_paint_tests` (if `paint_test.cpp` exists)
  - Entrypoint: `paint_test.cpp`
  - Links: `clever_paint`, `clever_layout`, `clever_html`, `clever_css_style`, `GTest::gtest_main`
  - CTest: `paint_tests`
- `clever_native_image_tests` (APPLE and `native_image_test.mm` exists)
  - Entrypoint: `native_image_test.mm`
  - Links: `GTest::gtest_main` + macOS frameworks (CoreFoundation, CoreGraphics, ImageIO)
  - CTest: `native_image_tests`
- `clever_js_tests` (if `js_engine_test.cpp` exists)
  - Entrypoint: `js_engine_test.cpp`
  - Links: `clever_js`, `clever_html`, `clever_dom`, `quickjs`, `GTest::gtest_main`
  - CTest: `js_tests`

### Clever platform/build gates
- Build-type gates:
  - `Release`: global `-Werror`
  - `Release`: enable IPO/LTO when `check_ipo_supported(...)` returns true
- Sanitizer options (off by default):
  - `CLEVER_ENABLE_ASAN`, `CLEVER_ENABLE_UBSAN`, `CLEVER_ENABLE_TSAN`
- Dependency resolution gates:
  - `ICU_FOUND` preferred; pkg-config ICU fallback
- Platform gates (`APPLE`):
  - Build `src/shell` (`clever_browser`) only on macOS
  - `quickjs`: adds `CONFIG_BIGNUM` compile definition
  - `clever_net`: links Security/CoreFoundation frameworks
  - `clever_paint`: links CoreText/CoreGraphics/CoreFoundation/ImageIO frameworks
  - `clever_native_image_tests`: built only on macOS
- Source-presence gates:
  - Multiple tests created only if corresponding `*_test.cpp` exists
  - CSS parser/style and paint libraries fall back to `empty.cpp` if source globs are empty
