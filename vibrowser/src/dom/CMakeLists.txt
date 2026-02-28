add_library(vibrowser_dom STATIC
    node.cpp
    element.cpp
    document.cpp
    text.cpp
    comment.cpp
    event.cpp
)
target_include_directories(vibrowser_dom PUBLIC ${CMAKE_SOURCE_DIR}/include)
