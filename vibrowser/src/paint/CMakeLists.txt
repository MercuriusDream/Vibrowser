file(GLOB PAINT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB PAINT_OBJCXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")

set(ALL_PAINT_SOURCES ${PAINT_SOURCES} ${PAINT_OBJCXX_SOURCES})

if(ALL_PAINT_SOURCES)
    add_library(vibrowser_paint STATIC ${ALL_PAINT_SOURCES})
else()
    add_library(vibrowser_paint STATIC empty.cpp)
endif()

# Ensure .mm files compile as Objective-C++
if(PAINT_OBJCXX_SOURCES)
    set_source_files_properties(${PAINT_OBJCXX_SOURCES} PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

# Third-party STB implementation units trigger compiler warnings that are
# upstream style choices (zero-init aggregates, deprecated sprintf path).
# Keep warnings enabled globally while silencing these implementation files.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/stb_image_write_impl.cpp
    PROPERTIES
    COMPILE_OPTIONS "-Wno-missing-field-initializers;-Wno-deprecated-declarations"
)

target_include_directories(vibrowser_paint PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(vibrowser_paint PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
target_include_directories(vibrowser_paint PRIVATE ${CMAKE_SOURCE_DIR}/third_party/nanosvg)
target_link_libraries(vibrowser_paint PUBLIC vibrowser_layout vibrowser_html vibrowser_css_style vibrowser_net vibrowser_js)

# Link macOS frameworks for CoreText text rendering
if(APPLE)
    target_link_libraries(vibrowser_paint PUBLIC
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
    )
endif()
