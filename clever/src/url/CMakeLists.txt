add_library(clever_url STATIC
    url_parser.cpp
    percent_encoding.cpp
    idna.cpp
    url.cpp
)

target_include_directories(clever_url PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(ICU_FOUND)
    target_link_libraries(clever_url PUBLIC ICU::uc ICU::i18n)
elseif(TARGET PkgConfig::ICU)
    target_link_libraries(clever_url PUBLIC PkgConfig::ICU)
endif()
