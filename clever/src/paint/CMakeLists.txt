file(GLOB PAINT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB PAINT_OBJCXX_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.mm")

set(ALL_PAINT_SOURCES ${PAINT_SOURCES} ${PAINT_OBJCXX_SOURCES})

if(ALL_PAINT_SOURCES)
    add_library(clever_paint STATIC ${ALL_PAINT_SOURCES})
else()
    add_library(clever_paint STATIC empty.cpp)
endif()

# Ensure .mm files compile as Objective-C++
if(PAINT_OBJCXX_SOURCES)
    set_source_files_properties(${PAINT_OBJCXX_SOURCES} PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
endif()

target_include_directories(clever_paint PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(clever_paint PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)
target_link_libraries(clever_paint PUBLIC clever_layout clever_html clever_css_style clever_net clever_js)

# Link macOS frameworks for CoreText text rendering
if(APPLE)
    target_link_libraries(clever_paint PUBLIC
        "-framework CoreText"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ImageIO"
    )
endif()
