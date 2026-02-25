cmake_minimum_required(VERSION 3.16)

project(from_scratch_browser VERSION 0.1.0 LANGUAGES CXX)

# Entry point under src/.
set(BROWSER_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Module-oriented source globs under src/.
file(GLOB_RECURSE BROWSER_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp"
)
file(GLOB_RECURSE BROWSER_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)
file(GLOB_RECURSE BROWSER_HTML_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/html/*.cpp"
)
file(GLOB_RECURSE BROWSER_CSS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/css/*.cpp"
)
file(GLOB_RECURSE BROWSER_JS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/js/*.cpp"
)
file(GLOB_RECURSE BROWSER_LAYOUT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/layout/*.cpp"
)
file(GLOB_RECURSE BROWSER_RENDER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/*.cpp"
)
file(GLOB_RECURSE BROWSER_NET_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/net/*.cpp"
)
file(GLOB_RECURSE BROWSER_BROWSER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/browser/*.cpp"
)
file(GLOB_RECURSE BROWSER_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp"
)
file(GLOB_RECURSE BROWSER_UTILS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

set(FROM_SCRATCH_BROWSER_SOURCES
    ${BROWSER_BROWSER_SOURCES}
    ${BROWSER_APP_SOURCES}
    ${BROWSER_CORE_SOURCES}
    ${BROWSER_ENGINE_SOURCES}
    ${BROWSER_HTML_SOURCES}
    ${BROWSER_CSS_SOURCES}
    ${BROWSER_JS_SOURCES}
    ${BROWSER_LAYOUT_SOURCES}
    ${BROWSER_RENDER_SOURCES}
    ${BROWSER_NET_SOURCES}
    ${BROWSER_UTILS_SOURCES}
)

if(EXISTS "${BROWSER_MAIN_SOURCE}")
    list(APPEND FROM_SCRATCH_BROWSER_SOURCES "${BROWSER_MAIN_SOURCE}")
endif()

# Keep the bootstrap buildable even before src/ is populated.
if(NOT EXISTS "${BROWSER_MAIN_SOURCE}")
    set(BOOTSTRAP_MAIN "${CMAKE_CURRENT_BINARY_DIR}/bootstrap_main.cpp")
    file(WRITE "${BOOTSTRAP_MAIN}"
        "#include <iostream>\n"
        "int main() {\n"
        "    std::cout << \"from_scratch_browser bootstrap\" << std::endl;\n"
        "    return 0;\n"
        "}\n"
    )
    list(APPEND FROM_SCRATCH_BROWSER_SOURCES "${BOOTSTRAP_MAIN}")
endif()

add_executable(from_scratch_browser ${FROM_SCRATCH_BROWSER_SOURCES})

target_compile_features(from_scratch_browser PRIVATE cxx_std_17)
set_target_properties(from_scratch_browser PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(from_scratch_browser
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(from_scratch_browser PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(from_scratch_browser PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(from_scratch_browser PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(from_scratch_browser PRIVATE BROWSER_USE_OPENSSL=1)
endif()

# Library sources (everything except main.cpp) for tests
set(BROWSER_LIB_SOURCES
    ${BROWSER_BROWSER_SOURCES}
    ${BROWSER_APP_SOURCES}
    ${BROWSER_CORE_SOURCES}
    ${BROWSER_ENGINE_SOURCES}
    ${BROWSER_HTML_SOURCES}
    ${BROWSER_CSS_SOURCES}
    ${BROWSER_JS_SOURCES}
    ${BROWSER_LAYOUT_SOURCES}
    ${BROWSER_RENDER_SOURCES}
    ${BROWSER_NET_SOURCES}
    ${BROWSER_UTILS_SOURCES}
)

# Tests
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${BROWSER_LIB_SOURCES})
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    if(NOT MSVC)
        target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(OpenSSL_FOUND)
        target_link_libraries(${TEST_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
        target_compile_definitions(${TEST_NAME} PRIVATE BROWSER_USE_OPENSSL=1)
    endif()
endforeach()
