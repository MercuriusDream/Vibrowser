cmake_minimum_required(VERSION 3.20)

project(vibrowser_engine VERSION 0.5.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Werror)
endif()

# Sanitizers for debug builds
option(VIBROWSER_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(VIBROWSER_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(VIBROWSER_ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(VIBROWSER_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(VIBROWSER_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

if(VIBROWSER_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# LTO for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported)
    if(lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Dependencies via vcpkg or find_package
find_package(ICU COMPONENTS uc i18n data QUIET)
if(NOT ICU_FOUND)
    # Fallback: try pkg-config
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ICU IMPORTED_TARGET icu-uc icu-i18n)
    endif()
endif()

# GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Third-party libraries
add_subdirectory(third_party/quickjs)

# Phase 1 modules (static libraries)
add_subdirectory(src/url)
add_subdirectory(src/platform)
add_subdirectory(src/ipc)
add_subdirectory(src/net)

# Phase 2 modules
add_subdirectory(src/dom)
add_subdirectory(src/html)
add_subdirectory(src/css/parser)
add_subdirectory(src/css/style)

# JavaScript engine
add_subdirectory(src/js)
# add_subdirectory(src/bindings)  # TODO: Web API bindings layer

# Phase 3 modules
add_subdirectory(src/layout)
add_subdirectory(src/paint)

# Phase 4: Browser shell (macOS)
if(APPLE)
    add_subdirectory(src/shell)
endif()

# Test targets
add_subdirectory(tests)
