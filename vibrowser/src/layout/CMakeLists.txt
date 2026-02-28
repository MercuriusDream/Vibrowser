add_library(vibrowser_layout STATIC
    box.cpp
    layout_engine.cpp
)
target_include_directories(vibrowser_layout PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(vibrowser_layout PUBLIC vibrowser_css_style)
