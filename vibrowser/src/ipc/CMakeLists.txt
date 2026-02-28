add_library(vibrowser_ipc STATIC
    message_pipe.cpp
    message_channel.cpp
    serializer.cpp
)

target_include_directories(vibrowser_ipc PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(vibrowser_ipc PUBLIC vibrowser_platform)
