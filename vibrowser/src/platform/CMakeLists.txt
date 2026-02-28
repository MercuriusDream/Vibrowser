add_library(vibrowser_platform STATIC
    thread_pool.cpp
    event_loop.cpp
    timer.cpp
)

target_include_directories(vibrowser_platform PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(vibrowser_platform PUBLIC Threads::Threads)
