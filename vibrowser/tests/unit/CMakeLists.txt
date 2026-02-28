# URL Parser tests
add_executable(vibrowser_url_tests
    url_parser_test.cpp
    percent_encoding_test.cpp
)
target_link_libraries(vibrowser_url_tests PRIVATE vibrowser_url GTest::gtest_main)
add_test(NAME url_tests COMMAND vibrowser_url_tests)

# Platform tests
add_executable(vibrowser_platform_tests
    thread_pool_test.cpp
    event_loop_test.cpp
)
target_link_libraries(vibrowser_platform_tests PRIVATE vibrowser_platform GTest::gtest_main)
add_test(NAME platform_tests COMMAND vibrowser_platform_tests)

# IPC tests
add_executable(vibrowser_ipc_tests
    message_pipe_test.cpp
    message_channel_test.cpp
    serializer_test.cpp
)
target_link_libraries(vibrowser_ipc_tests PRIVATE vibrowser_ipc GTest::gtest_main)
add_test(NAME ipc_tests COMMAND vibrowser_ipc_tests)

# Networking tests
add_executable(vibrowser_net_tests
    http_client_test.cpp
)
target_link_libraries(vibrowser_net_tests PRIVATE vibrowser_net GTest::gtest_main)
add_test(NAME net_tests COMMAND vibrowser_net_tests)

# Phase 2: DOM tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dom_test.cpp")
    add_executable(vibrowser_dom_tests dom_test.cpp)
    target_link_libraries(vibrowser_dom_tests PRIVATE vibrowser_dom GTest::gtest_main)
    add_test(NAME dom_tests COMMAND vibrowser_dom_tests)
endif()

# Phase 2: HTML Parser tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/html_parser_test.cpp")
    add_executable(vibrowser_html_tests html_parser_test.cpp)
    target_link_libraries(vibrowser_html_tests PRIVATE vibrowser_html vibrowser_dom GTest::gtest_main)
    add_test(NAME html_tests COMMAND vibrowser_html_tests)
endif()

# Phase 2: CSS Parser tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/css_parser_test.cpp")
    add_executable(vibrowser_css_parser_tests css_parser_test.cpp)
    target_link_libraries(vibrowser_css_parser_tests PRIVATE vibrowser_css_parser GTest::gtest_main)
    add_test(NAME css_parser_tests COMMAND vibrowser_css_parser_tests)
endif()

# Phase 2: CSS Style Engine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/css_style_test.cpp")
    add_executable(vibrowser_css_style_tests css_style_test.cpp)
    target_link_libraries(vibrowser_css_style_tests PRIVATE vibrowser_css_style vibrowser_css_parser GTest::gtest_main)
    add_test(NAME css_style_tests COMMAND vibrowser_css_style_tests)
endif()

# Phase 3: Layout Engine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/layout_test.cpp")
    add_executable(vibrowser_layout_tests layout_test.cpp)
    target_link_libraries(vibrowser_layout_tests PRIVATE vibrowser_layout GTest::gtest_main)
    add_test(NAME layout_tests COMMAND vibrowser_layout_tests)
endif()

# Phase 3: Paint module tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/paint_test.cpp")
    add_executable(vibrowser_paint_tests paint_test.cpp)
    target_link_libraries(vibrowser_paint_tests PRIVATE vibrowser_paint GTest::gtest_main)
    add_test(NAME paint_tests COMMAND vibrowser_paint_tests)
endif()

# Native image decoding tests (macOS only, Objective-C++)
if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/native_image_test.mm")
    add_executable(vibrowser_native_image_tests native_image_test.mm)
    set_source_files_properties(native_image_test.mm PROPERTIES COMPILE_FLAGS "-x objective-c++")
    target_link_libraries(vibrowser_native_image_tests PRIVATE
        GTest::gtest_main
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework ImageIO"
    )
    add_test(NAME native_image_tests COMMAND vibrowser_native_image_tests)
endif()

# JavaScript Engine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/js_engine_test.cpp")
    add_executable(vibrowser_js_tests js_engine_test.cpp)
    target_link_libraries(vibrowser_js_tests PRIVATE vibrowser_js vibrowser_html vibrowser_dom quickjs GTest::gtest_main)
    add_test(NAME js_tests COMMAND vibrowser_js_tests)
endif()

# JS CORS policy unit tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cors_policy_test.cpp")
    add_executable(vibrowser_js_cors_tests cors_policy_test.cpp)
    target_link_libraries(vibrowser_js_cors_tests PRIVATE vibrowser_js GTest::gtest_main)
    add_test(NAME js_cors_tests COMMAND vibrowser_js_cors_tests)
endif()
