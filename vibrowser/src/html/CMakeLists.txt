add_library(vibrowser_html STATIC
    tokenizer.cpp
    tree_builder.cpp
    simple_node.cpp
)
target_include_directories(vibrowser_html PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(vibrowser_html PUBLIC vibrowser_dom)
