#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace browser::html {

enum class NodeType {
    Document,
    Element,
    Text,
};

struct Node {
    NodeType type = NodeType::Document;
    std::string tag_name;
    std::map<std::string, std::string> attributes;
    std::string text_content;
    std::vector<std::unique_ptr<Node>> children;
    Node* parent = nullptr;

    Node() = default;
    explicit Node(NodeType node_type) : type(node_type) {}
    Node(NodeType node_type, std::string tag) : type(node_type), tag_name(std::move(tag)) {}
};

// Serialize DOM tree to a canonical string for deterministic comparison
std::string serialize_dom(const Node& node);

}  // namespace browser::html
