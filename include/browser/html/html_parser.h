#pragma once

#include <memory>
#include <string>
#include <vector>

#include "browser/html/dom.h"

namespace browser::html {

struct ParseWarning {
    std::string message;
    std::string recovery_action;
};

struct ParseResult {
    std::unique_ptr<Node> document;
    std::vector<ParseWarning> warnings;
};

std::unique_ptr<Node> parse_html(const std::string& html);
ParseResult parse_html_with_diagnostics(const std::string& html);

std::vector<Node*> query_all_by_tag(Node& root, const std::string& tag);
std::vector<const Node*> query_all_by_tag(const Node& root, const std::string& tag);
Node* query_first_by_tag(Node& root, const std::string& tag);
const Node* query_first_by_tag(const Node& root, const std::string& tag);
Node* query_first_by_id(Node& root, const std::string& id);
const Node* query_first_by_id(const Node& root, const std::string& id);
std::vector<Node*> query_all_by_attr(Node& root, const std::string& attr, const std::string& value);
std::vector<const Node*> query_all_by_attr(const Node& root, const std::string& attr, const std::string& value);
std::vector<Node*> query_all_by_attr_token(Node& root, const std::string& attr, const std::string& token);
std::vector<const Node*> query_all_by_attr_token(const Node& root, const std::string& attr, const std::string& token);
Node* query_first_by_attr_token(Node& root, const std::string& attr, const std::string& token);
const Node* query_first_by_attr_token(const Node& root, const std::string& attr, const std::string& token);
const Node* query_first_by_attr(const Node& root, const std::string& attr, const std::string& value);
Node* query_first_by_attr(Node& root, const std::string& attr, const std::string& value);
std::vector<Node*> query_all_by_class(Node& root, const std::string& class_name);
std::vector<const Node*> query_all_by_class(const Node& root, const std::string& class_name);
Node* query_first_by_class(Node& root, const std::string& class_name);
const Node* query_first_by_class(const Node& root, const std::string& class_name);
std::vector<Node*> query_all_text_contains(Node& root, const std::string& needle);
std::vector<const Node*> query_all_text_contains(const Node& root, const std::string& needle);

std::string inner_text(const Node& root);

}  // namespace browser::html
