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
