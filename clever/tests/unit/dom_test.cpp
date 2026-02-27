#include <clever/dom/node.h>
#include <clever/dom/element.h>
#include <clever/dom/text.h>
#include <clever/dom/comment.h>
#include <clever/dom/document.h>
#include <clever/dom/event.h>

#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace clever::dom;

// ---------------------------------------------------------------------------
// 1. Create Element with tag name
// ---------------------------------------------------------------------------
TEST(DomElement, CreateWithTagName) {
    Element elem("div");
    EXPECT_EQ(elem.tag_name(), "div");
    EXPECT_EQ(elem.node_type(), NodeType::Element);
    EXPECT_EQ(elem.namespace_uri(), "");
}

TEST(DomElement, CreateWithNamespace) {
    Element elem("svg", "http://www.w3.org/2000/svg");
    EXPECT_EQ(elem.tag_name(), "svg");
    EXPECT_EQ(elem.namespace_uri(), "http://www.w3.org/2000/svg");
}

// ---------------------------------------------------------------------------
// 2. Set/get/remove attributes
// ---------------------------------------------------------------------------
TEST(DomElement, SetAndGetAttribute) {
    Element elem("div");
    elem.set_attribute("class", "container");
    auto val = elem.get_attribute("class");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "container");
}

TEST(DomElement, HasAttribute) {
    Element elem("div");
    EXPECT_FALSE(elem.has_attribute("id"));
    elem.set_attribute("id", "main");
    EXPECT_TRUE(elem.has_attribute("id"));
}

TEST(DomElement, RemoveAttribute) {
    Element elem("div");
    elem.set_attribute("title", "hello");
    EXPECT_TRUE(elem.has_attribute("title"));
    elem.remove_attribute("title");
    EXPECT_FALSE(elem.has_attribute("title"));
    EXPECT_FALSE(elem.get_attribute("title").has_value());
}

TEST(DomElement, OverwriteAttribute) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("type", "password");
    EXPECT_EQ(elem.get_attribute("type").value(), "password");
    // Should not duplicate the attribute
    EXPECT_EQ(elem.attributes().size(), 1u);
}

TEST(DomElement, GetMissingAttributeReturnsNullopt) {
    Element elem("span");
    EXPECT_FALSE(elem.get_attribute("nonexistent").has_value());
}

// ---------------------------------------------------------------------------
// 3. Append child to node
// ---------------------------------------------------------------------------
TEST(DomNode, AppendChild) {
    auto parent = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();

    parent->append_child(std::move(child));

    EXPECT_EQ(parent->child_count(), 1u);
    EXPECT_EQ(parent->first_child(), child_ptr);
    EXPECT_EQ(parent->last_child(), child_ptr);
}

TEST(DomNode, AppendMultipleChildren) {
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");
    Element* p1 = li1.get();
    [[maybe_unused]] Element* p2 = li2.get();
    Element* p3 = li3.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), p1);
    EXPECT_EQ(parent->last_child(), p3);
}

// ---------------------------------------------------------------------------
// 4. Insert before reference node
// ---------------------------------------------------------------------------
TEST(DomNode, InsertBefore) {
    auto parent = std::make_unique<Element>("div");
    auto first = std::make_unique<Element>("a");
    auto third = std::make_unique<Element>("c");
    Element* first_ptr = first.get();
    Element* third_ptr = third.get();

    parent->append_child(std::move(first));
    parent->append_child(std::move(third));

    auto second = std::make_unique<Element>("b");
    Element* second_ptr = second.get();
    parent->insert_before(std::move(second), third_ptr);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->next_sibling(), third_ptr);
    EXPECT_EQ(parent->last_child(), third_ptr);
}

TEST(DomNode, InsertBeforeNullAppendsChild) {
    auto parent = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();

    parent->insert_before(std::move(child), nullptr);
    EXPECT_EQ(parent->child_count(), 1u);
    EXPECT_EQ(parent->first_child(), child_ptr);
}

// ---------------------------------------------------------------------------
// 5. Remove child
// ---------------------------------------------------------------------------
TEST(DomNode, RemoveChild) {
    auto parent = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();

    parent->append_child(std::move(child));
    EXPECT_EQ(parent->child_count(), 1u);

    auto removed = parent->remove_child(*child_ptr);
    EXPECT_EQ(removed.get(), child_ptr);
    EXPECT_EQ(parent->child_count(), 0u);
    EXPECT_EQ(child_ptr->parent(), nullptr);
}

TEST(DomNode, RemoveMiddleChild) {
    auto parent = std::make_unique<Element>("div");
    auto a = std::make_unique<Element>("a");
    auto b = std::make_unique<Element>("b");
    auto c = std::make_unique<Element>("c");
    Element* a_ptr = a.get();
    Element* b_ptr = b.get();
    Element* c_ptr = c.get();

    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    parent->append_child(std::move(c));

    auto removed = parent->remove_child(*b_ptr);
    EXPECT_EQ(removed.get(), b_ptr);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(a_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), a_ptr);
}

// ---------------------------------------------------------------------------
// 6. Parent pointer is set correctly
// ---------------------------------------------------------------------------
TEST(DomNode, ParentPointerSetOnAppend) {
    auto parent = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();

    EXPECT_EQ(child_ptr->parent(), nullptr);
    parent->append_child(std::move(child));
    EXPECT_EQ(child_ptr->parent(), parent.get());
}

TEST(DomNode, ParentPointerClearedOnRemove) {
    auto parent = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();

    parent->append_child(std::move(child));
    EXPECT_EQ(child_ptr->parent(), parent.get());

    auto removed = parent->remove_child(*child_ptr);
    EXPECT_EQ(child_ptr->parent(), nullptr);
}

// ---------------------------------------------------------------------------
// 7. Sibling pointers are correct
// ---------------------------------------------------------------------------
TEST(DomNode, SiblingPointers) {
    auto parent = std::make_unique<Element>("div");
    auto a = std::make_unique<Element>("a");
    auto b = std::make_unique<Element>("b");
    auto c = std::make_unique<Element>("c");
    Element* a_ptr = a.get();
    Element* b_ptr = b.get();
    Element* c_ptr = c.get();

    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    parent->append_child(std::move(c));

    EXPECT_EQ(a_ptr->previous_sibling(), nullptr);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->previous_sibling(), a_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), b_ptr);
    EXPECT_EQ(c_ptr->next_sibling(), nullptr);
}

// ---------------------------------------------------------------------------
// 8. Child count
// ---------------------------------------------------------------------------
TEST(DomNode, ChildCount) {
    auto parent = std::make_unique<Element>("div");
    EXPECT_EQ(parent->child_count(), 0u);

    parent->append_child(std::make_unique<Element>("a"));
    EXPECT_EQ(parent->child_count(), 1u);

    parent->append_child(std::make_unique<Element>("b"));
    EXPECT_EQ(parent->child_count(), 2u);

    parent->append_child(std::make_unique<Element>("c"));
    EXPECT_EQ(parent->child_count(), 3u);
}

// ---------------------------------------------------------------------------
// 9. Document create_element factory
// ---------------------------------------------------------------------------
TEST(DomDocument, CreateElement) {
    Document doc;
    auto elem = doc.create_element("div");
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "div");
    EXPECT_EQ(elem->node_type(), NodeType::Element);
}

// ---------------------------------------------------------------------------
// 10. Document create_text_node
// ---------------------------------------------------------------------------
TEST(DomDocument, CreateTextNode) {
    Document doc;
    auto text = doc.create_text_node("Hello, World!");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->data(), "Hello, World!");
    EXPECT_EQ(text->node_type(), NodeType::Text);
}

// ---------------------------------------------------------------------------
// 11. Document get_element_by_id
// ---------------------------------------------------------------------------
TEST(DomDocument, GetElementById) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->set_attribute("id", "main");
    Element* elem_ptr = elem.get();

    doc.register_id("main", elem_ptr);
    doc.append_child(std::move(elem));

    Element* found = doc.get_element_by_id("main");
    EXPECT_EQ(found, elem_ptr);
}

TEST(DomDocument, GetElementByIdNotFound) {
    Document doc;
    EXPECT_EQ(doc.get_element_by_id("nonexistent"), nullptr);
}

// ---------------------------------------------------------------------------
// 12. ID map updates on setAttribute("id", ...)
// ---------------------------------------------------------------------------
TEST(DomElement, IdUpdatedOnSetAttribute) {
    Element elem("div");
    EXPECT_EQ(elem.id(), "");
    elem.set_attribute("id", "my-id");
    EXPECT_EQ(elem.id(), "my-id");
}

// ---------------------------------------------------------------------------
// 13. Text node data get/set
// ---------------------------------------------------------------------------
TEST(DomText, CreateAndGetData) {
    Text text("Hello");
    EXPECT_EQ(text.data(), "Hello");
    EXPECT_EQ(text.node_type(), NodeType::Text);
}

TEST(DomText, SetData) {
    Text text("Hello");
    text.set_data("World");
    EXPECT_EQ(text.data(), "World");
}

TEST(DomText, TextContent) {
    Text text("some text");
    EXPECT_EQ(text.text_content(), "some text");
}

// ---------------------------------------------------------------------------
// 14. Comment node
// ---------------------------------------------------------------------------
TEST(DomComment, CreateAndGetData) {
    Comment comment("this is a comment");
    EXPECT_EQ(comment.data(), "this is a comment");
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
}

TEST(DomComment, SetData) {
    Comment comment("old");
    comment.set_data("new");
    EXPECT_EQ(comment.data(), "new");
}

// ---------------------------------------------------------------------------
// 15. text_content() recursive
// ---------------------------------------------------------------------------
TEST(DomNode, TextContentRecursive) {
    auto div = std::make_unique<Element>("div");
    div->append_child(std::make_unique<Text>("Hello "));

    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("World"));
    div->append_child(std::move(span));

    div->append_child(std::make_unique<Text>("!"));

    EXPECT_EQ(div->text_content(), "Hello World!");
}

TEST(DomNode, TextContentIgnoresComments) {
    auto div = std::make_unique<Element>("div");
    div->append_child(std::make_unique<Text>("visible"));
    div->append_child(std::make_unique<Comment>("hidden"));
    div->append_child(std::make_unique<Text>(" text"));

    EXPECT_EQ(div->text_content(), "visible text");
}

// ---------------------------------------------------------------------------
// 16. Dirty flag propagation: mark child dirty -> propagates to ancestors
// ---------------------------------------------------------------------------
TEST(DomNode, DirtyFlagPropagation) {
    auto grandparent = std::make_unique<Element>("div");
    auto parent_elem = std::make_unique<Element>("section");
    auto child = std::make_unique<Element>("p");
    Node* child_ptr = child.get();
    Node* parent_ptr = parent_elem.get();

    parent_elem->append_child(std::move(child));
    grandparent->append_child(std::move(parent_elem));

    // All should start clean
    EXPECT_EQ(grandparent->dirty_flags(), DirtyFlags::None);
    EXPECT_EQ(parent_ptr->dirty_flags(), DirtyFlags::None);
    EXPECT_EQ(child_ptr->dirty_flags(), DirtyFlags::None);

    // Mark child dirty
    child_ptr->mark_dirty(DirtyFlags::Style);

    // Child should be dirty
    EXPECT_NE(child_ptr->dirty_flags() & DirtyFlags::Style, DirtyFlags::None);

    // Parent and grandparent should also be dirty
    EXPECT_NE(parent_ptr->dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
    EXPECT_NE(grandparent->dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
}

TEST(DomNode, ClearDirty) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Layout);
    EXPECT_NE(elem.dirty_flags(), DirtyFlags::None);
    elem.clear_dirty();
    EXPECT_EQ(elem.dirty_flags(), DirtyFlags::None);
}

TEST(DomNode, DirtyFlagCombination) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Style);
    elem.mark_dirty(DirtyFlags::Layout);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

// ---------------------------------------------------------------------------
// 17. ClassList add/remove/contains/toggle
// ---------------------------------------------------------------------------
TEST(DomClassList, AddAndContains) {
    ClassList cl;
    cl.add("foo");
    EXPECT_TRUE(cl.contains("foo"));
    EXPECT_FALSE(cl.contains("bar"));
    EXPECT_EQ(cl.length(), 1u);
}

TEST(DomClassList, AddDuplicate) {
    ClassList cl;
    cl.add("foo");
    cl.add("foo");
    EXPECT_EQ(cl.length(), 1u);
}

TEST(DomClassList, Remove) {
    ClassList cl;
    cl.add("foo");
    cl.add("bar");
    cl.remove("foo");
    EXPECT_FALSE(cl.contains("foo"));
    EXPECT_TRUE(cl.contains("bar"));
    EXPECT_EQ(cl.length(), 1u);
}

TEST(DomClassList, Toggle) {
    ClassList cl;
    cl.toggle("foo");
    EXPECT_TRUE(cl.contains("foo"));
    cl.toggle("foo");
    EXPECT_FALSE(cl.contains("foo"));
    EXPECT_EQ(cl.length(), 0u);
}

TEST(DomClassList, ToString) {
    ClassList cl;
    cl.add("a");
    cl.add("b");
    cl.add("c");
    EXPECT_EQ(cl.to_string(), "a b c");
}

// ---------------------------------------------------------------------------
// 18. Event creation
// ---------------------------------------------------------------------------
TEST(DomEvent, Creation) {
    Event event("click");
    EXPECT_EQ(event.type(), "click");
    EXPECT_TRUE(event.bubbles());
    EXPECT_TRUE(event.cancelable());
    EXPECT_EQ(event.phase(), EventPhase::None);
    EXPECT_EQ(event.target(), nullptr);
    EXPECT_EQ(event.current_target(), nullptr);
    EXPECT_FALSE(event.propagation_stopped());
    EXPECT_FALSE(event.default_prevented());
}

TEST(DomEvent, NonBubbling) {
    Event event("focus", false, false);
    EXPECT_FALSE(event.bubbles());
    EXPECT_FALSE(event.cancelable());
}

// ---------------------------------------------------------------------------
// 19. Event dispatch: capture -> target -> bubble
// ---------------------------------------------------------------------------
TEST(DomEvent, DispatchCaptureTargetBubble) {
    // Build tree: grandparent -> parent -> child
    auto grandparent = std::make_unique<Element>("div");
    auto parent_elem = std::make_unique<Element>("section");
    auto child = std::make_unique<Element>("button");
    Node* grandparent_ptr = grandparent.get();
    Node* parent_ptr = parent_elem.get();
    Node* child_ptr = child.get();

    parent_elem->append_child(std::move(child));
    grandparent->append_child(std::move(parent_elem));

    std::vector<std::string> log;

    // Add capturing listener on grandparent
    EventTarget gp_target;
    gp_target.add_event_listener("click", [&](Event&) {
        log.push_back("grandparent-capture");
    }, true);
    // Bubbling listener on grandparent
    gp_target.add_event_listener("click", [&](Event&) {
        log.push_back("grandparent-bubble");
    }, false);

    EventTarget p_target;
    p_target.add_event_listener("click", [&](Event&) {
        log.push_back("parent-capture");
    }, true);
    p_target.add_event_listener("click", [&](Event&) {
        log.push_back("parent-bubble");
    }, false);

    EventTarget c_target;
    c_target.add_event_listener("click", [&](Event&) {
        log.push_back("child-target");
    }, false);

    // Build ancestor path
    // We need to dispatch manually to test the ordering
    Event event("click");

    // Build path from target to root
    std::vector<std::pair<Node*, EventTarget*>> path;
    path.push_back({grandparent_ptr, &gp_target});
    path.push_back({parent_ptr, &p_target});
    path.push_back({child_ptr, &c_target});

    event.target_ = child_ptr;

    // Capture phase: root -> target
    event.phase_ = EventPhase::Capturing;
    for (size_t i = 0; i < path.size() - 1; ++i) {
        event.current_target_ = path[i].first;
        path[i].second->dispatch_event(event, *path[i].first);
        if (event.propagation_stopped()) break;
    }

    // Target phase
    if (!event.propagation_stopped()) {
        event.phase_ = EventPhase::AtTarget;
        event.current_target_ = child_ptr;
        c_target.dispatch_event(event, *child_ptr);
    }

    // Bubble phase: target -> root
    if (!event.propagation_stopped() && event.bubbles()) {
        event.phase_ = EventPhase::Bubbling;
        for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
            event.current_target_ = path[i].first;
            path[i].second->dispatch_event(event, *path[i].first);
            if (event.propagation_stopped()) break;
        }
    }

    ASSERT_EQ(log.size(), 5u);
    EXPECT_EQ(log[0], "grandparent-capture");
    EXPECT_EQ(log[1], "parent-capture");
    EXPECT_EQ(log[2], "child-target");
    EXPECT_EQ(log[3], "parent-bubble");
    EXPECT_EQ(log[4], "grandparent-bubble");
}

// ---------------------------------------------------------------------------
// 20. Event stop_propagation
// ---------------------------------------------------------------------------
TEST(DomEvent, StopPropagation) {
    Event event("click");
    EXPECT_FALSE(event.propagation_stopped());
    event.stop_propagation();
    EXPECT_TRUE(event.propagation_stopped());
}

TEST(DomEvent, StopImmediatePropagation) {
    Event event("click");
    event.stop_immediate_propagation();
    EXPECT_TRUE(event.propagation_stopped());
    EXPECT_TRUE(event.immediate_propagation_stopped());
}

TEST(DomEvent, StopPropagationInListener) {
    std::vector<std::string> log;

    EventTarget target;
    target.add_event_listener("click", [&](Event& e) {
        log.push_back("first");
        e.stop_propagation();
    }, false);
    target.add_event_listener("click", [&]([[maybe_unused]] Event& e) {
        log.push_back("second");
    }, false);

    Event event("click");
    auto node = std::make_unique<Element>("div");
    event.target_ = node.get();
    event.current_target_ = node.get();
    event.phase_ = EventPhase::AtTarget;
    target.dispatch_event(event, *node);

    // stop_propagation should NOT prevent other listeners on same target
    EXPECT_EQ(log.size(), 2u);
    EXPECT_TRUE(event.propagation_stopped());
}

TEST(DomEvent, StopImmediatePropagationInListener) {
    std::vector<std::string> log;

    EventTarget target;
    target.add_event_listener("click", [&](Event& e) {
        log.push_back("first");
        e.stop_immediate_propagation();
    }, false);
    target.add_event_listener("click", [&]([[maybe_unused]] Event& e) {
        log.push_back("second");
    }, false);

    Event event("click");
    auto node = std::make_unique<Element>("div");
    event.target_ = node.get();
    event.current_target_ = node.get();
    event.phase_ = EventPhase::AtTarget;
    target.dispatch_event(event, *node);

    // stop_immediate_propagation SHOULD prevent remaining listeners on same target
    EXPECT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0], "first");
}

// ---------------------------------------------------------------------------
// 21. Event prevent_default
// ---------------------------------------------------------------------------
TEST(DomEvent, PreventDefault) {
    Event event("click", true, true);
    EXPECT_FALSE(event.default_prevented());
    event.prevent_default();
    EXPECT_TRUE(event.default_prevented());
}

TEST(DomEvent, PreventDefaultOnNonCancelable) {
    Event event("click", true, false);
    event.prevent_default();
    EXPECT_FALSE(event.default_prevented());
}

// ---------------------------------------------------------------------------
// 22. Multiple listeners on same type
// ---------------------------------------------------------------------------
TEST(DomEvent, MultipleListenersSameType) {
    std::vector<int> order;

    EventTarget target;
    target.add_event_listener("click", [&](Event&) {
        order.push_back(1);
    }, false);
    target.add_event_listener("click", [&](Event&) {
        order.push_back(2);
    }, false);
    target.add_event_listener("click", [&](Event&) {
        order.push_back(3);
    }, false);

    Event event("click");
    auto node = std::make_unique<Element>("div");
    event.target_ = node.get();
    event.current_target_ = node.get();
    event.phase_ = EventPhase::AtTarget;
    target.dispatch_event(event, *node);

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(DomEvent, RemoveAllListeners) {
    std::vector<int> order;

    EventTarget target;
    target.add_event_listener("click", [&](Event&) {
        order.push_back(1);
    }, false);
    target.add_event_listener("click", [&](Event&) {
        order.push_back(2);
    }, false);

    target.remove_all_listeners("click");

    Event event("click");
    auto node = std::make_unique<Element>("div");
    event.target_ = node.get();
    event.current_target_ = node.get();
    event.phase_ = EventPhase::AtTarget;
    target.dispatch_event(event, *node);

    EXPECT_TRUE(order.empty());
}

// ---------------------------------------------------------------------------
// Additional edge-case tests
// ---------------------------------------------------------------------------
TEST(DomDocument, DocumentElementAccessors) {
    Document doc;
    auto html = doc.create_element("html");
    Element* html_ptr = html.get();

    auto head = doc.create_element("head");
    Element* head_ptr = head.get();
    auto body = doc.create_element("body");
    Element* body_ptr = body.get();

    html->append_child(std::move(head));
    html->append_child(std::move(body));
    doc.append_child(std::move(html));

    EXPECT_EQ(doc.document_element(), html_ptr);
    EXPECT_EQ(doc.head(), head_ptr);
    EXPECT_EQ(doc.body(), body_ptr);
}

TEST(DomDocument, CreateComment) {
    Document doc;
    auto comment = doc.create_comment("test comment");
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->data(), "test comment");
    EXPECT_EQ(comment->node_type(), NodeType::Comment);
}

TEST(DomNode, ForEachChild) {
    auto parent = std::make_unique<Element>("div");
    parent->append_child(std::make_unique<Element>("a"));
    parent->append_child(std::make_unique<Element>("b"));
    parent->append_child(std::make_unique<Element>("c"));

    std::vector<NodeType> types;
    parent->for_each_child([&](const Node& child) {
        types.push_back(child.node_type());
    });
    EXPECT_EQ(types.size(), 3u);
}

TEST(DomDocument, UnregisterId) {
    Document doc;
    auto elem = doc.create_element("div");
    Element* elem_ptr = elem.get();
    doc.register_id("foo", elem_ptr);
    EXPECT_EQ(doc.get_element_by_id("foo"), elem_ptr);
    doc.unregister_id("foo");
    EXPECT_EQ(doc.get_element_by_id("foo"), nullptr);
}

TEST(DomNode, FirstAndLastChildEmpty) {
    Element elem("div");
    EXPECT_EQ(elem.first_child(), nullptr);
    EXPECT_EQ(elem.last_child(), nullptr);
}

TEST(DomElement, ClassListFromElement) {
    Element elem("div");
    elem.class_list().add("foo");
    elem.class_list().add("bar");
    EXPECT_TRUE(elem.class_list().contains("foo"));
    EXPECT_TRUE(elem.class_list().contains("bar"));
    EXPECT_EQ(elem.class_list().length(), 2u);
}

// ---------------------------------------------------------------------------
// Cycle 431 — DOM attribute vector, id-clear, dirty-on-set, ClassList items,
//             text_content empty, remove-preserves-others, Document node type,
//             and fresh element attribute count
// ---------------------------------------------------------------------------

TEST(DomElement, AttributesVectorPreservesInsertionOrder) {
    Element elem("div");
    elem.set_attribute("name", "test");
    elem.set_attribute("class", "main");
    elem.set_attribute("id", "root");

    const auto& attrs = elem.attributes();
    ASSERT_EQ(attrs.size(), 3u);
    EXPECT_EQ(attrs[0].name, "name");
    EXPECT_EQ(attrs[0].value, "test");
    EXPECT_EQ(attrs[1].name, "class");
    EXPECT_EQ(attrs[1].value, "main");
    EXPECT_EQ(attrs[2].name, "id");
    EXPECT_EQ(attrs[2].value, "root");
}

TEST(DomElement, RemoveIdAttributeClearsIdAccessor) {
    Element elem("div");
    elem.set_attribute("id", "hero");
    EXPECT_EQ(elem.id(), "hero");
    elem.remove_attribute("id");
    EXPECT_EQ(elem.id(), "");
    EXPECT_FALSE(elem.has_attribute("id"));
}

TEST(DomElement, SetAttributeMarksDirtyStyle) {
    Element elem("span");
    EXPECT_EQ(elem.dirty_flags(), DirtyFlags::None);
    elem.set_attribute("data-x", "1");
    // set_attribute triggers on_attribute_changed which marks Style dirty
    EXPECT_NE(static_cast<uint8_t>(elem.dirty_flags() & DirtyFlags::Style), 0u);
}

TEST(DomElement, ClassListItemsAccessor) {
    Element elem("p");
    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    const auto& items = elem.class_list().items();
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0], "alpha");
    EXPECT_EQ(items[1], "beta");
}

TEST(DomElement, TextContentEmptyElement) {
    Element elem("div");
    EXPECT_EQ(elem.text_content(), "");
}

TEST(DomElement, RemoveAttributePreservesOthers) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    elem.set_attribute("placeholder", "Enter name");
    ASSERT_EQ(elem.attributes().size(), 3u);

    elem.remove_attribute("name");

    ASSERT_EQ(elem.attributes().size(), 2u);
    EXPECT_EQ(elem.get_attribute("type").value(), "text");
    EXPECT_EQ(elem.get_attribute("placeholder").value(), "Enter name");
    EXPECT_FALSE(elem.has_attribute("name"));
}

TEST(DomDocument, DocumentNodeType) {
    Document doc;
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

TEST(DomElement, FreshElementHasNoAttributes) {
    Element elem("section");
    EXPECT_EQ(elem.attributes().size(), 0u);
    EXPECT_EQ(elem.id(), "");
    EXPECT_EQ(elem.class_list().length(), 0u);
}

// ---------------------------------------------------------------------------
// Cycle 452 — DOM tree manipulation: append_child, insert_before, remove_child,
//             first_child, last_child, next_sibling, prev_sibling,
//             child_count, text content with children
// ---------------------------------------------------------------------------

TEST(DomNode, AppendChildAndFirstLastChild) {
    Document doc;
    auto parent = std::make_unique<Element>("div");

    auto child1 = std::make_unique<Element>("span");
    auto child2 = std::make_unique<Element>("p");

    auto* c1 = child1.get();
    auto* c2 = child2.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));

    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), c1);
    EXPECT_EQ(parent->last_child(), c2);
}

TEST(DomNode, SiblingNavigation) {
    Document doc;
    auto parent = std::make_unique<Element>("ul");

    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");

    auto* p1 = li1.get();
    auto* p2 = li2.get();
    auto* p3 = li3.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));

    EXPECT_EQ(p1->next_sibling(), p2);
    EXPECT_EQ(p2->next_sibling(), p3);
    EXPECT_EQ(p3->next_sibling(), nullptr);

    EXPECT_EQ(p3->previous_sibling(), p2);
    EXPECT_EQ(p2->previous_sibling(), p1);
    EXPECT_EQ(p1->previous_sibling(), nullptr);
}

TEST(DomNode, InsertBeforeMiddleChild) {
    Document doc;
    auto parent = std::make_unique<Element>("div");

    auto first = std::make_unique<Element>("a");
    auto second = std::make_unique<Element>("b");
    auto inserted = std::make_unique<Element>("ins");

    auto* f = first.get();
    auto* s = second.get();
    auto* ins = inserted.get();

    parent->append_child(std::move(first));
    parent->append_child(std::move(second));

    // Insert before second
    parent->insert_before(std::move(inserted), s);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), f);
    EXPECT_EQ(f->next_sibling(), ins);
    EXPECT_EQ(ins->next_sibling(), s);
}

TEST(DomNode, RemoveChildFromParent) {
    Document doc;
    auto parent = std::make_unique<Element>("div");

    auto child = std::make_unique<Element>("span");
    auto* cp = child.get();

    parent->append_child(std::move(child));
    EXPECT_EQ(parent->child_count(), 1u);

    auto removed = parent->remove_child(*cp);
    EXPECT_EQ(parent->child_count(), 0u);
    EXPECT_NE(removed, nullptr);
}

TEST(DomText, TextNodeContent) {
    Text text_node("Hello World");
    EXPECT_EQ(text_node.node_type(), NodeType::Text);
    EXPECT_EQ(text_node.data(), "Hello World");
}

TEST(DomComment, CommentNodeContent) {
    Comment comment("This is a comment");
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
    EXPECT_EQ(comment.data(), "This is a comment");
}

TEST(DomElement, TextContentFromChildren) {
    auto parent = std::make_unique<Element>("p");
    parent->append_child(std::make_unique<Text>("Hello "));
    parent->append_child(std::make_unique<Text>("World"));

    EXPECT_EQ(parent->text_content(), "Hello World");
}

TEST(DomDocument, CreateButtonElementViaDocument) {
    Document doc;
    auto elem = doc.create_element("button");
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "button");
    EXPECT_EQ(elem->node_type(), NodeType::Element);
}

// ---------------------------------------------------------------------------
// Cycle 488 — DOM additional edge-case tests
// ---------------------------------------------------------------------------

// Remove middle child — siblings of remaining children updated correctly
TEST(DomNode, RemoveMiddleChildUpdatesSiblings) {
    auto parent = std::make_unique<Element>("div");
    auto c1 = std::make_unique<Element>("a");
    auto c2 = std::make_unique<Element>("b");
    auto c3 = std::make_unique<Element>("c");
    auto* p1 = c1.get();
    auto* p2 = c2.get();
    auto* p3 = c3.get();
    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    parent->append_child(std::move(c3));

    parent->remove_child(*p2);

    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(p1->next_sibling(), p3);
    EXPECT_EQ(p3->previous_sibling(), p1);
    EXPECT_EQ(p1->previous_sibling(), nullptr);
    EXPECT_EQ(p3->next_sibling(), nullptr);
}

// ClassList::toggle adds when absent
TEST(DomClassList, ToggleAddsWhenAbsent) {
    Element elem("div");
    elem.class_list().toggle("foo");
    EXPECT_TRUE(elem.class_list().contains("foo"));
    EXPECT_EQ(elem.class_list().length(), 1u);
}

// ClassList::toggle removes when present
TEST(DomClassList, ToggleRemovesWhenPresent) {
    Element elem("div");
    elem.class_list().add("bar");
    elem.class_list().toggle("bar");
    EXPECT_FALSE(elem.class_list().contains("bar"));
    EXPECT_EQ(elem.class_list().length(), 0u);
}

// Event::type() returns the type string passed at construction
TEST(DomEvent, EventTypeProperty) {
    Event e("mouseover");
    EXPECT_EQ(e.type(), "mouseover");

    Event e2("keydown", false, false);
    EXPECT_EQ(e2.type(), "keydown");
}

// Listener for "click" not called when "keydown" event is dispatched
TEST(DomEvent, DifferentEventTypeListenerNotCalled) {
    EventTarget target;
    bool click_called = false;
    target.add_event_listener("click", [&](Event&) { click_called = true; }, false);

    Event event("keydown");
    auto node = std::make_unique<Element>("div");
    event.target_ = node.get();
    event.current_target_ = node.get();
    event.phase_ = EventPhase::AtTarget;
    target.dispatch_event(event, *node);

    EXPECT_FALSE(click_called);
}

// set_attribute on same key multiple times: only 1 entry in attributes vector
TEST(DomElement, AttributeCountAfterRepeatedSetSameKey) {
    Element elem("input");
    elem.set_attribute("class", "a");
    elem.set_attribute("class", "b");
    elem.set_attribute("class", "c");
    EXPECT_EQ(elem.attributes().size(), 1u);
    EXPECT_EQ(elem.get_attribute("class").value(), "c");
}

// Deeply nested element has correct text_content
TEST(DomNode, DeepNestedTextContent) {
    auto outer = std::make_unique<Element>("div");
    auto mid = std::make_unique<Element>("p");
    auto inner = std::make_unique<Element>("span");
    inner->append_child(std::make_unique<Text>("deep text"));

    mid->append_child(std::move(inner));
    outer->append_child(std::move(mid));

    EXPECT_EQ(outer->text_content(), "deep text");
}

// Document::get_element_by_id after calling register_id
TEST(DomDocument, GetElementByIdViaRegisterWithAttribute) {
    Document doc;
    auto div = doc.create_element("div");
    Element* div_ptr = div.get();
    div->set_attribute("id", "target");
    doc.register_id("target", div_ptr);
    doc.append_child(std::move(div));

    EXPECT_EQ(doc.get_element_by_id("target"), div_ptr);
    EXPECT_EQ(doc.get_element_by_id("missing"), nullptr);
}

// ---------------------------------------------------------------------------
// Cycle 494 — DOM additional regression tests
// ---------------------------------------------------------------------------

// Element::tag_name() returns the tag name passed at construction
TEST(DomElement, TagNameAccessor) {
    Element section("section");
    EXPECT_EQ(section.tag_name(), "section");

    Element btn("button", "http://www.w3.org/1999/xhtml");
    EXPECT_EQ(btn.tag_name(), "button");
}

// ClassList::length() reflects the number of distinct classes
TEST(DomClassList, LengthReflectsClassCount) {
    Element elem("div");
    EXPECT_EQ(elem.class_list().length(), 0u);

    elem.class_list().add("a");
    elem.class_list().add("b");
    elem.class_list().add("c");
    EXPECT_EQ(elem.class_list().length(), 3u);

    elem.class_list().remove("b");
    EXPECT_EQ(elem.class_list().length(), 2u);
}

// Event::bubbles() and Event::cancelable() accessors
TEST(DomEvent, BubblesAndCancelableAccessors) {
    Event bubbling("click", true, true);
    EXPECT_TRUE(bubbling.bubbles());
    EXPECT_TRUE(bubbling.cancelable());

    Event non_bubbling("focus", false, false);
    EXPECT_FALSE(non_bubbling.bubbles());
    EXPECT_FALSE(non_bubbling.cancelable());
}

// Event::default_prevented() is false initially
TEST(DomEvent, DefaultPreventedFalseInitially) {
    Event evt("submit", true, true);
    EXPECT_FALSE(evt.default_prevented());

    evt.prevent_default();
    EXPECT_TRUE(evt.default_prevented());
}

// Node::next_sibling() and Node::previous_sibling() explicit traversal
TEST(DomNode, NextAndPreviousSiblingTraversal) {
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li"); // first
    auto li2 = std::make_unique<Element>("li"); // second
    auto li3 = std::make_unique<Element>("li"); // third

    Node* li1_ptr = &parent->append_child(std::move(li1));
    Node* li2_ptr = &parent->append_child(std::move(li2));
    Node* li3_ptr = &parent->append_child(std::move(li3));

    EXPECT_EQ(li1_ptr->next_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->next_sibling(), nullptr);

    EXPECT_EQ(li3_ptr->previous_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(li1_ptr->previous_sibling(), nullptr);
}

// Element::namespace_uri() returns the namespace set at construction
TEST(DomElement, NamespaceUriAccessor) {
    Element svg("svg", "http://www.w3.org/2000/svg");
    EXPECT_EQ(svg.namespace_uri(), "http://www.w3.org/2000/svg");

    Element html("div"); // default empty namespace
    EXPECT_TRUE(html.namespace_uri().empty());
}

// Event::propagation_stopped() is false initially, true after stop_propagation
TEST(DomEvent, PropagationStoppedAccessor) {
    Event evt("click");
    EXPECT_FALSE(evt.propagation_stopped());
    EXPECT_FALSE(evt.immediate_propagation_stopped());

    evt.stop_propagation();
    EXPECT_TRUE(evt.propagation_stopped());
    EXPECT_FALSE(evt.immediate_propagation_stopped()); // only propagation stopped
}

// stop_immediate_propagation sets both flags
TEST(DomEvent, StopImmediatePropagationSetsBothFlags) {
    Event evt("click");
    evt.stop_immediate_propagation();
    EXPECT_TRUE(evt.propagation_stopped());
    EXPECT_TRUE(evt.immediate_propagation_stopped());
}

// Child count is updated correctly after append and remove operations
TEST(DomNode, ChildCountUpdatesOnAppendAndRemove) {
    auto parent = std::make_unique<Element>("div");
    EXPECT_EQ(parent->child_count(), 0u);

    parent->append_child(std::make_unique<Element>("span"));
    EXPECT_EQ(parent->child_count(), 1u);

    Node& c2_ref = parent->append_child(std::make_unique<Text>("hello"));
    EXPECT_EQ(parent->child_count(), 2u);

    parent->remove_child(c2_ref);
    EXPECT_EQ(parent->child_count(), 1u);
}

// ============================================================================
// Cycle 505: DOM regression tests
// ============================================================================

TEST(DomNode, InsertBeforeAddsChildAtCorrectPosition) {
    auto parent = std::make_unique<Element>("ul");
    Node* li1 = &parent->append_child(std::make_unique<Element>("li"));
    Node* li3 = &parent->append_child(std::make_unique<Element>("li"));
    Node* li2 = &parent->insert_before(std::make_unique<Element>("li"), li3);
    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(li1->next_sibling(), li2);
    EXPECT_EQ(li2->next_sibling(), li3);
    EXPECT_EQ(li3->previous_sibling(), li2);
}

TEST(DomElement, HasAttributeReturnsTrueAfterSet) {
    Element e("div");
    EXPECT_FALSE(e.has_attribute("class"));
    e.set_attribute("class", "foo");
    EXPECT_TRUE(e.has_attribute("class"));
}

TEST(DomElement, RemoveAttributeThenHasReturnsFalse) {
    Element e("input");
    e.set_attribute("type", "text");
    EXPECT_TRUE(e.has_attribute("type"));
    e.remove_attribute("type");
    EXPECT_FALSE(e.has_attribute("type"));
}

TEST(DomNode, ForEachChildIteratesAllChildren) {
    auto parent = std::make_unique<Element>("div");
    parent->append_child(std::make_unique<Element>("span"));
    parent->append_child(std::make_unique<Text>("hello"));
    parent->append_child(std::make_unique<Element>("em"));
    int count = 0;
    parent->for_each_child([&](const Node&) { count++; });
    EXPECT_EQ(count, 3);
}

TEST(DomNode, LastChildAfterMultipleAppends) {
    auto p = std::make_unique<Element>("p");
    p->append_child(std::make_unique<Text>("first"));
    Node* last = &p->append_child(std::make_unique<Text>("last"));
    EXPECT_EQ(p->last_child(), last);
}

TEST(DomNode, FirstChildAfterAppend) {
    auto p = std::make_unique<Element>("p");
    Node* first = &p->append_child(std::make_unique<Text>("first"));
    p->append_child(std::make_unique<Text>("second"));
    EXPECT_EQ(p->first_child(), first);
}

TEST(DomNode, DirtyFlagsAfterMarkAndClear) {
    Element e("div");
    EXPECT_EQ(e.dirty_flags(), DirtyFlags::None);
    e.mark_dirty(DirtyFlags::Style);
    EXPECT_EQ(e.dirty_flags() & DirtyFlags::Style, DirtyFlags::Style);
    e.clear_dirty();
    EXPECT_EQ(e.dirty_flags(), DirtyFlags::None);
}

TEST(DomClassList, ToStringContainsAllClasses) {
    ClassList cl;
    cl.add("foo");
    cl.add("bar");
    cl.add("baz");
    std::string s = cl.to_string();
    EXPECT_NE(s.find("foo"), std::string::npos);
    EXPECT_NE(s.find("bar"), std::string::npos);
    EXPECT_NE(s.find("baz"), std::string::npos);
}

// ============================================================================
// Cycle 513: DOM regression tests
// ============================================================================

TEST(DomNode, RemoveOnlyChildLeavesEmptyParent) {
    auto parent = std::make_unique<Element>("div");
    Node* child = &parent->append_child(std::make_unique<Element>("span"));
    EXPECT_EQ(parent->child_count(), 1u);
    parent->remove_child(*child);
    EXPECT_EQ(parent->child_count(), 0u);
    EXPECT_EQ(parent->first_child(), nullptr);
    EXPECT_EQ(parent->last_child(), nullptr);
}

TEST(DomNode, InsertBeforeFirstChildMakesItSecond) {
    auto parent = std::make_unique<Element>("ul");
    Node* li1 = &parent->append_child(std::make_unique<Element>("li"));
    Node* li0 = &parent->insert_before(std::make_unique<Element>("li"), li1);
    EXPECT_EQ(parent->first_child(), li0);
    EXPECT_EQ(li0->next_sibling(), li1);
    EXPECT_EQ(li1->previous_sibling(), li0);
}

TEST(DomNode, ChildCountAfterMixedOps) {
    auto parent = std::make_unique<Element>("div");
    Node* a = &parent->append_child(std::make_unique<Element>("a"));
    Node* b = &parent->append_child(std::make_unique<Element>("b"));
    parent->append_child(std::make_unique<Element>("c"));
    EXPECT_EQ(parent->child_count(), 3u);
    parent->remove_child(*b);
    EXPECT_EQ(parent->child_count(), 2u);
    parent->insert_before(std::make_unique<Element>("x"), a);
    EXPECT_EQ(parent->child_count(), 3u);
}

TEST(DomElement, MultipleAttributesPreserveAllValues) {
    Element e("input");
    e.set_attribute("type", "text");
    e.set_attribute("name", "username");
    e.set_attribute("placeholder", "Enter name");
    EXPECT_EQ(e.get_attribute("type").value_or(""), "text");
    EXPECT_EQ(e.get_attribute("name").value_or(""), "username");
    EXPECT_EQ(e.get_attribute("placeholder").value_or(""), "Enter name");
    EXPECT_EQ(e.attributes().size(), 3u);
}

TEST(DomElement, TextContentFromNestedElements) {
    auto outer = std::make_unique<Element>("p");
    auto inner = std::make_unique<Element>("strong");
    inner->append_child(std::make_unique<Text>("bold"));
    outer->append_child(std::move(inner));
    outer->append_child(std::make_unique<Text>(" text"));
    EXPECT_EQ(outer->text_content(), "bold text");
}

TEST(DomClassList, ItemCountAfterRemoveAndAdd) {
    ClassList cl;
    cl.add("a");
    cl.add("b");
    cl.add("c");
    EXPECT_EQ(cl.length(), 3u);
    cl.remove("b");
    EXPECT_EQ(cl.length(), 2u);
    cl.add("d");
    EXPECT_EQ(cl.length(), 3u);
    EXPECT_TRUE(cl.contains("a"));
    EXPECT_FALSE(cl.contains("b"));
    EXPECT_TRUE(cl.contains("d"));
}

TEST(DomEvent, ListenerCalledOnlyOncePerDispatch) {
    EventTarget target;
    int call_count = 0;
    target.add_event_listener("click", [&call_count](Event&) { call_count++; });
    auto node = std::make_unique<Element>("button");
    Event ev("click", true, true);
    ev.target_ = node.get();
    ev.current_target_ = node.get();
    ev.phase_ = EventPhase::AtTarget;
    target.dispatch_event(ev, *node);
    EXPECT_EQ(call_count, 1);
}

TEST(DomDocument, CreateElementHasCorrectTagName) {
    Document doc;
    auto el = doc.create_element("article");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name(), "article");
}

// ============================================================================
// Cycle 526: DOM regression tests
// ============================================================================

TEST(DomDocument, CreateTextNodeHasCorrectData) {
    Document doc;
    auto text = doc.create_text_node("hello world");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->data(), "hello world");
}

TEST(DomElement, ClassListMultipleClassesContainsAll) {
    Element e("div");
    e.class_list().add("foo");
    e.class_list().add("bar");
    e.class_list().add("baz");
    auto& cl = e.class_list();
    EXPECT_TRUE(cl.contains("foo"));
    EXPECT_TRUE(cl.contains("bar"));
    EXPECT_TRUE(cl.contains("baz"));
    EXPECT_EQ(cl.length(), 3u);
}

TEST(DomElement, RemoveNonexistentAttributeIsNoOp) {
    Element e("div");
    e.set_attribute("data-x", "1");
    // Removing an attribute that doesn't exist should not crash
    e.remove_attribute("nonexistent");
    EXPECT_EQ(e.attributes().size(), 1u);
}

TEST(DomNode, SiblingPointersClearedOnRemove) {
    auto parent = std::make_unique<Element>("ul");
    Node* li1 = &parent->append_child(std::make_unique<Element>("li"));
    Node* li2 = &parent->append_child(std::make_unique<Element>("li"));
    Node* li3 = &parent->append_child(std::make_unique<Element>("li"));
    parent->remove_child(*li2);  // remove middle
    // li1 and li3 should now be adjacent
    EXPECT_EQ(li1->next_sibling(), li3);
    EXPECT_EQ(li3->previous_sibling(), li1);
}

TEST(DomNode, AppendChildReturnReference) {
    auto parent = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = static_cast<Element*>(child.get());
    Node& ref = parent->append_child(std::move(child));
    // The returned reference should be the same node
    EXPECT_EQ(&ref, child_ptr);
}

TEST(DomText, SetDataUpdatesTextContent) {
    Text t("original");
    EXPECT_EQ(t.data(), "original");
    t.set_data("updated");
    EXPECT_EQ(t.data(), "updated");
    EXPECT_EQ(t.text_content(), "updated");
}

TEST(DomEvent, EventTypeIsPreserved) {
    Event e("keydown", true, true);
    EXPECT_EQ(e.type(), "keydown");
}

TEST(DomNode, EmptyParentHasNullFirstLast) {
    auto parent = std::make_unique<Element>("div");
    EXPECT_EQ(parent->first_child(), nullptr);
    EXPECT_EQ(parent->last_child(), nullptr);
    EXPECT_EQ(parent->child_count(), 0u);
}

// ============================================================================
// Cycle 537: DOM regression tests
// ============================================================================

// Element has no children initially
TEST(DomNode, NewElementHasNoChildren) {
    Element e("div");
    EXPECT_EQ(e.child_count(), 0u);
    EXPECT_EQ(e.first_child(), nullptr);
    EXPECT_EQ(e.last_child(), nullptr);
}

// Append two children, verify order
TEST(DomNode, TwoChildrenPreserveOrder) {
    auto parent = std::make_unique<Element>("ul");
    auto& li1 = parent->append_child(std::make_unique<Element>("li"));
    auto& li2 = parent->append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent->first_child(), &li1);
    EXPECT_EQ(parent->last_child(), &li2);
    EXPECT_EQ(parent->child_count(), 2u);
}

// Text node initial data
TEST(DomText, InitialDataIsPreserved) {
    Text t("hello world");
    EXPECT_EQ(t.data(), "hello world");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

// Element tag name returns lowercase
TEST(DomElement, TagNamePreservedAsGiven) {
    Element e("section");
    EXPECT_EQ(e.tag_name(), "section");
}

// Element: has_attribute returns false if unset
TEST(DomElement, HasAttributeReturnsFalseWhenNotSet) {
    Element e("div");
    EXPECT_FALSE(e.has_attribute("data-value"));
}

// Element: has_attribute returns true after set
TEST(DomElement, HasAttributeReturnsTrueOnInput) {
    Element e("input");
    e.set_attribute("type", "text");
    EXPECT_TRUE(e.has_attribute("type"));
}

// ClassList: remove class that isn't present — no crash
TEST(DomClassList, RemoveNonexistentClassIsNoOp) {
    Element e("p");
    e.class_list().add("active");
    e.class_list().remove("nonexistent");  // should not crash
    EXPECT_EQ(e.class_list().length(), 1u);
    EXPECT_TRUE(e.class_list().contains("active"));
}

// Comment node type
TEST(DomComment, CommentNodeTypeIsComment) {
    Comment c("a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
    EXPECT_EQ(c.data(), "a comment");
}

// ============================================================================
// Cycle 546: DOM regression tests
// ============================================================================

// Element: get_attribute returns correct value
TEST(DomElement, GetAttributeReturnsValue) {
    Element e("img");
    e.set_attribute("src", "photo.jpg");
    auto val = e.get_attribute("src");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "photo.jpg");
}

// Element: get_attribute returns nullopt for missing attribute
TEST(DomElement, GetAttributeNulloptForMissing) {
    Element e("div");
    EXPECT_FALSE(e.get_attribute("nonexistent").has_value());
}

// Document: create_element sets correct node type
TEST(DomDocument, CreateElementNodeType) {
    Document doc;
    auto el = doc.create_element("p");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->node_type(), NodeType::Element);
}

// Document: create_text_node sets correct node type
TEST(DomDocument, CreateTextNodeType) {
    Document doc;
    auto t = doc.create_text_node("sample");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->node_type(), NodeType::Text);
}

// Node: append_child increases child_count
TEST(DomNode, AppendChildIncrementsCount) {
    auto parent = std::make_unique<Element>("div");
    EXPECT_EQ(parent->child_count(), 0u);
    parent->append_child(std::make_unique<Element>("span"));
    EXPECT_EQ(parent->child_count(), 1u);
    parent->append_child(std::make_unique<Element>("span"));
    EXPECT_EQ(parent->child_count(), 2u);
}

// Element: tag_name() for different tag names
TEST(DomElement, DifferentTagNames) {
    Element e1("header");
    Element e2("footer");
    Element e3("nav");
    EXPECT_EQ(e1.tag_name(), "header");
    EXPECT_EQ(e2.tag_name(), "footer");
    EXPECT_EQ(e3.tag_name(), "nav");
}

// ClassList: toggle adds and removes alternately
TEST(DomClassList, ToggleAddsAndRemoves) {
    Element e("p");
    e.class_list().toggle("active");  // should add
    EXPECT_TRUE(e.class_list().contains("active"));
    e.class_list().toggle("active");  // should remove
    EXPECT_FALSE(e.class_list().contains("active"));
}

// Text node: data can be updated multiple times
TEST(DomText, DataUpdatedMultipleTimes) {
    Text t("first");
    t.set_data("second");
    EXPECT_EQ(t.data(), "second");
    t.set_data("third");
    EXPECT_EQ(t.data(), "third");
}

// ============================================================================
// Cycle 556: DOM regression tests
// ============================================================================

// Element: attributes initially empty
TEST(DomElement, AttributesInitiallyEmpty) {
    Element e("div");
    EXPECT_TRUE(e.attributes().empty());
    EXPECT_EQ(e.attributes().size(), 0u);
}

// Element: set two attributes, count is 2
TEST(DomElement, TwoAttributesCount) {
    Element e("input");
    e.set_attribute("type", "text");
    e.set_attribute("placeholder", "Enter text");
    EXPECT_EQ(e.attributes().size(), 2u);
}

// Node: parent_node is null initially
TEST(DomNode, ParentNodeNullInitially) {
    auto el = std::make_unique<Element>("div");
    EXPECT_EQ(el->parent(), nullptr);
}

// Node: parent_node is set after append_child
TEST(DomNode, ParentNodeSetAfterAppend) {
    auto parent = std::make_unique<Element>("div");
    Element* child_ptr = static_cast<Element*>(
        &parent->append_child(std::make_unique<Element>("span")));
    EXPECT_EQ(child_ptr->parent(), parent.get());
}

// ClassList: items() returns all classes
TEST(DomClassList, ItemsVectorHasAllClasses) {
    Element e("div");
    e.class_list().add("first");
    e.class_list().add("second");
    const auto& items = e.class_list().items();
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0], "first");
    EXPECT_EQ(items[1], "second");
}

// ClassList: single class, items() has size 1
TEST(DomClassList, SingleClassItemsSize) {
    Element e("div");
    e.class_list().add("only");
    EXPECT_EQ(e.class_list().items().size(), 1u);
    EXPECT_EQ(e.class_list().items()[0], "only");
}

// Element: text_content from single text child
TEST(DomElement, TextContentFromSingleChild) {
    auto el = std::make_unique<Element>("p");
    el->append_child(std::make_unique<Text>("Hello!"));
    EXPECT_EQ(el->text_content(), "Hello!");
}

// Event: bubbles and cancelable set correctly
TEST(DomEvent, BubblesAndCancelableSetInConstructor) {
    Event e("click", true, false);
    EXPECT_TRUE(e.bubbles());
    EXPECT_FALSE(e.cancelable());
}

// ============================================================================
// Cycle 562: DOM node traversal, event methods, document
// ============================================================================

// Node: first_child returns first appended child
TEST(DomNode, FirstChildIsFirstAppended) {
    auto parent = std::make_unique<Element>("ul");
    parent->append_child(std::make_unique<Element>("li"));
    parent->append_child(std::make_unique<Element>("li"));
    ASSERT_NE(parent->first_child(), nullptr);
    EXPECT_EQ(parent->first_child()->node_type(), NodeType::Element);
}

// Node: last_child returns last appended child
TEST(DomNode, LastChildIsLastAppended) {
    auto parent = std::make_unique<Element>("ul");
    auto& first = parent->append_child(std::make_unique<Element>("li"));
    auto& last  = parent->append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent->last_child(), &last);
    EXPECT_NE(parent->last_child(), &first);
}

// Node: next_sibling traversal
TEST(DomNode, NextSiblingTraversal) {
    auto parent = std::make_unique<Element>("div");
    auto& a = parent->append_child(std::make_unique<Element>("a"));
    auto& b = parent->append_child(std::make_unique<Element>("b"));
    EXPECT_EQ(a.next_sibling(), &b);
    EXPECT_EQ(b.next_sibling(), nullptr);
}

// Node: previous_sibling traversal
TEST(DomNode, PreviousSiblingTraversal) {
    auto parent = std::make_unique<Element>("div");
    auto& a = parent->append_child(std::make_unique<Element>("a"));
    auto& b = parent->append_child(std::make_unique<Element>("b"));
    EXPECT_EQ(b.previous_sibling(), &a);
    EXPECT_EQ(a.previous_sibling(), nullptr);
}

// Event: type() returns the event type
TEST(DomEvent, TypeReturnsEventType) {
    Event e("mousedown", true, true);
    EXPECT_EQ(e.type(), "mousedown");
}

// Event: prevent_default sets default_prevented
TEST(DomEvent, PreventDefaultSetsFlag) {
    Event e("submit", true, true);
    EXPECT_FALSE(e.default_prevented());
    e.prevent_default();
    EXPECT_TRUE(e.default_prevented());
}

// Event: prevent_default is no-op when not cancelable
TEST(DomEvent, PreventDefaultNoOpForNonCancelable) {
    Event e("click", true, false);
    e.prevent_default();
    EXPECT_FALSE(e.default_prevented());
}

// Document: create_element returns element with correct tag
TEST(DomDocument, CreateElementHasCorrectTag) {
    Document doc;
    auto el = doc.create_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name(), "section");
    EXPECT_EQ(el->node_type(), NodeType::Element);
}

// ============================================================================
// Cycle 574: More DOM tests
// ============================================================================

// Element: set_attribute updates existing attribute
TEST(DomElement, SetAttributeUpdatesExisting) {
    Element e("input");
    e.set_attribute("type", "text");
    e.set_attribute("type", "email");
    auto val = e.get_attribute("type");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "email");
}

// Element: attributes() method returns count
TEST(DomElement, AttributesMethodCount) {
    Element e("a");
    e.set_attribute("href", "https://example.com");
    e.set_attribute("target", "_blank");
    EXPECT_EQ(e.attributes().size(), 2u);
}

// Element: id() convenience (via attribute)
TEST(DomElement, IdAttributeAccessible) {
    Element e("section");
    e.set_attribute("id", "main");
    auto val = e.get_attribute("id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "main");
}

// Text: data() returns the text content
TEST(DomText, DataReturnsContent) {
    Text t("Hello, World!");
    EXPECT_EQ(t.data(), "Hello, World!");
}

// Text: set_data updates content
TEST(DomText, SetDataUpdatesContent) {
    Text t("initial");
    t.set_data("updated");
    EXPECT_EQ(t.data(), "updated");
}

// Comment: data() returns comment content
TEST(DomComment, DataReturnsCommentText) {
    Comment c("This is a comment");
    EXPECT_EQ(c.data(), "This is a comment");
}

// Node: remove_child removes the node
TEST(DomNode, RemoveChildReducesCount) {
    auto parent = std::make_unique<Element>("ul");
    auto& child = parent->append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent->child_count(), 1u);
    parent->remove_child(child);
    EXPECT_EQ(parent->child_count(), 0u);
}

// Event: phase initially None
TEST(DomEvent, PhaseInitiallyNone) {
    Event e("keydown", true, true);
    EXPECT_EQ(e.phase(), EventPhase::None);
}

// ============================================================================
// Cycle 582: More DOM tests
// ============================================================================

// Document: create_text_node returns correct content
TEST(DomDocument, CreateTextNodeContent) {
    Document doc;
    auto t = doc.create_text_node("hello text");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->data(), "hello text");
}

// Element: namespace_uri is empty by default
TEST(DomElement, NamespaceURIEmptyByDefault) {
    Element e("div");
    EXPECT_TRUE(e.namespace_uri().empty());
}

// Element: namespace_uri is set on construction with namespace
TEST(DomElement, NamespaceURISetInConstructor) {
    Element e("svg", "http://www.w3.org/2000/svg");
    EXPECT_EQ(e.namespace_uri(), "http://www.w3.org/2000/svg");
}

// Node: child_count after three appends
TEST(DomNode, ChildCountThreeAfterThreeAppends) {
    auto parent = std::make_unique<Element>("ol");
    parent->append_child(std::make_unique<Element>("li"));
    parent->append_child(std::make_unique<Element>("li"));
    parent->append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent->child_count(), 3u);
}

// Node: child_count returns 0 initially
TEST(DomNode, ChildCountZeroInitially) {
    Element e("p");
    EXPECT_EQ(e.child_count(), 0u);
}

// ClassList: contains returns false initially
TEST(DomClassList, ContainsFalseInitially) {
    Element e("div");
    EXPECT_FALSE(e.class_list().contains("active"));
}

// ClassList: contains returns true after add
TEST(DomClassList, ContainsTrueAfterAdd) {
    Element e("div");
    e.class_list().add("active");
    EXPECT_TRUE(e.class_list().contains("active"));
}

// ClassList: remove makes contains return false
TEST(DomClassList, RemoveMakesContainsFalse) {
    Element e("div");
    e.class_list().add("visible");
    e.class_list().remove("visible");
    EXPECT_FALSE(e.class_list().contains("visible"));
}

// ============================================================================
// Cycle 591: More DOM tests
// ============================================================================

// Element: node_type is Element
TEST(DomElement, NodeTypeIsElement) {
    Element e("span");
    EXPECT_EQ(e.node_type(), NodeType::Element);
}

// Text: node_type is Text
TEST(DomText, NodeTypeIsText) {
    Text t("content");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

// Comment: node_type is Comment
TEST(DomComment, NodeTypeIsComment) {
    Comment c("a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

// Element: remove_attribute makes has_attribute false
TEST(DomElement, RemoveAttributeMakesHasFalse) {
    Element e("input");
    e.set_attribute("disabled", "");
    EXPECT_TRUE(e.has_attribute("disabled"));
    e.remove_attribute("disabled");
    EXPECT_FALSE(e.has_attribute("disabled"));
}

// Element: set_attribute with empty value works
TEST(DomElement, SetAttributeEmptyValue) {
    Element e("input");
    e.set_attribute("checked", "");
    EXPECT_TRUE(e.has_attribute("checked"));
    auto val = e.get_attribute("checked");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "");
}

// Node: first_child is nullptr when no children
TEST(DomNode, FirstChildNullWhenNoChildren) {
    Element e("div");
    EXPECT_EQ(e.first_child(), nullptr);
}

// Node: last_child is nullptr when no children
TEST(DomNode, LastChildNullWhenNoChildren) {
    Element e("div");
    EXPECT_EQ(e.last_child(), nullptr);
}

// Document: node_type is Document
TEST(DomDocument, NodeTypeIsDocument) {
    Document doc;
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

// ============================================================================
// Cycle 597: More DOM tests
// ============================================================================

// Document: can create element via Document
TEST(DomDocument, CreateElementReturnsElement) {
    Document doc;
    auto elem = doc.create_element("span");
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "span");
}

// Document: can create text node via Document
TEST(DomDocument, CreateTextNodeReturnsText) {
    Document doc;
    auto text = doc.create_text_node("hello");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->data(), "hello");
}

// Element: tag_name returns correct value
TEST(DomElement, TagNameLowerCase) {
    Element e("section");
    EXPECT_EQ(e.tag_name(), "section");
}

// Element: has_attribute returns false initially
TEST(DomElement, HasAttributeFalseInitially) {
    Element e("div");
    EXPECT_FALSE(e.has_attribute("class"));
}

// Element: set multiple attributes
TEST(DomElement, SetTwoAttributesAccessible) {
    Element e("input");
    e.set_attribute("type", "text");
    e.set_attribute("name", "username");
    EXPECT_TRUE(e.has_attribute("type"));
    EXPECT_TRUE(e.has_attribute("name"));
}

// Text: node_type is Text (v2 — from text "world")
TEST(DomText, NodeTypeIsTextV2) {
    Text t("world");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

// Comment: node_type is Comment (v2 — different content)
TEST(DomComment, NodeTypeIsCommentV2) {
    Comment c("another comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

// Element: child_count zero initially
TEST(DomElement, ChildCountZeroInitiallyV2) {
    Element e("article");
    EXPECT_EQ(e.child_count(), 0u);
}

// ============================================================================
// Cycle 608: More DOM tests
// ============================================================================

// Element: class_list toggle adds "hidden" when absent
TEST(DomClassList, ToggleAddsHiddenWhenAbsent) {
    Element e("div");
    e.class_list().toggle("hidden");
    EXPECT_TRUE(e.class_list().contains("hidden"));
}

// Element: class_list toggle removes "visible" when present
TEST(DomClassList, ToggleRemovesVisibleWhenPresent) {
    Element e("div");
    e.class_list().add("visible");
    e.class_list().toggle("visible");
    EXPECT_FALSE(e.class_list().contains("visible"));
}

// Element: class_list items() returns all classes
TEST(DomClassList, ItemsReturnsAllClasses) {
    Element e("div");
    e.class_list().add("foo");
    e.class_list().add("bar");
    auto items = e.class_list().items();
    EXPECT_EQ(items.size(), 2u);
}

// Element: remove_attribute then has_attribute false
TEST(DomElement, RemoveAttributeThenHasFalseV2) {
    Element e("div");
    e.set_attribute("data-id", "42");
    e.remove_attribute("data-id");
    EXPECT_FALSE(e.has_attribute("data-id"));
}

// Element: get_attribute after set returns value
TEST(DomElement, GetAttributeAfterSet) {
    Element e("input");
    e.set_attribute("maxlength", "100");
    auto val = e.get_attribute("maxlength");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "100");
}

// Text: set_data changes content
TEST(DomText, SetDataChangesContent) {
    Text t("original");
    t.set_data("changed");
    EXPECT_EQ(t.data(), "changed");
}

// Element: append then child_count is one
TEST(DomElement, AppendTextChildCount) {
    Element e("p");
    auto t = std::make_unique<Text>("hello");
    e.append_child(std::move(t));
    EXPECT_EQ(e.child_count(), 1u);
}

// Document: node_type is Document v2
TEST(DomDocument, NodeTypeIsDocumentV2) {
    Document doc;
    EXPECT_NE(doc.node_type(), NodeType::Element);
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

// ============================================================================
// Cycle 617: More DOM tests
// ============================================================================

// Element: namespace URI is settable
TEST(DomElement, NamespaceURISettable) {
    Element e("rect", "http://www.w3.org/2000/svg");
    EXPECT_EQ(e.namespace_uri(), "http://www.w3.org/2000/svg");
}

// Element: append two children, first_child correct
TEST(DomElement, FirstChildAfterTwoAppends) {
    Element parent("div");
    auto c1 = std::make_unique<Element>("span");
    auto c2 = std::make_unique<Element>("p");
    Element* c1_ptr = c1.get();
    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    EXPECT_EQ(parent.first_child(), c1_ptr);
}

// Element: append two children, last_child correct
TEST(DomElement, LastChildAfterTwoAppends) {
    Element parent("div");
    auto c1 = std::make_unique<Element>("span");
    auto c2 = std::make_unique<Element>("p");
    Element* c2_ptr = c2.get();
    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    EXPECT_EQ(parent.last_child(), c2_ptr);
}

// Element: parent() after append
TEST(DomElement, ParentAfterAppend) {
    Element parent("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();
    parent.append_child(std::move(child));
    EXPECT_EQ(child_ptr->parent(), &parent);
}

// Event: type accessible
TEST(DomEvent, TypeAccessible) {
    Event e("click", true, true);
    EXPECT_EQ(e.type(), "click");
}

// Event: bubbles and cancelable
TEST(DomEvent, BubblesAndCancelable) {
    Event e("submit", true, true);
    EXPECT_TRUE(e.bubbles());
    EXPECT_TRUE(e.cancelable());
}

// Text: initial data from constructor
TEST(DomText, InitialDataFromConstructor) {
    Text t("initial text");
    EXPECT_EQ(t.data(), "initial text");
}

// Comment: initial data from constructor
TEST(DomComment, InitialDataFromConstructor) {
    Comment c("comment text");
    EXPECT_EQ(c.data(), "comment text");
}

// ============================================================================
// Cycle 626: More DOM tests
// ============================================================================

// Element: three attributes set
TEST(DomElement, ThreeAttributesSet) {
    Element e("input");
    e.set_attribute("type", "email");
    e.set_attribute("name", "email");
    e.set_attribute("required", "");
    EXPECT_TRUE(e.has_attribute("type"));
    EXPECT_TRUE(e.has_attribute("name"));
    EXPECT_TRUE(e.has_attribute("required"));
}

// Element: attributes count
TEST(DomElement, AttributesCountThree) {
    Element e("a");
    e.set_attribute("href", "#");
    e.set_attribute("target", "_blank");
    e.set_attribute("rel", "noopener");
    auto attrs = e.attributes();
    EXPECT_EQ(attrs.size(), 3u);
}

// Element: get_attribute returns empty string value
TEST(DomElement, GetAttributeEmptyStringValue) {
    Element e("input");
    e.set_attribute("disabled", "");
    auto val = e.get_attribute("disabled");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "");
}

// Element: class_list add two items
TEST(DomClassList, AddTwoItems) {
    Element e("div");
    e.class_list().add("btn");
    e.class_list().add("primary");
    EXPECT_TRUE(e.class_list().contains("btn"));
    EXPECT_TRUE(e.class_list().contains("primary"));
}

// Element: class_list remove one of two
TEST(DomClassList, RemoveOneOfTwo) {
    Element e("div");
    e.class_list().add("a");
    e.class_list().add("b");
    e.class_list().remove("a");
    EXPECT_FALSE(e.class_list().contains("a"));
    EXPECT_TRUE(e.class_list().contains("b"));
}

// Document: create_element section returns element type
TEST(DomDocument, CreateSectionElementNodeType) {
    Document doc;
    auto el = doc.create_element("section");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->node_type(), NodeType::Element);
    EXPECT_EQ(el->tag_name(), "section");
}

// Text: parent is nullptr initially
TEST(DomText, ParentNullInitially) {
    Text t("hello");
    EXPECT_EQ(t.parent(), nullptr);
}

// Comment: parent is nullptr initially
TEST(DomComment, ParentNullInitially) {
    Comment c("remark");
    EXPECT_EQ(c.parent(), nullptr);
}

// ============================================================================
// Cycle 634: More DOM tests
// ============================================================================

// Element: namespace URI with SVG
TEST(DomElement, SvgNamespaceURISet) {
    Element el("circle", "http://www.w3.org/2000/svg");
    EXPECT_EQ(el.namespace_uri(), "http://www.w3.org/2000/svg");
}

// Element: set attribute with empty string value for required
TEST(DomElement, SetRequiredAttributeEmpty) {
    Element el("input");
    el.set_attribute("required", "");
    auto val = el.get_attribute("required");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "");
}

// Element: has_attribute returns false for nonexistent key
TEST(DomElement, HasAttributeNonexistentKey) {
    Element el("div");
    EXPECT_FALSE(el.has_attribute("data-x"));
}

// Element: remove attribute makes has_attribute false
TEST(DomElement, RemoveAttributeMakesAbsent) {
    Element el("button");
    el.set_attribute("disabled", "true");
    EXPECT_TRUE(el.has_attribute("disabled"));
    el.remove_attribute("disabled");
    EXPECT_FALSE(el.has_attribute("disabled"));
}

// Element: class_list add two different classes
TEST(DomClassList, AddTwoDifferentClasses) {
    Element el("div");
    el.class_list().add("alpha");
    el.class_list().add("beta");
    EXPECT_TRUE(el.class_list().contains("alpha"));
    EXPECT_TRUE(el.class_list().contains("beta"));
}

// Element: class_list toggle is idempotent on re-add
TEST(DomClassList, ToggleAddRemoveToggle) {
    Element el("p");
    el.class_list().toggle("active");   // adds
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");   // removes
    EXPECT_FALSE(el.class_list().contains("active"));
}

// Document: create_element returns correct tag
TEST(DomDocument, CreateElementTagName) {
    Document doc;
    auto el = doc.create_element("nav");
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->tag_name(), "nav");
}

// Document: create_text_node data accessible
TEST(DomDocument, CreateTextNodeData) {
    Document doc;
    auto t = doc.create_text_node("hello world");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->data(), "hello world");
}

// ============================================================================
// Cycle 642: More DOM tests
// ============================================================================

// Element: node_type is Element
TEST(DomElement, NodeTypeIsElementV3) {
    Element el("main");
    EXPECT_EQ(el.node_type(), NodeType::Element);
}

// Text: node_type is Text
TEST(DomText, NodeTypeIsTextV3) {
    Text t("content");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

// Comment: node_type is Comment
TEST(DomComment, NodeTypeIsCommentV3) {
    Comment c("note");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

// Element: append_child returns non-null child
TEST(DomElement, AppendChildReturnsNonNull) {
    Element parent("div");
    auto child = std::make_unique<Element>("p");
    Element* ptr = child.get();
    parent.append_child(std::move(child));
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(parent.child_count(), 1u);
}

// Element: first_child is nullptr when no children
TEST(DomElement, FirstChildNullWhenEmpty) {
    Element el("div");
    EXPECT_EQ(el.first_child(), nullptr);
}

// Element: last_child is nullptr when no children
TEST(DomElement, LastChildNullWhenEmpty) {
    Element el("span");
    EXPECT_EQ(el.last_child(), nullptr);
}

// Element: class_list is empty initially
TEST(DomClassList, EmptyInitially) {
    Element el("div");
    EXPECT_TRUE(el.class_list().items().empty());
}

// Element: get_attribute returns nullopt for never-set key
TEST(DomElement, GetAttributeNulloptForNeverSetKey) {
    Element el("article");
    auto val = el.get_attribute("data-missing");
    EXPECT_FALSE(val.has_value());
}

// ============================================================================
// Cycle 651: More DOM tests
// ============================================================================

// Element: next sibling is null for single child
TEST(DomElement, NextSiblingNullForSingleChild) {
    Element parent("div");
    auto child = std::make_unique<Element>("p");
    Element* ptr = child.get();
    parent.append_child(std::move(child));
    EXPECT_EQ(ptr->next_sibling(), nullptr);
}

// Element: previous sibling is null for first child
TEST(DomElement, PrevSiblingNullForFirstChild) {
    Element parent("div");
    auto child = std::make_unique<Element>("p");
    Element* ptr = child.get();
    parent.append_child(std::move(child));
    EXPECT_EQ(ptr->previous_sibling(), nullptr);
}

// Element: tag_name is accessible
TEST(DomElement, TagNameAccessible) {
    Element el("footer");
    EXPECT_EQ(el.tag_name(), "footer");
}

// Text: set_data changes content
TEST(DomText, SetDataChangesContentV2) {
    Text t("original");
    t.set_data("updated");
    EXPECT_EQ(t.data(), "updated");
}

// Element: child_count with three children
TEST(DomElement, ChildCountThreeChildren) {
    Element parent("ul");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent.child_count(), 3u);
}

// Element: first_child is first appended
TEST(DomElement, FirstChildIsFirstAppended) {
    Element parent("div");
    auto first = std::make_unique<Element>("h1");
    Element* first_ptr = first.get();
    parent.append_child(std::move(first));
    parent.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(parent.first_child(), first_ptr);
}

// Element: last_child is last appended
TEST(DomElement, LastChildIsLastAppended) {
    Element parent("div");
    parent.append_child(std::make_unique<Element>("h1"));
    auto last = std::make_unique<Element>("p");
    Element* last_ptr = last.get();
    parent.append_child(std::move(last));
    EXPECT_EQ(parent.last_child(), last_ptr);
}

// Event: type accessible
TEST(DomEvent, TypeAccessibleV2) {
    Event ev("mousedown", true, false);
    EXPECT_EQ(ev.type(), "mousedown");
}

// ============================================================================
// Cycle 661: More DOM tests
// ============================================================================

// Element: parent is set after append_child
TEST(DomElement, ParentIsSetAfterAppend) {
    Element parent("section");
    auto child = std::make_unique<Element>("article");
    Element* child_ptr = child.get();
    parent.append_child(std::move(child));
    EXPECT_EQ(child_ptr->parent(), &parent);
}

// Element: has_attribute false for removed attribute
TEST(DomElement, HasAttributeFalseAfterRemove) {
    Element elem("input");
    elem.set_attribute("disabled", "");
    elem.remove_attribute("disabled");
    EXPECT_FALSE(elem.has_attribute("disabled"));
}

// Element: set_attribute overwrites previous value
TEST(DomElement, SetAttributeOverwritesPrevious) {
    Element elem("a");
    elem.set_attribute("href", "/old");
    elem.set_attribute("href", "/new");
    auto val = elem.get_attribute("href");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "/new");
}

// ClassList: toggle adds "selected" class when absent
TEST(DomClassList, ToggleAddsSelectedWhenAbsent) {
    Element elem("li");
    elem.class_list().toggle("selected");
    EXPECT_TRUE(elem.class_list().contains("selected"));
}

// ClassList: toggle removes "selected" class when present
TEST(DomClassList, ToggleRemovesSelectedWhenPresent) {
    Element elem("li");
    elem.class_list().add("selected");
    elem.class_list().toggle("selected");
    EXPECT_FALSE(elem.class_list().contains("selected"));
}

// Text: node type is Text for "world" node
TEST(DomText, NodeTypeIsTextForWorldNode) {
    Text t("world");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

// Comment: data returns text
TEST(DomComment, DataReturnsText) {
    Comment c("this is a comment");
    EXPECT_EQ(c.data(), "this is a comment");
}

// Document: create_element returns non-null
TEST(DomDocument, CreateElementNonNull) {
    Document doc;
    auto elem = doc.create_element("div");
    EXPECT_NE(elem, nullptr);
}

// ============================================================================
// Cycle 668: More DOM tests
// ============================================================================

// Element: two siblings share same parent
TEST(DomElement, TwoSiblingsShareParent) {
    Element parent("div");
    auto c1 = std::make_unique<Element>("h1");
    auto c2 = std::make_unique<Element>("p");
    Element* p1 = c1.get();
    Element* p2 = c2.get();
    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    EXPECT_EQ(p1->parent(), &parent);
    EXPECT_EQ(p2->parent(), &parent);
}

// Element: get_attribute returns "photo.jpg" for src
TEST(DomElement, GetAttributeSrcReturnsPhotoJpg) {
    Element elem("img");
    elem.set_attribute("src", "photo.jpg");
    auto val = elem.get_attribute("src");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "photo.jpg");
}

// Element: child_count is 0 for br element
TEST(DomElement, ChildCountZeroForBrElement) {
    Element leaf("br");
    EXPECT_EQ(leaf.child_count(), 0u);
}

// ClassList: contains "invisible" returns false before add
TEST(DomClassList, ContainsInvisibleFalseBeforeAdd) {
    Element elem("div");
    EXPECT_FALSE(elem.class_list().contains("invisible"));
}

// ClassList: size is zero initially
TEST(DomClassList, SizeIsZeroInitially) {
    Element elem("p");
    EXPECT_EQ(elem.class_list().items().size(), 0u);
}

// ClassList: adding three classes yields size 3
TEST(DomClassList, ThreeClassesYieldSizeThree) {
    Element elem("ul");
    elem.class_list().add("a");
    elem.class_list().add("b");
    elem.class_list().add("c");
    EXPECT_EQ(elem.class_list().items().size(), 3u);
}

// Text: data returns initial text
TEST(DomText, DataReturnsInitialText) {
    Text t("initial text");
    EXPECT_EQ(t.data(), "initial text");
}

// Document: create_text_node data correct
TEST(DomDocument, CreateTextNodeDataCorrect) {
    Document doc;
    auto node = doc.create_text_node("hello");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->data(), "hello");
}

// ============================================================================
// Cycle 676: More DOM tests
// ============================================================================

// Element: multiple attributes accessible
TEST(DomElement, ThreeAttributesAllAccessible) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "q");
    elem.set_attribute("placeholder", "Search");
    EXPECT_TRUE(elem.has_attribute("type"));
    EXPECT_TRUE(elem.has_attribute("name"));
    EXPECT_TRUE(elem.has_attribute("placeholder"));
}

// Element: namespace uri empty for regular element
TEST(DomElement, NamespaceURIEmptyForRegularElement) {
    Element elem("div");
    EXPECT_TRUE(elem.namespace_uri().empty());
}

// Element: node_type is Element for any element
TEST(DomElement, NodeTypeIsElementForSpan) {
    Element elem("span");
    EXPECT_EQ(elem.node_type(), NodeType::Element);
}

// Element: tag_name matches constructor
TEST(DomElement, TagNameMatchesConstructorInput) {
    Element elem("section");
    EXPECT_EQ(elem.tag_name(), "section");
}

// ClassList: remove non-existent class is safe
TEST(DomClassList, RemoveNonExistentClassIsSafe) {
    Element elem("div");
    // Should not throw or crash
    elem.class_list().remove("nonexistent");
    EXPECT_FALSE(elem.class_list().contains("nonexistent"));
}

// ClassList: add same class twice keeps count at 1
TEST(DomClassList, AddSameClassTwiceKeepsCountOne) {
    Element elem("p");
    elem.class_list().add("visible");
    elem.class_list().add("visible");
    EXPECT_EQ(elem.class_list().items().size(), 1u);
}

// Comment: node_type is Comment for "note" comment
TEST(DomComment, NodeTypeIsCommentForNoteComment) {
    Comment c("note");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

// Document: node_type is Document for new document
TEST(DomDocument, NodeTypeIsDocumentForNewDoc) {
    Document doc;
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

// ============================================================================
// Cycle 684: More DOM tests
// ============================================================================

// Element: get_attribute for href returns link
TEST(DomElement, GetAttributeHrefReturnsLink) {
    Element elem("a");
    elem.set_attribute("href", "https://example.com");
    auto val = elem.get_attribute("href");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "https://example.com");
}

// Element: get_attribute for id returns id value
TEST(DomElement, GetAttributeIdReturnsIdValue) {
    Element elem("div");
    elem.set_attribute("id", "main-content");
    auto val = elem.get_attribute("id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "main-content");
}

// Element: has_attribute true for multiple attrs
TEST(DomElement, HasAttributeTrueForMultipleAttrs) {
    Element elem("input");
    elem.set_attribute("type", "email");
    elem.set_attribute("required", "");
    EXPECT_TRUE(elem.has_attribute("type"));
    EXPECT_TRUE(elem.has_attribute("required"));
}

// Element: first li is first child of ul
TEST(DomElement, FirstLiIsFirstChildOfUl) {
    Element parent("ul");
    auto first = std::make_unique<Element>("li");
    Element* first_ptr = first.get();
    parent.append_child(std::move(first));
    parent.append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent.first_child(), first_ptr);
}

// ClassList: items contains added classes
TEST(DomClassList, ItemsContainsAddedClasses) {
    Element elem("div");
    elem.class_list().add("foo");
    elem.class_list().add("bar");
    auto items = elem.class_list().items();
    bool has_foo = false, has_bar = false;
    for (auto& item : items) {
        if (item == "foo") has_foo = true;
        if (item == "bar") has_bar = true;
    }
    EXPECT_TRUE(has_foo);
    EXPECT_TRUE(has_bar);
}

// Text: set_data changes content
TEST(DomText, SetDataChangesContentDirectly) {
    Text t("original");
    t.set_data("modified");
    EXPECT_EQ(t.data(), "modified");
}

// Element: tag_name is main for main element
TEST(DomElement, TagNameIsMainForMainElement) {
    Element elem("main");
    EXPECT_EQ(elem.tag_name(), "main");
}

// Event: cancelable flag works
TEST(DomEvent, CancelableFlagWorks) {
    Event ev("click", true, true);
    EXPECT_TRUE(ev.cancelable());
}

// ---------------------------------------------------------------------------
// Cycle 689 — 8 additional DOM tests
// ---------------------------------------------------------------------------

// Element: attributes() vector has correct name field
TEST(DomElement, AttributeVectorFirstNameMatchesSet) {
    Element elem("a");
    elem.set_attribute("href", "https://example.com");
    ASSERT_EQ(elem.attributes().size(), 1u);
    EXPECT_EQ(elem.attributes()[0].name, "href");
}

// Element: attributes() vector has correct value field
TEST(DomElement, AttributeVectorFirstValueMatchesSet) {
    Element elem("a");
    elem.set_attribute("href", "https://example.com");
    ASSERT_EQ(elem.attributes().size(), 1u);
    EXPECT_EQ(elem.attributes()[0].value, "https://example.com");
}

// ClassList: length decreases after remove
TEST(DomClassList, LengthDecreasesAfterRemove) {
    Element elem("div");
    elem.class_list().add("foo");
    elem.class_list().add("bar");
    elem.class_list().remove("foo");
    EXPECT_EQ(elem.class_list().length(), 1u);
}

// Node: previous_sibling of third child is second child
TEST(DomNode, SiblingThreePreviousIsSecond) {
    Element parent("div");
    auto first  = std::make_unique<Element>("p");
    auto second = std::make_unique<Element>("p");
    auto third  = std::make_unique<Element>("p");
    Element* second_ptr = second.get();
    parent.append_child(std::move(first));
    parent.append_child(std::move(second));
    auto& third_ref = parent.append_child(std::move(third));
    EXPECT_EQ(third_ref.previous_sibling(), second_ptr);
}

// Node: next_sibling of first child is second child in three-child list
TEST(DomNode, SiblingThreeNextIsSecond) {
    Element parent("div");
    auto first  = std::make_unique<Element>("p");
    auto second = std::make_unique<Element>("p");
    Element* first_ptr  = first.get();
    Element* second_ptr = second.get();
    parent.append_child(std::move(first));
    parent.append_child(std::move(second));
    parent.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
}

// Node: child_count is two after insert_before on one-child parent
TEST(DomNode, ChildCountAfterInsertBeforeIsTwo) {
    Element parent("div");
    auto existing = std::make_unique<Element>("span");
    Element* existing_ptr = existing.get();
    parent.append_child(std::move(existing));
    parent.insert_before(std::make_unique<Element>("span"), existing_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
}

// Node: remove_child returns non-null unique_ptr (ownership transferred)
TEST(DomNode, RemoveChildReturnsOwnership) {
    Element parent("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();
    parent.append_child(std::move(child));
    auto removed = parent.remove_child(*child_ptr);
    EXPECT_NE(removed, nullptr);
}

// Node: mark_dirty with DirtyFlags::All sets all three flag bits
TEST(DomNode, MarkDirtyAllSetsAllFlags) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::All);
    auto flags = elem.dirty_flags();
    EXPECT_NE(flags & DirtyFlags::Style,  DirtyFlags::None);
    EXPECT_NE(flags & DirtyFlags::Layout, DirtyFlags::None);
    EXPECT_NE(flags & DirtyFlags::Paint,  DirtyFlags::None);
}

// ---------------------------------------------------------------------------
// Cycle 699 — 8 additional DOM tests
// ---------------------------------------------------------------------------

// Node: mark_dirty(Style) does NOT set Paint flag
TEST(DomNode, MarkDirtyStyleNotPaint) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Style);
    EXPECT_EQ(elem.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
}

// Element: attributes() vector second element has correct name
TEST(DomElement, AttributeVectorSecondNameMatchesSet) {
    Element elem("img");
    elem.set_attribute("src", "photo.jpg");
    elem.set_attribute("alt", "A photo");
    ASSERT_GE(elem.attributes().size(), 2u);
    EXPECT_EQ(elem.attributes()[1].name, "alt");
}

// Node: child_count is zero after removing all children
TEST(DomNode, ChildCountZeroAfterRemovingAllChildren) {
    Element parent("div");
    auto c1 = std::make_unique<Element>("p");
    auto c2 = std::make_unique<Element>("p");
    Element* c1_ptr = c1.get();
    Element* c2_ptr = c2.get();
    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    parent.remove_child(*c1_ptr);
    parent.remove_child(*c2_ptr);
    EXPECT_EQ(parent.child_count(), 0u);
}

// Node: parent is null after being removed from parent
TEST(DomNode, ParentNullAfterRemoveFromParent) {
    Element parent("div");
    auto child = std::make_unique<Element>("span");
    Element* child_ptr = child.get();
    parent.append_child(std::move(child));
    parent.remove_child(*child_ptr);
    EXPECT_EQ(child_ptr->parent(), nullptr);
}

// Document: register_id then get_element_by_id returns that element
TEST(DomDocument, DocumentRegisterIdAndRetrieve) {
    Document doc;
    auto elem = doc.create_element("div");
    Element* ptr = elem.get();
    doc.register_id("my-id", ptr);
    EXPECT_EQ(doc.get_element_by_id("my-id"), ptr);
}

// Document: unregister_id clears lookup
TEST(DomDocument, DocumentUnregisterIdClearsLookup) {
    Document doc;
    auto elem = doc.create_element("div");
    Element* ptr = elem.get();
    doc.register_id("some-id", ptr);
    doc.unregister_id("some-id");
    EXPECT_EQ(doc.get_element_by_id("some-id"), nullptr);
}

// Node: insert_before at front makes new node the first_child
TEST(DomNode, InsertBeforeFirstNodeBecomesFirstChild) {
    Element parent("div");
    auto orig = std::make_unique<Element>("p");
    Element* orig_ptr = orig.get();
    parent.append_child(std::move(orig));
    auto newnode = std::make_unique<Element>("h1");
    Element* new_ptr = newnode.get();
    parent.insert_before(std::move(newnode), orig_ptr);
    EXPECT_EQ(parent.first_child(), new_ptr);
}

// Node: three children can be traversed via next_sibling in order
TEST(DomNode, ThreeChildrenInOrderViaSiblings) {
    Element parent("ul");
    auto a = std::make_unique<Element>("li");
    auto b = std::make_unique<Element>("li");
    auto c = std::make_unique<Element>("li");
    Element* a_ptr = a.get();
    Element* b_ptr = b.get();
    Element* c_ptr = c.get();
    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->next_sibling(), nullptr);
}

TEST(DomNode, TextNodeHasNoChildren) {
    Text txt("hello");
    EXPECT_EQ(txt.child_count(), 0u);
}

TEST(DomNode, ElementTagNamePreservedOnCreate) {
    Element span("span");
    EXPECT_EQ(span.tag_name(), "span");
}

TEST(DomNode, HasAttributeReturnsFalseWhenAbsent) {
    Element div("div");
    EXPECT_FALSE(div.has_attribute("class"));
}

TEST(DomNode, HasAttributeReturnsTrueAfterSet) {
    Element div("div");
    div.set_attribute("class", "box");
    EXPECT_TRUE(div.has_attribute("class"));
}

TEST(DomNode, GetAttributeReturnsNulloptWhenAbsent) {
    Element img("img");
    EXPECT_FALSE(img.get_attribute("src").has_value());
}

TEST(DomNode, RemoveAttributeErasesIt) {
    Element p("p");
    p.set_attribute("id", "main");
    p.remove_attribute("id");
    EXPECT_FALSE(p.has_attribute("id"));
}

TEST(DomNode, FirstChildNullOnEmptyElement) {
    Element ul("ul");
    EXPECT_EQ(ul.first_child(), nullptr);
}

TEST(DomNode, LastChildNullOnEmptyElement) {
    Element ol("ol");
    EXPECT_EQ(ol.last_child(), nullptr);
}

TEST(DomNode, SetAttributeOverwritesPrevious) {
    Element div("div");
    div.set_attribute("id", "first");
    div.set_attribute("id", "second");
    EXPECT_EQ(div.get_attribute("id").value(), "second");
}

TEST(DomNode, TwoAttributesCount) {
    Element a("a");
    a.set_attribute("href", "https://example.com");
    a.set_attribute("target", "_blank");
    EXPECT_EQ(a.attributes().size(), 2u);
}

TEST(DomNode, AppendChildSetsParentPointer) {
    Element outer("div");
    auto inner = std::make_unique<Element>("span");
    Element* inner_ptr = inner.get();
    outer.append_child(std::move(inner));
    EXPECT_EQ(inner_ptr->parent(), &outer);
}

TEST(DomNode, NodeTypeElementIsElement) {
    Element em("em");
    EXPECT_EQ(em.node_type(), NodeType::Element);
}

TEST(DomNode, NodeTypeTextIsText) {
    Text t("hello");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

TEST(DomNode, ChildAtIndexZeroIsFirstChild) {
    Element ul("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    Element* li1_ptr = li1.get();
    ul.append_child(std::move(li1));
    ul.append_child(std::move(li2));
    EXPECT_EQ(ul.first_child(), li1_ptr);
}

TEST(DomNode, LastChildIsThirdAppended) {
    Element ul("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    Element* li2_ptr = li2.get();
    ul.append_child(std::move(li1));
    ul.append_child(std::move(li2));
    EXPECT_EQ(ul.last_child(), li2_ptr);
}

TEST(DomNode, TextContentOnSpanMatchesContent) {
    Element span("span");
    auto txt = std::make_unique<Text>("hello world");
    span.append_child(std::move(txt));
    EXPECT_NE(span.text_content().find("hello"), std::string::npos);
}

TEST(DomClassList, ToStringHasClass) {
    ClassList cl;
    cl.add("active");
    cl.add("visible");
    std::string str = cl.to_string();
    EXPECT_NE(str.find("active"), std::string::npos);
}









TEST(DomClassList, SupportsTwoItems) {
    ClassList cl;
    cl.add("alpha");
    cl.add("beta");
    EXPECT_EQ(cl.length(), 2u);
}









TEST(DomNode, ChildCountAfterTwoAppends) {
    Element div("div");
    div.append_child(std::make_unique<Element>("span"));
    div.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(div.child_count(), 2u);
}

TEST(DomNode, GrandchildAccessibleViaFirstChild) {
    Element outer("div");
    auto middle = std::make_unique<Element>("section");
    auto inner = std::make_unique<Element>("p");
    middle->append_child(std::move(inner));
    outer.append_child(std::move(middle));
    auto* section = outer.first_child();
    ASSERT_NE(section, nullptr);
    EXPECT_NE(section->first_child(), nullptr);
}

TEST(DomNode, PreviousSiblingNullForFirstChild) {
    Element parent("ul");
    auto li = std::make_unique<Element>("li");
    Element* li_ptr = li.get();
    parent.append_child(std::move(li));
    EXPECT_EQ(li_ptr->previous_sibling(), nullptr);
}

TEST(DomNode, NextSiblingNullForLastChild) {
    Element parent("ul");
    auto li = std::make_unique<Element>("li");
    Element* li_ptr = li.get();
    parent.append_child(std::move(li));
    EXPECT_EQ(li_ptr->next_sibling(), nullptr);
}

TEST(DomClassList, RemoveThenAddActsAsReplace) {
    ClassList cl;
    cl.add("old-class");
    cl.remove("old-class");
    cl.add("new-class");
    EXPECT_FALSE(cl.contains("old-class"));
    EXPECT_TRUE(cl.contains("new-class"));
}

TEST(DomClassList, RemoveBothReducesLengthToZero) {
    ClassList cl;
    cl.add("x");
    cl.add("y");
    cl.remove("x");
    cl.remove("y");
    EXPECT_EQ(cl.length(), 0u);
}

TEST(DomDocument, CreateElementReturnsCorrectTag) {
    Document doc;
    auto elem = doc.create_element("section");
    EXPECT_EQ(elem->tag_name(), "section");
}

TEST(DomDocument, CreateTextNodeHelloData) {
    Document doc;
    auto txt = doc.create_text_node("hello");
    EXPECT_EQ(txt->data(), "hello");
}

TEST(DomDocument, CreateCommentHasCorrectData) {
    Document doc;
    auto comment = doc.create_comment("TODO: fix this");
    EXPECT_EQ(comment->data(), "TODO: fix this");
}

TEST(DomNode, DeepTreeFourLevels) {
    Element level1("html");
    auto l2 = std::make_unique<Element>("body");
    auto l3 = std::make_unique<Element>("div");
    auto l4 = std::make_unique<Element>("p");
    Element* l4_ptr = l4.get();
    l3->append_child(std::move(l4));
    l2->append_child(std::move(l3));
    level1.append_child(std::move(l2));
    // l4 is at depth 3 from level1
    auto* body = level1.first_child();
    ASSERT_NE(body, nullptr);
    auto* div = body->first_child();
    ASSERT_NE(div, nullptr);
    EXPECT_EQ(div->first_child(), l4_ptr);
}

TEST(DomNode, MultipleChildrenPreserveOrder) {
    Element parent("ul");
    std::vector<Element*> ptrs;
    for (int i = 0; i < 5; ++i) {
        auto child = std::make_unique<Element>("li");
        ptrs.push_back(child.get());
        parent.append_child(std::move(child));
    }
    auto* cur = parent.first_child();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(cur, ptrs[i]);
        cur = cur->next_sibling();
    }
}

TEST(DomElement, SetAndGetMultipleAttributes) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    elem.set_attribute("placeholder", "Enter name");
    EXPECT_EQ(elem.get_attribute("type").value(), "text");
    EXPECT_EQ(elem.get_attribute("name").value(), "username");
    EXPECT_EQ(elem.get_attribute("placeholder").value(), "Enter name");
}

TEST(DomNode, TagNameIsLowercaseDiv) {
    Element div("div");
    EXPECT_EQ(div.tag_name(), "div");
}

TEST(DomNode, ChildCountAfterRemoveIsCorrect) {
    Element parent("div");
    auto c1 = std::make_unique<Element>("span");
    auto c2 = std::make_unique<Element>("p");
    Element* c2_ptr = c2.get();
    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    parent.remove_child(*c2_ptr);
    EXPECT_EQ(parent.child_count(), 1u);
}

TEST(DomNode, ClearDirtyResetsFlags) {
    Element div("div");
    div.mark_dirty(DirtyFlags::All);
    div.clear_dirty();
    EXPECT_EQ(div.dirty_flags(), DirtyFlags::None);
}

TEST(DomNode, MarkDirtyLayoutNotStyle) {
    Element p("p");
    p.mark_dirty(DirtyFlags::Layout);
    EXPECT_EQ(p.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
    EXPECT_NE(p.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

TEST(DomNode, MarkDirtyAllIncludesPaint) {
    Element span("span");
    span.mark_dirty(DirtyFlags::All);
    EXPECT_NE(span.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
}

TEST(DomNode, ForEachChildVisitsAllChildren) {
    Element parent("div");
    int count = 0;
    parent.append_child(std::make_unique<Element>("span"));
    parent.append_child(std::make_unique<Element>("p"));
    parent.append_child(std::make_unique<Element>("a"));
    parent.for_each_child([&](const Node& child) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(DomNode, InsertBeforeThreeChildren) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");
    Element* li1_ptr = li1.get();
    Element* li2_ptr = li2.get();
    Element* li3_ptr = li3.get();
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li3));
    parent.insert_before(std::move(li2), li3_ptr);
    EXPECT_EQ(parent.first_child(), li1_ptr);
    EXPECT_EQ(li1_ptr->next_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
}

TEST(DomNode, TextContentWithNestedText) {
    Element outer("div");
    auto inner = std::make_unique<Element>("p");
    inner->append_child(std::make_unique<Text>("inner text"));
    outer.append_child(std::move(inner));
    std::string content = outer.text_content();
    EXPECT_NE(content.find("inner"), std::string::npos);
}

TEST(DomNode, MarkDirtyStyleAndPaintCombined) {
    Element h1("h1");
    h1.mark_dirty(DirtyFlags::Style | DirtyFlags::Paint);
    EXPECT_NE(h1.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
    EXPECT_NE(h1.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
    EXPECT_EQ(h1.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

TEST(DomNode, RemoveChildReturnsAndOrphansNode) {
    Element parent("section");
    auto child = std::make_unique<Element>("div");
    Element* child_ptr = child.get();
    parent.append_child(std::move(child));
    auto removed = parent.remove_child(*child_ptr);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed.get(), child_ptr);
    EXPECT_EQ(parent.child_count(), 0u);
}

TEST(DomDocument, CreateElementSectionTag) {
    Document doc;
    auto elem = doc.create_element("article");
    EXPECT_EQ(elem->tag_name(), "article");
    EXPECT_EQ(elem->node_type(), NodeType::Element);
}

TEST(DomNode, EmptyTextContentForNewElement) {
    Element div("div");
    EXPECT_TRUE(div.text_content().empty());
}

TEST(DomNode, TextContentUpdatesAfterChildAdded) {
    Element div("div");
    auto txt = std::make_unique<Text>("changed");
    div.append_child(std::move(txt));
    EXPECT_NE(div.text_content().find("changed"), std::string::npos);
}

TEST(DomNode, ForEachChildLambdaReceivesTag) {
    Element parent("nav");
    parent.append_child(std::make_unique<Element>("a"));
    parent.append_child(std::make_unique<Element>("button"));
    std::vector<std::string> tags;
    parent.for_each_child([&](const Node& child) {
        if (child.node_type() == NodeType::Element) {
            tags.push_back(static_cast<const Element&>(child).tag_name());
        }
    });
    ASSERT_EQ(tags.size(), 2u);
    EXPECT_EQ(tags[0], "a");
    EXPECT_EQ(tags[1], "button");
}

TEST(DomNode, InsertBeforeNullReferenceAppends) {
    Element parent("div");
    auto child = std::make_unique<Element>("p");
    Element* p_ptr = child.get();
    parent.insert_before(std::move(child), nullptr);
    EXPECT_EQ(parent.first_child(), p_ptr);
}

TEST(DomNode, MarkDirtyPaintOnlyLayout) {
    Element div("div");
    div.mark_dirty(DirtyFlags::Paint);
    EXPECT_EQ(div.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
    EXPECT_NE(div.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
}

TEST(DomDocument, MultipleRegisteredIds) {
    Document doc;
    auto e1 = doc.create_element("div");
    auto e2 = doc.create_element("span");
    Element* p1 = e1.get();
    Element* p2 = e2.get();
    doc.register_id("first", p1);
    doc.register_id("second", p2);
    EXPECT_EQ(doc.get_element_by_id("first"), p1);
    EXPECT_EQ(doc.get_element_by_id("second"), p2);
}

TEST(DomNode, ChildrenCountAfterInsertBeforeMiddle) {
    Element parent("ol");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");
    Element* li3_ptr = li3.get();
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li3));
    parent.insert_before(std::move(li2), li3_ptr);
    EXPECT_EQ(parent.child_count(), 3u);
}

// Cycle 755 — Element attribute and ClassList edge cases
TEST(DomNode, AttributeNameAccessible) {
    Element el("div");
    el.set_attribute("role", "button");
    const auto& attrs = el.attributes();
    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0].name, "role");
}

TEST(DomNode, AttributeValueAccessible) {
    Element el("input");
    el.set_attribute("type", "checkbox");
    const auto& attrs = el.attributes();
    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0].value, "checkbox");
}

TEST(DomNode, ClassListItemsVectorNotEmpty) {
    Element el("div");
    el.class_list().add("foo");
    el.class_list().add("bar");
    EXPECT_EQ(el.class_list().items().size(), 2u);
}

TEST(DomNode, ClassListItemsContainsAdded) {
    Element el("span");
    el.class_list().add("active");
    const auto& items = el.class_list().items();
    EXPECT_NE(std::find(items.begin(), items.end(), "active"), items.end());
}

TEST(DomNode, NamespaceUriDefaultEmpty) {
    Element el("div");
    EXPECT_EQ(el.namespace_uri(), "");
}

TEST(DomNode, NamespaceUriCustom) {
    Element el("svg", "http://www.w3.org/2000/svg");
    EXPECT_EQ(el.namespace_uri(), "http://www.w3.org/2000/svg");
}

TEST(DomNode, IdAttributeSetsIdField) {
    Element el("div");
    el.set_attribute("id", "hero");
    EXPECT_EQ(el.id(), "hero");
}

TEST(DomNode, ThreeChildrenInsertBeforeOrderCorrect) {
    Element parent("ul");
    auto a = std::make_unique<Element>("li");
    auto b = std::make_unique<Element>("li");
    auto c = std::make_unique<Element>("li");
    a->set_attribute("id", "a");
    b->set_attribute("id", "b");
    c->set_attribute("id", "c");
    Element* c_ptr = c.get();
    parent.append_child(std::move(a));
    parent.append_child(std::move(c));
    parent.insert_before(std::move(b), c_ptr);
    // order: a, b, c — b was inserted before c
    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(dynamic_cast<const Element*>(parent.first_child())->id(), "a");
}

// Cycle 765 — Event phase and target accessor tests
TEST(DomEvent, EventPhaseInitiallyNone) {
    Event ev("click");
    EXPECT_EQ(ev.phase(), EventPhase::None);
}

TEST(DomEvent, EventTargetInitiallyNull) {
    Event ev("keydown");
    EXPECT_EQ(ev.target(), nullptr);
}

TEST(DomEvent, EventCurrentTargetInitiallyNull) {
    Event ev("mouseover");
    EXPECT_EQ(ev.current_target(), nullptr);
}

TEST(DomEvent, EventBubblesDefaultTrue) {
    Event ev("click");
    EXPECT_TRUE(ev.bubbles());
}

TEST(DomEvent, EventCancelableDefaultTrue) {
    Event ev("click");
    EXPECT_TRUE(ev.cancelable());
}

TEST(DomEvent, EventNonBubblingNonCancelable) {
    Event ev("focus", false, false);
    EXPECT_FALSE(ev.bubbles());
    EXPECT_FALSE(ev.cancelable());
}

TEST(DomEvent, PropagationNotStoppedInitially) {
    Event ev("input");
    EXPECT_FALSE(ev.propagation_stopped());
}

TEST(DomEvent, ImmediatePropagationNotStoppedInitially) {
    Event ev("change");
    EXPECT_FALSE(ev.immediate_propagation_stopped());
}

// Cycle 773 — Document API edge cases
TEST(DomDocument, DocumentBodyNullInitially) {
    Document doc;
    EXPECT_EQ(doc.body(), nullptr);
}

TEST(DomDocument, DocumentHeadNullInitially) {
    Document doc;
    EXPECT_EQ(doc.head(), nullptr);
}

TEST(DomDocument, DocumentElementNullInitially) {
    Document doc;
    EXPECT_EQ(doc.document_element(), nullptr);
}

TEST(DomDocument, RegisterMultipleIdsDistinct) {
    Document doc;
    auto e1 = doc.create_element("div");
    auto e2 = doc.create_element("span");
    Element* p1 = e1.get();
    Element* p2 = e2.get();
    doc.register_id("x", p1);
    doc.register_id("y", p2);
    EXPECT_EQ(doc.get_element_by_id("x"), p1);
    EXPECT_EQ(doc.get_element_by_id("y"), p2);
}

TEST(DomDocument, UnregisterKeepsOtherIds) {
    Document doc;
    auto e1 = doc.create_element("p");
    auto e2 = doc.create_element("h1");
    doc.register_id("keep", e1.get());
    doc.register_id("remove", e2.get());
    doc.unregister_id("remove");
    EXPECT_NE(doc.get_element_by_id("keep"), nullptr);
    EXPECT_EQ(doc.get_element_by_id("remove"), nullptr);
}

TEST(DomDocument, CreateTwoElementsDifferentTags) {
    Document doc;
    auto div = doc.create_element("div");
    auto span = doc.create_element("span");
    EXPECT_EQ(div->tag_name(), "div");
    EXPECT_EQ(span->tag_name(), "span");
}

TEST(DomDocument, CreateCommentDataStored) {
    Document doc;
    auto comment = doc.create_comment("hello comment");
    EXPECT_EQ(comment->data(), "hello comment");
}

TEST(DomDocument, DocumentNodeTypeIsDocument) {
    Document doc;
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

// Cycle 780 — Text and Comment node accessor tests
TEST(DomText, TextNodeSetDataChanges) {
    Text t("initial");
    t.set_data("updated");
    EXPECT_EQ(t.data(), "updated");
}

TEST(DomText, TextNodeTextContentMatchesData) {
    Text t("hello world");
    EXPECT_EQ(t.text_content(), "hello world");
}

TEST(DomText, TextNodeNodeTypeIsText) {
    Text t("abc");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

TEST(DomText, TextNodeInitialDataEmpty) {
    Text t("");
    EXPECT_TRUE(t.data().empty());
}

TEST(DomText, TextNodeChildCountZero) {
    Text t("no children");
    EXPECT_EQ(t.child_count(), 0u);
}

TEST(DomComment, CommentNodeTypeIsCommentV2) {
    Comment c("a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

TEST(DomComment, CommentSetDataUpdates) {
    Comment c("old");
    c.set_data("new content");
    EXPECT_EQ(c.data(), "new content");
}

TEST(DomComment, CommentTextContentIsEmpty) {
    Comment c("ignored in layout");
    // comment text content should be empty (not exposed to layout)
    EXPECT_TRUE(c.text_content().empty());
}

TEST(DomClassList, ClassListRemoveReducesLength) {
    ClassList cl;
    cl.add("foo");
    cl.add("bar");
    cl.remove("foo");
    EXPECT_EQ(cl.length(), 1u);
}

TEST(DomClassList, ClassListRemoveContainsFalse) {
    ClassList cl;
    cl.add("active");
    cl.remove("active");
    EXPECT_FALSE(cl.contains("active"));
}

TEST(DomClassList, ClassListToggleAdds) {
    ClassList cl;
    cl.toggle("open");
    EXPECT_TRUE(cl.contains("open"));
}

TEST(DomClassList, ClassListToggleRemoves) {
    ClassList cl;
    cl.add("open");
    cl.toggle("open");
    EXPECT_FALSE(cl.contains("open"));
}

TEST(DomClassList, ClassListLengthAfterThreeAdds) {
    ClassList cl;
    cl.add("a");
    cl.add("b");
    cl.add("c");
    EXPECT_EQ(cl.length(), 3u);
}

TEST(DomClassList, ClassListEmptyInitially) {
    ClassList cl;
    EXPECT_EQ(cl.length(), 0u);
}

TEST(DomClassList, ClassListAddDuplicateNoGrow) {
    ClassList cl;
    cl.add("x");
    cl.add("x");
    EXPECT_EQ(cl.length(), 1u);
}

TEST(DomClassList, ClassListContainsReturnsFalseEmpty) {
    ClassList cl;
    EXPECT_FALSE(cl.contains("anything"));
}

TEST(DomNode, TextContentIncludesChildText) {
    Document doc;
    auto elem = doc.create_element("p");
    auto txt = doc.create_text_node("hello");
    elem->append_child(std::move(txt));
    EXPECT_EQ(elem->text_content(), "hello");
}

TEST(DomNode, MultiLevelTreeParentIsCorrect) {
    Document doc;
    auto root = doc.create_element("div");
    auto child = doc.create_element("span");
    auto grandchild = doc.create_element("em");
    child->append_child(std::move(grandchild));
    Node* gc = child->first_child();
    root->append_child(std::move(child));
    ASSERT_NE(gc, nullptr);
    ASSERT_NE(gc->parent(), nullptr);
    EXPECT_EQ(gc->parent()->parent(), root.get());
}

TEST(DomNode, ForEachChildCountsCorrectly) {
    Document doc;
    auto elem = doc.create_element("ul");
    elem->append_child(doc.create_element("li"));
    elem->append_child(doc.create_element("li"));
    elem->append_child(doc.create_element("li"));
    int count = 0;
    elem->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(DomNode, RemoveMiddleChildLeavesOthers) {
    Document doc;
    auto parent = doc.create_element("div");
    auto& c1 = parent->append_child(doc.create_element("a"));
    auto& c2 = parent->append_child(doc.create_element("b"));
    auto& c3 = parent->append_child(doc.create_element("c"));
    (void)c3;
    parent->remove_child(c2);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(c1.next_sibling(), parent->last_child());
}

TEST(DomNode, AppendAfterRemoveRestoresChild) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("span"));
    auto removed = parent->remove_child(*parent->first_child());
    EXPECT_EQ(parent->child_count(), 0u);
    parent->append_child(std::move(removed));
    EXPECT_EQ(parent->child_count(), 1u);
}

TEST(DomNode, TextContentConcatenatesMultipleTexts) {
    Document doc;
    auto elem = doc.create_element("p");
    elem->append_child(doc.create_text_node("foo"));
    elem->append_child(doc.create_text_node("bar"));
    EXPECT_EQ(elem->text_content(), "foobar");
}

TEST(DomNode, SiblingTraversalAllThree) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("a"));
    parent->append_child(doc.create_element("b"));
    parent->append_child(doc.create_element("c"));
    Node* cur = parent->first_child();
    int count = 0;
    while (cur) { ++count; cur = cur->next_sibling(); }
    EXPECT_EQ(count, 3);
}

TEST(DomNode, InsertBeforeNullAppendsAtEnd) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("first"));
    parent->insert_before(doc.create_element("last"), nullptr);
    EXPECT_EQ(parent->child_count(), 2u);
}

TEST(DomEventTarget, AddListenerCalledOnDispatch) {
    Document doc;
    auto elem = doc.create_element("button");
    int called = 0;
    EventTarget target;
    target.add_event_listener("click", [&](Event&) { ++called; });
    Event ev("click");
    target.dispatch_event(ev, *elem);
    EXPECT_EQ(called, 1);
}

TEST(DomEventTarget, TwoListenersBothCalled) {
    Document doc;
    auto elem = doc.create_element("div");
    int count = 0;
    EventTarget target;
    target.add_event_listener("input", [&](Event&) { ++count; });
    target.add_event_listener("input", [&](Event&) { ++count; });
    Event ev("input");
    target.dispatch_event(ev, *elem);
    EXPECT_EQ(count, 2);
}

TEST(DomEventTarget, WrongEventTypeNotCalled) {
    Document doc;
    auto elem = doc.create_element("span");
    int count = 0;
    EventTarget target;
    target.add_event_listener("click", [&](Event&) { ++count; });
    Event ev("mouseover");
    target.dispatch_event(ev, *elem);
    EXPECT_EQ(count, 0);
}

TEST(DomEventTarget, RemoveAllListenersPreventsCall) {
    Document doc;
    auto elem = doc.create_element("p");
    int count = 0;
    EventTarget target;
    target.add_event_listener("focus", [&](Event&) { ++count; });
    target.remove_all_listeners("focus");
    Event ev("focus");
    target.dispatch_event(ev, *elem);
    EXPECT_EQ(count, 0);
}

TEST(DomEventTarget, DispatchTwiceCallsTwice) {
    Document doc;
    auto elem = doc.create_element("div");
    int count = 0;
    EventTarget target;
    target.add_event_listener("change", [&](Event&) { ++count; });
    Event ev("change");
    target.dispatch_event(ev, *elem);
    target.dispatch_event(ev, *elem);
    EXPECT_EQ(count, 2);
}

TEST(DomEventTarget, ListenerReceivesCorrectEvent) {
    Document doc;
    auto elem = doc.create_element("input");
    std::string captured_type;
    EventTarget target;
    target.add_event_listener("keyup", [&](Event& e) { captured_type = e.type(); });
    Event ev("keyup");
    target.dispatch_event(ev, *elem);
    EXPECT_EQ(captured_type, "keyup");
}

TEST(DomEventTarget, ThreeListenersDifferentTypes) {
    Document doc;
    auto elem = doc.create_element("div");
    int clicks = 0, keys = 0;
    EventTarget target;
    target.add_event_listener("click", [&](Event&) { ++clicks; });
    target.add_event_listener("keydown", [&](Event&) { ++keys; });
    Event e1("click"), e2("keydown");
    target.dispatch_event(e1, *elem);
    target.dispatch_event(e2, *elem);
    EXPECT_EQ(clicks, 1);
    EXPECT_EQ(keys, 1);
}

TEST(DomEventTarget, DispatchReturnsTrue) {
    Document doc;
    auto elem = doc.create_element("div");
    EventTarget target;
    target.add_event_listener("click", [](Event&) {});
    Event ev("click");
    bool result = target.dispatch_event(ev, *elem);
    EXPECT_TRUE(result);
}

TEST(DomElement, SetDataAttribute) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->set_attribute("data-id", "42");
    auto val = elem->get_attribute("data-id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "42");
}

TEST(DomElement, SetMultipleDataAttributes) {
    Document doc;
    auto elem = doc.create_element("article");
    elem->set_attribute("data-author", "Alice");
    elem->set_attribute("data-category", "tech");
    EXPECT_EQ(*elem->get_attribute("data-author"), "Alice");
    EXPECT_EQ(*elem->get_attribute("data-category"), "tech");
}

TEST(DomElement, DataAttributeHasAttributeTrue) {
    Document doc;
    auto elem = doc.create_element("span");
    elem->set_attribute("data-visible", "true");
    EXPECT_TRUE(elem->has_attribute("data-visible"));
}

TEST(DomElement, RemoveDataAttribute) {
    Document doc;
    auto elem = doc.create_element("p");
    elem->set_attribute("data-temp", "123");
    elem->remove_attribute("data-temp");
    EXPECT_FALSE(elem->has_attribute("data-temp"));
}

TEST(DomElement, DataAttributeOverwrite) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->set_attribute("data-count", "1");
    elem->set_attribute("data-count", "2");
    EXPECT_EQ(*elem->get_attribute("data-count"), "2");
}

TEST(DomElement, DataAttributeEmptyValue) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->set_attribute("data-flag", "");
    auto val = elem->get_attribute("data-flag");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "");
}

TEST(DomElement, ThreeDataAttributesAllPresent) {
    Document doc;
    auto elem = doc.create_element("li");
    elem->set_attribute("data-a", "1");
    elem->set_attribute("data-b", "2");
    elem->set_attribute("data-c", "3");
    EXPECT_TRUE(elem->has_attribute("data-a"));
    EXPECT_TRUE(elem->has_attribute("data-b"));
    EXPECT_TRUE(elem->has_attribute("data-c"));
}

TEST(DomElement, DataAttributeInAttributesList) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->set_attribute("data-role", "button");
    bool found = false;
    for (const auto& attr : elem->attributes())
        if (attr.name == "data-role") found = true;
    EXPECT_TRUE(found);
}

// Cycle 821 — ClassList::to_string(), Comment in tree, Text node edge cases
TEST(DomClassList, ToStringEmptyIsEmpty) {
    ClassList cl;
    EXPECT_EQ(cl.to_string(), "");
}

TEST(DomClassList, ToStringSingleClass) {
    ClassList cl;
    cl.add("foo");
    EXPECT_EQ(cl.to_string(), "foo");
}

TEST(DomClassList, ToStringTwoClassesSpaceSeparated) {
    ClassList cl;
    cl.add("foo");
    cl.add("bar");
    std::string s = cl.to_string();
    EXPECT_NE(s.find("foo"), std::string::npos);
    EXPECT_NE(s.find("bar"), std::string::npos);
}

TEST(DomClassList, ToStringAfterRemoveDropsClass) {
    ClassList cl;
    cl.add("alpha");
    cl.add("beta");
    cl.remove("alpha");
    EXPECT_EQ(cl.to_string().find("alpha"), std::string::npos);
    EXPECT_NE(cl.to_string().find("beta"), std::string::npos);
}

TEST(DomComment, AppendedToParentHasCorrectParent) {
    Element parent("div");
    auto comment = std::make_unique<Comment>("a note");
    auto* ptr = comment.get();
    parent.append_child(std::move(comment));
    EXPECT_EQ(ptr->parent(), &parent);
}

TEST(DomComment, AppendedCommentIncreasesChildCount) {
    Element parent("section");
    parent.append_child(std::make_unique<Comment>("note1"));
    parent.append_child(std::make_unique<Comment>("note2"));
    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DomText, EmptyTextNodeDataIsEmpty) {
    Text t("");
    EXPECT_EQ(t.data(), "");
    EXPECT_EQ(t.text_content(), "");
}

TEST(DomText, TextContentEqualsData) {
    Text t("hello world");
    EXPECT_EQ(t.text_content(), t.data());
}

// Cycle 830 — Document: create/append, getElementById after unregister, id from attribute, child count
TEST(DomDocument, CreateElementAppendToDocChildCount) {
    Document doc;
    auto elem = doc.create_element("section");
    doc.append_child(std::move(elem));
    EXPECT_EQ(doc.child_count(), 1u);
}

TEST(DomDocument, CreateTextNodeAppendedChildCount) {
    Document doc;
    auto txt = doc.create_text_node("Hello");
    doc.append_child(std::move(txt));
    EXPECT_EQ(doc.child_count(), 1u);
}

TEST(DomDocument, CreateCommentAppendedFirstChild) {
    Document doc;
    auto c = doc.create_comment("copyright 2026");
    auto* ptr = c.get();
    doc.append_child(std::move(c));
    EXPECT_EQ(doc.first_child(), ptr);
}

TEST(DomDocument, GetElementByIdAfterUnregisterReturnsNull) {
    Document doc;
    auto elem = doc.create_element("div");
    auto* ptr = elem.get();
    doc.register_id("main", ptr);
    doc.unregister_id("main");
    EXPECT_EQ(doc.get_element_by_id("main"), nullptr);
}

TEST(DomDocument, RegisterTwoIdsRetrieval) {
    Document doc;
    auto a = doc.create_element("div");
    auto b = doc.create_element("span");
    auto* aptr = a.get();
    auto* bptr = b.get();
    doc.register_id("alpha", aptr);
    doc.register_id("beta", bptr);
    EXPECT_EQ(doc.get_element_by_id("alpha"), aptr);
    EXPECT_EQ(doc.get_element_by_id("beta"), bptr);
}

TEST(DomDocument, GetElementByIdViaSetAttribute) {
    Document doc;
    auto elem = doc.create_element("input");
    elem->set_attribute("id", "email-field");
    auto* ptr = elem.get();
    doc.register_id("email-field", ptr);
    EXPECT_EQ(doc.get_element_by_id("email-field"), ptr);
}

TEST(DomDocument, CreateMultipleChildrenCount) {
    Document doc;
    doc.append_child(doc.create_element("div"));
    doc.append_child(doc.create_element("p"));
    doc.append_child(doc.create_text_node("text"));
    EXPECT_EQ(doc.child_count(), 3u);
}

TEST(DomDocument, GetElementByIdMissingKeyReturnsNull) {
    Document doc;
    EXPECT_EQ(doc.get_element_by_id("nonexistent"), nullptr);
}

// ---------------------------------------------------------------------------
// dispatch_event_to_tree tests (Cycle 841)
// ---------------------------------------------------------------------------
TEST(DomDispatchTree, OrphanNodeTargetIsSet) {
    auto elem = std::make_unique<Element>("div");
    Element* ptr = elem.get();
    Event event("click");
    dispatch_event_to_tree(event, *ptr);
    EXPECT_EQ(event.target(), ptr);
}

TEST(DomDispatchTree, OrphanNodePhaseIsNoneAfterDispatch) {
    auto elem = std::make_unique<Element>("div");
    Event event("click");
    dispatch_event_to_tree(event, *elem);
    EXPECT_EQ(event.phase(), EventPhase::None);
}

TEST(DomDispatchTree, OrphanNodeCurrentTargetNullAfterDispatch) {
    auto elem = std::make_unique<Element>("div");
    Event event("click");
    dispatch_event_to_tree(event, *elem);
    EXPECT_EQ(event.current_target(), nullptr);
}

TEST(DomDispatchTree, ChildTargetIsChild) {
    Element parent("div");
    auto child_ptr = std::make_unique<Element>("span");
    Element* child = child_ptr.get();
    parent.append_child(std::move(child_ptr));
    Event event("click");
    dispatch_event_to_tree(event, *child);
    EXPECT_EQ(event.target(), child);
}

TEST(DomDispatchTree, ChildTargetNotParent) {
    Element parent("div");
    auto child_ptr = std::make_unique<Element>("span");
    Element* child = child_ptr.get();
    parent.append_child(std::move(child_ptr));
    Event event("mouseover");
    dispatch_event_to_tree(event, *child);
    EXPECT_NE(event.target(), &parent);
}

TEST(DomDispatchTree, GrandchildTargetIsGrandchild) {
    Element root("div");
    auto child_ptr = std::make_unique<Element>("section");
    auto grand_ptr = std::make_unique<Element>("p");
    Element* grandchild = grand_ptr.get();
    child_ptr->append_child(std::move(grand_ptr));
    root.append_child(std::move(child_ptr));
    Event event("focus");
    dispatch_event_to_tree(event, *grandchild);
    EXPECT_EQ(event.target(), grandchild);
}

TEST(DomDispatchTree, NonBubblingTargetIsSet) {
    auto elem = std::make_unique<Element>("input");
    Element* ptr = elem.get();
    Event event("change", /*bubbles=*/false);
    dispatch_event_to_tree(event, *ptr);
    EXPECT_EQ(event.target(), ptr);
}

TEST(DomDispatchTree, DispatchTwiceSecondTargetUpdates) {
    Element elem("button");
    Event event1("click");
    Event event2("keypress");
    dispatch_event_to_tree(event1, elem);
    dispatch_event_to_tree(event2, elem);
    EXPECT_EQ(event2.target(), &elem);
}

TEST(DomDispatchTree, TargetNullBeforeDispatch) {
    Event event("focus");
    EXPECT_EQ(event.target(), nullptr);
}

TEST(DomDispatchTree, PhaseNoneBeforeDispatch) {
    Event event("blur");
    EXPECT_EQ(event.phase(), EventPhase::None);
}

TEST(DomDispatchTree, EventTypePreservedAfterDispatch) {
    Element elem("span");
    Event event("input");
    dispatch_event_to_tree(event, elem);
    EXPECT_EQ(event.type(), "input");
}

TEST(DomDispatchTree, CurrentTargetNullInitially) {
    Event event("keydown");
    EXPECT_EQ(event.current_target(), nullptr);
}

TEST(DomDispatchTree, BubblesPreservedAfterDispatch) {
    Element elem("div");
    Event event("scroll", /*bubbles=*/true);
    dispatch_event_to_tree(event, elem);
    EXPECT_TRUE(event.bubbles());
}

TEST(DomDispatchTree, NonBubblingPreservedAfterDispatch) {
    Element elem("div");
    Event event("resize", /*bubbles=*/false);
    dispatch_event_to_tree(event, elem);
    EXPECT_FALSE(event.bubbles());
}

TEST(DomDispatchTree, DispatchToSiblingSetsSiblingTarget) {
    Element parent("ul");
    auto li1_ptr = std::make_unique<Element>("li");
    auto li2_ptr = std::make_unique<Element>("li");
    Element* li2 = li2_ptr.get();
    parent.append_child(std::move(li1_ptr));
    parent.append_child(std::move(li2_ptr));
    Event event("click");
    dispatch_event_to_tree(event, *li2);
    EXPECT_EQ(event.target(), li2);
}

TEST(DomDispatchTree, CancelablePreservedAfterDispatch) {
    Element elem("button");
    Event event("click", /*bubbles=*/true, /*cancelable=*/true);
    dispatch_event_to_tree(event, elem);
    EXPECT_TRUE(event.cancelable());
}

// Cycle 859 — DomNode traversal edge cases
TEST(DomNode, ForEachChildCountsAllChildren) {
    Element parent("ul");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    int count = 0;
    parent.for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(DomNode, ForEachChildEmptyNeverCalled) {
    Element elem("div");
    int count = 0;
    elem.for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 0);
}

TEST(DomNode, FirstChildPrevSiblingIsNull) {
    Element parent("div");
    auto child_ptr = std::make_unique<Element>("span");
    parent.append_child(std::move(child_ptr));
    EXPECT_EQ(parent.first_child()->previous_sibling(), nullptr);
}

TEST(DomNode, LastChildNextSiblingIsNull) {
    Element parent("div");
    parent.append_child(std::make_unique<Element>("span"));
    parent.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(parent.last_child()->next_sibling(), nullptr);
}

TEST(DomNode, TraverseAllChildrenViaNextSibling) {
    Element parent("ol");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    int count = 0;
    for (Node* n = parent.first_child(); n != nullptr; n = n->next_sibling()) {
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST(DomNode, TraverseBackwardsViaPreviousSibling) {
    Element parent("nav");
    parent.append_child(std::make_unique<Element>("a"));
    parent.append_child(std::make_unique<Element>("a"));
    parent.append_child(std::make_unique<Element>("a"));
    int count = 0;
    for (Node* n = parent.last_child(); n != nullptr; n = n->previous_sibling()) {
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST(DomNode, ChildCountAfterRemoveIsOne) {
    Element parent("div");
    auto child1_ptr = std::make_unique<Element>("span");
    Element* child1_raw = child1_ptr.get();
    parent.append_child(std::make_unique<Element>("p"));
    parent.append_child(std::move(child1_ptr));
    EXPECT_EQ(parent.child_count(), 2u);
    parent.remove_child(*child1_raw);
    EXPECT_EQ(parent.child_count(), 1u);
}

TEST(DomNode, TextNodeSiblingOfElement) {
    Element parent("p");
    auto text = std::make_unique<Text>("Hello");
    Text* text_ptr = text.get();
    parent.append_child(std::make_unique<Element>("em"));
    parent.append_child(std::move(text));
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.last_child(), text_ptr);
}


// Cycle 869 — Element attribute/classList/textContent/nodeType operations
TEST(DomElement, GetAttributeAfterOverwrite) {
    Element elem("a");
    elem.set_attribute("href", "http://example.com");
    elem.set_attribute("href", "http://other.com");
    auto val = elem.get_attribute("href");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "http://other.com");
}

TEST(DomElement, SetThreeAttributesAccessibleByName) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    elem.set_attribute("placeholder", "Enter name");
    auto ph = elem.get_attribute("placeholder");
    ASSERT_TRUE(ph.has_value());
    EXPECT_EQ(ph.value(), "Enter name");
}

TEST(DomElement, HasAttributeAfterRemoval) {
    Element elem("div");
    elem.set_attribute("hidden", "");
    elem.remove_attribute("hidden");
    EXPECT_FALSE(elem.has_attribute("hidden"));
}

TEST(DomElement, ClassListContainsAfterToggle) {
    Element elem("li");
    elem.class_list().add("selected");
    elem.class_list().toggle("selected");
    EXPECT_FALSE(elem.class_list().contains("selected"));
}

TEST(DomElement, ClassListAddTwiceSameClass) {
    Element elem("span");
    elem.class_list().add("foo");
    elem.class_list().add("foo");
    EXPECT_TRUE(elem.class_list().contains("foo"));
}

TEST(DomElement, TextContentOfElementWithText) {
    Element elem("p");
    auto text = std::make_unique<Text>("Hello World");
    elem.append_child(std::move(text));
    EXPECT_EQ(elem.text_content(), "Hello World");
}

TEST(DomElement, ElementNodeTypeIsElement) {
    Element elem("div");
    EXPECT_EQ(elem.node_type(), NodeType::Element);
}

TEST(DomText, TextNodeTypeIsText) {
    Text t("content");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}


// Cycle 878 — Node parent pointer, dirty flags, Document create, Comment content
TEST(DomNode, ParentSetAfterInsertBefore) {
    Element parent("div");
    auto ref_ptr = std::make_unique<Element>("span");
    Element* ref = ref_ptr.get();
    parent.append_child(std::move(ref_ptr));
    auto new_ptr = std::make_unique<Element>("p");
    Element* newNode = new_ptr.get();
    parent.insert_before(std::move(new_ptr), ref);
    EXPECT_EQ(newNode->parent(), &parent);
}

TEST(DomNode, TwoChildrenAddedInOrder) {
    Element parent("div");
    parent.append_child(std::make_unique<Element>("h1"));
    parent.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_NE(parent.first_child(), parent.last_child());
}

TEST(DomNode, MarkDirtyLayoutSetsLayoutFlag) {
    Element e("div");
    e.mark_dirty(DirtyFlags::Layout);
    EXPECT_NE(e.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

TEST(DomNode, MarkDirtyAllSetsAllFlagsV2) {
    Element e("section");
    e.mark_dirty(DirtyFlags::All);
    EXPECT_NE(e.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
    EXPECT_NE(e.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
    EXPECT_NE(e.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

TEST(DomDocument, CreateCommentIsCommentType) {
    Document doc;
    auto comment = doc.create_comment("test comment");
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->node_type(), NodeType::Comment);
}

TEST(DomComment, CommentDataIsPreserved) {
    Comment c("my comment data");
    EXPECT_EQ(c.data(), "my comment data");
}

TEST(DomDocument, CreateTextReturnsTextNode) {
    Document doc;
    auto text = doc.create_text_node("world");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->node_type(), NodeType::Text);
}

TEST(DomNode, ForEachChildVisitsInOrder) {
    Element parent("ol");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    std::vector<const Node*> visited;
    parent.for_each_child([&](const Node& n) { visited.push_back(&n); });
    EXPECT_EQ(visited.size(), 3u);
    EXPECT_EQ(visited[0], parent.first_child());
    EXPECT_EQ(visited[2], parent.last_child());
}
