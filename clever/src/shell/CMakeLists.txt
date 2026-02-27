# Vibrowser — macOS Shell Application
# Objective-C++ for AppKit integration

add_executable(clever_browser
    main.mm
    browser_window.mm
    render_view.mm
)

# Enable Objective-C++ and ARC
target_compile_options(clever_browser PRIVATE -fobjc-arc)
set_target_properties(clever_browser PROPERTIES
    LINKER_LANGUAGE CXX
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Vibrowser"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.vibrowser.browser"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

target_include_directories(clever_browser PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(clever_browser PRIVATE
    clever_paint
    clever_layout
    clever_html
    clever_css_style
    clever_css_parser
    clever_net
    clever_url
    "-framework Cocoa"
    "-framework AppKit"
    "-framework QuartzCore"
    "-framework UniformTypeIdentifiers"
)
