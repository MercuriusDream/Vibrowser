# Vibrowser — macOS Shell Application
# Objective-C++ for AppKit integration

add_executable(vibrowser
    main.mm
    browser_window.mm
    render_view.mm
)

# Enable Objective-C++ and ARC
target_compile_options(vibrowser PRIVATE -fobjc-arc)
set_target_properties(vibrowser PROPERTIES
    LINKER_LANGUAGE CXX
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Vibrowser"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.vibrowser.browser"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
)

target_include_directories(vibrowser PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(vibrowser PRIVATE
    vibrowser_paint
    "-framework Cocoa"
    "-framework AppKit"
    "-framework QuartzCore"
    "-framework UniformTypeIdentifiers"
)
