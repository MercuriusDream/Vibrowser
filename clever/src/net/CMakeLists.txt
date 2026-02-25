add_library(clever_net STATIC
    http_client.cpp
    connection_pool.cpp
    cookie_jar.cpp
    header_map.cpp
    request.cpp
    response.cpp
    tls_socket.cpp
)

target_include_directories(clever_net PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

find_package(ZLIB REQUIRED)
target_link_libraries(clever_net PUBLIC clever_url clever_platform ZLIB::ZLIB)

# SecureTransport / CoreFoundation for TLS support (macOS)
if(APPLE)
    target_link_libraries(clever_net PUBLIC "-framework Security" "-framework CoreFoundation")
endif()
