add_library(vibrowser_net STATIC
    http_client.cpp
    connection_pool.cpp
    cookie_jar.cpp
    header_map.cpp
    request.cpp
    response.cpp
    tls_socket.cpp
    hpack.cpp
    http2_connection.cpp
)

target_include_directories(vibrowser_net PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

find_package(ZLIB REQUIRED)
target_link_libraries(vibrowser_net PUBLIC vibrowser_url vibrowser_platform ZLIB::ZLIB)

# SecureTransport / CoreFoundation for TLS support (macOS)
if(APPLE)
    target_link_libraries(vibrowser_net PUBLIC "-framework Security" "-framework CoreFoundation")
endif()
