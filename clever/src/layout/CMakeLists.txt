add_library(clever_layout STATIC
    box.cpp
    layout_engine.cpp
)
target_include_directories(clever_layout PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(clever_layout PUBLIC clever_css_style)
