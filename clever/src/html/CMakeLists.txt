add_library(clever_html STATIC
    tokenizer.cpp
    tree_builder.cpp
    simple_node.cpp
)
target_include_directories(clever_html PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clever_html PUBLIC clever_dom)
