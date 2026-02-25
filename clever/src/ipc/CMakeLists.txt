add_library(clever_ipc STATIC
    message_pipe.cpp
    message_channel.cpp
    serializer.cpp
)

target_include_directories(clever_ipc PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(clever_ipc PUBLIC clever_platform)
