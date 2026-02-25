# Clever JavaScript Engine bindings
# Wraps QuickJS and provides DOM/console integration

add_library(clever_js STATIC
    cors_policy.cpp
    js_engine.cpp
    js_dom_bindings.cpp
    js_fetch_bindings.cpp
    js_timers.cpp
    js_window.cpp
)

target_include_directories(clever_js PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(clever_js PUBLIC
    quickjs
    clever_dom
    clever_html
    clever_net
    clever_css_parser
    clever_css_style
)

# Suppress C99 compound literal warnings from QuickJS macros (JS_NULL, JS_UNDEFINED)
target_compile_options(clever_js PRIVATE -Wno-c99-extensions)
