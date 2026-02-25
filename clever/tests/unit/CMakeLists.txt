# URL Parser tests
add_executable(clever_url_tests
    url_parser_test.cpp
    percent_encoding_test.cpp
)
target_link_libraries(clever_url_tests PRIVATE clever_url GTest::gtest_main)
add_test(NAME url_tests COMMAND clever_url_tests)

# Platform tests
add_executable(clever_platform_tests
    thread_pool_test.cpp
    event_loop_test.cpp
)
target_link_libraries(clever_platform_tests PRIVATE clever_platform GTest::gtest_main)
add_test(NAME platform_tests COMMAND clever_platform_tests)

# IPC tests
add_executable(clever_ipc_tests
    message_pipe_test.cpp
    message_channel_test.cpp
    serializer_test.cpp
)
target_link_libraries(clever_ipc_tests PRIVATE clever_ipc GTest::gtest_main)
add_test(NAME ipc_tests COMMAND clever_ipc_tests)

# Networking tests
add_executable(clever_net_tests
    http_client_test.cpp
)
target_link_libraries(clever_net_tests PRIVATE clever_net GTest::gtest_main)
add_test(NAME net_tests COMMAND clever_net_tests)

# Phase 2: DOM tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dom_test.cpp")
    add_executable(clever_dom_tests dom_test.cpp)
    target_link_libraries(clever_dom_tests PRIVATE clever_dom GTest::gtest_main)
    add_test(NAME dom_tests COMMAND clever_dom_tests)
endif()

# Phase 2: HTML Parser tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/html_parser_test.cpp")
    add_executable(clever_html_tests html_parser_test.cpp)
    target_link_libraries(clever_html_tests PRIVATE clever_html clever_dom GTest::gtest_main)
    add_test(NAME html_tests COMMAND clever_html_tests)
endif()

# Phase 2: CSS Parser tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/css_parser_test.cpp")
    add_executable(clever_css_parser_tests css_parser_test.cpp)
    target_link_libraries(clever_css_parser_tests PRIVATE clever_css_parser GTest::gtest_main)
    add_test(NAME css_parser_tests COMMAND clever_css_parser_tests)
endif()

# Phase 2: CSS Style Engine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/css_style_test.cpp")
    add_executable(clever_css_style_tests css_style_test.cpp)
    target_link_libraries(clever_css_style_tests PRIVATE clever_css_style clever_css_parser GTest::gtest_main)
    add_test(NAME css_style_tests COMMAND clever_css_style_tests)
endif()

# Phase 3: Layout Engine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/layout_test.cpp")
    add_executable(clever_layout_tests layout_test.cpp)
    target_link_libraries(clever_layout_tests PRIVATE clever_layout GTest::gtest_main)
    add_test(NAME layout_tests COMMAND clever_layout_tests)
endif()

# Phase 3: Paint module tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/paint_test.cpp")
    add_executable(clever_paint_tests paint_test.cpp)
    target_link_libraries(clever_paint_tests PRIVATE clever_paint clever_layout clever_html clever_css_style GTest::gtest_main)
    add_test(NAME paint_tests COMMAND clever_paint_tests)
endif()

# Native image decoding tests (macOS only, Objective-C++)
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native_image_test.mm")
    add_executable(clever_native_image_tests native_image_test.mm)
    set_source_files_properties(native_image_test.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(clever_native_image_tests PRIVATE
        GTest::gtest_main
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework ImageIO"
    )
    add_test(NAME native_image_tests COMMAND clever_native_image_tests)
endif()

# JavaScript Engine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/js_engine_test.cpp")
    add_executable(clever_js_tests js_engine_test.cpp)
    target_link_libraries(clever_js_tests PRIVATE clever_js clever_html clever_dom quickjs GTest::gtest_main)
    add_test(NAME js_tests COMMAND clever_js_tests)
endif()

# JS CORS policy unit tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cors_policy_test.cpp")
    add_executable(clever_js_cors_tests cors_policy_test.cpp)
    target_link_libraries(clever_js_cors_tests PRIVATE clever_js GTest::gtest_main)
    add_test(NAME js_cors_tests COMMAND clever_js_cors_tests)
endif()
