#pragma once

#include <string>

namespace browser::net {

struct Url {
  std::string scheme;
  std::string host;
  int port = 0;
  std::string path_and_query;

  std::string to_string() const;
};

bool parse_url(const std::string& input, Url& out, std::string& err);
bool is_absolute_url(const std::string& value);
std::string resolve_url(const std::string& base_url, const std::string& ref, std::string& err);
bool is_file_url(const std::string& value);
bool file_url_to_path(const std::string& file_url, std::string& path, std::string& err);
std::string path_to_file_url(const std::string& path);

}  // namespace browser::net
