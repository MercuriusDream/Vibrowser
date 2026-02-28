# QuickJS JavaScript Engine (vendored third-party)
# https://bellard.org/quickjs/

cmake_minimum_required(VERSION 3.20)

# Enable C language for this subdirectory
enable_language(C)

add_library(quickjs STATIC
    quickjs.c
    cutils.c
    dtoa.c
    libregexp.c
    libunicode.c
)

# QuickJS is C11
set_target_properties(quickjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Suppress all warnings for third-party code
target_compile_options(quickjs PRIVATE -w)

# Public include directory so consumers can #include "quickjs.h"
# Use SYSTEM to prevent quickjs's "version" file from shadowing the C++20
# <version> header.
target_include_directories(quickjs SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Required compile definitions
target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="2025-09-13"
)

# BigNum support on Apple platforms
if(APPLE)
    target_compile_definitions(quickjs PRIVATE CONFIG_BIGNUM)
endif()
