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
