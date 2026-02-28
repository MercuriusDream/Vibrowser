# Clever JavaScript Engine bindings
# Wraps QuickJS and provides DOM/console integration

add_library(vibrowser_js STATIC
    cors_policy.cpp
    js_engine.cpp
    js_dom_bindings.cpp
    js_fetch_bindings.cpp
    js_timers.cpp
    js_window.cpp
)

target_include_directories(vibrowser_js PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(vibrowser_js PUBLIC
    quickjs
    vibrowser_dom
    vibrowser_html
    vibrowser_net
    vibrowser_css_parser
    vibrowser_css_style
)

# Suppress C99 compound literal warnings from QuickJS macros (JS_NULL, JS_UNDEFINED)
target_compile_options(vibrowser_js PRIVATE -Wno-c99-extensions)
