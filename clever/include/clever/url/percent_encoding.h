#pragma once
#include <string>
#include <string_view>

namespace clever::url {

std::string percent_encode(std::string_view input, bool encode_path_chars = false);
std::string percent_decode(std::string_view input);
bool is_url_code_point(char32_t c);

} // namespace clever::url
