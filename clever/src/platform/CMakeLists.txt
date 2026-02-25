add_library(clever_platform STATIC
    thread_pool.cpp
    event_loop.cpp
    timer.cpp
)

target_include_directories(clever_platform PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(clever_platform PUBLIC Threads::Threads)
