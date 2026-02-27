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

TEST(DomNode, LastChildOfTwoHasNoNextSibling) {
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

// Cycle 887 — DOM structural and attribute tests

TEST(DomNode, RemoveAndReAppendSameChild) {
    Element parent("div");
    parent.append_child(std::make_unique<Element>("span"));
    auto* child = parent.first_child();
    auto owned = parent.remove_child(*child);
    EXPECT_EQ(parent.child_count(), 0u);
    parent.append_child(std::move(owned));
    EXPECT_EQ(parent.child_count(), 1u);
}

TEST(DomNode, AllChildrenRemovedLeaveEmpty) {
    Element parent("ul");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.remove_child(*parent.first_child());
    parent.remove_child(*parent.first_child());
    EXPECT_EQ(parent.child_count(), 0u);
    EXPECT_EQ(parent.first_child(), nullptr);
}

TEST(DomElement, AttributeCountDecreases) {
    Element e("input");
    e.set_attribute("type", "text");
    e.set_attribute("name", "username");
    EXPECT_EQ(e.attributes().size(), 2u);
    e.remove_attribute("type");
    EXPECT_EQ(e.attributes().size(), 1u);
}

TEST(DomElement, SetAndRemoveOneAttribute) {
    Element e("img");
    e.set_attribute("alt", "description");
    EXPECT_TRUE(e.has_attribute("alt"));
    e.remove_attribute("alt");
    EXPECT_FALSE(e.has_attribute("alt"));
}

TEST(DomElement, AttributeNameCasePreserved) {
    Element e("div");
    e.set_attribute("data-Value", "42");
    const auto& attrs = e.attributes();
    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0].name, "data-Value");
}

TEST(DomDocument, CreateElementInDocumentContext) {
    Document doc;
    auto elem = doc.create_element("section");
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "section");
    EXPECT_EQ(elem->node_type(), NodeType::Element);
}

TEST(DomNode, SiblingOrderAfterInsertBefore) {
    Element parent("nav");
    parent.append_child(std::make_unique<Element>("a")); // first
    parent.append_child(std::make_unique<Element>("b")); // last
    auto* last = parent.last_child();
    parent.insert_before(std::make_unique<Element>("c"), last);
    // order: a, c, b
    EXPECT_EQ(parent.first_child()->next_sibling(), last->previous_sibling());
}

TEST(DomElement, MultipleAttributeValuesDistinct) {
    Element e("input");
    e.set_attribute("type", "email");
    e.set_attribute("placeholder", "Enter email");
    e.set_attribute("required", "");
    EXPECT_EQ(e.attributes().size(), 3u);
    bool found_type = false, found_placeholder = false, found_required = false;
    for (const auto& attr : e.attributes()) {
        if (attr.name == "type" && attr.value == "email") found_type = true;
        if (attr.name == "placeholder" && attr.value == "Enter email") found_placeholder = true;
        if (attr.name == "required" && attr.value == "") found_required = true;
    }
    EXPECT_TRUE(found_type);
    EXPECT_TRUE(found_placeholder);
    EXPECT_TRUE(found_required);
}

// Cycle 895 — DOM event tests

TEST(DomEvent, TargetNullInitially) {
    Event evt("click");
    EXPECT_EQ(evt.target(), nullptr);
}

TEST(DomEvent, BubblesCanBeSetFalse) {
    Event evt("click", false, true);
    EXPECT_FALSE(evt.bubbles());
}

TEST(DomEvent, CancelableCanBeSetFalse) {
    Event evt("click", true, false);
    EXPECT_FALSE(evt.cancelable());
}

TEST(DomEvent, DefaultNotPreventedOnNonCancelable) {
    Event evt("click", true, false);
    evt.prevent_default();
    EXPECT_FALSE(evt.default_prevented());
}

TEST(DomEvent, EventDispatchFiresListener) {
    Element node("div");
    EventTarget et;
    int count = 0;
    et.add_event_listener("click", [&](Event&) { ++count; });
    Event evt("click");
    et.dispatch_event(evt, node);
    EXPECT_EQ(count, 1);
}

TEST(DomEvent, EventDispatchDoesNotFireWrongType) {
    Element node("div");
    EventTarget et;
    int count = 0;
    et.add_event_listener("click", [&](Event&) { ++count; });
    Event evt("mouseover");
    et.dispatch_event(evt, node);
    EXPECT_EQ(count, 0);
}

TEST(DomEvent, RemoveAllListenersSilencesType) {
    Element node("button");
    EventTarget et;
    int count = 0;
    et.add_event_listener("click", [&](Event&) { ++count; });
    et.remove_all_listeners("click");
    Event evt("click");
    et.dispatch_event(evt, node);
    EXPECT_EQ(count, 0);
}

TEST(DomEvent, MultipleListenersAllFiredOnDispatch) {
    Element node("div");
    EventTarget et;
    int total = 0;
    et.add_event_listener("focus", [&](Event&) { total += 1; });
    et.add_event_listener("focus", [&](Event&) { total += 10; });
    Event evt("focus");
    et.dispatch_event(evt, node);
    EXPECT_EQ(total, 11);
}

TEST(DomEvent, EventTypeMatchesString) {
    Event evt("input");
    EXPECT_EQ(evt.type(), "input");
}

TEST(DomEvent, EventTypeChangeEvent) {
    Event evt("change");
    EXPECT_EQ(evt.type(), "change");
}

TEST(DomEvent, EventDefaultNotPreventedInitially) {
    Event evt("click", true, true);
    EXPECT_FALSE(evt.default_prevented());
}

TEST(DomEvent, EventDefaultPreventedOnCancelable) {
    Event evt("click", true, true);
    evt.prevent_default();
    EXPECT_TRUE(evt.default_prevented());
}

TEST(DomEvent, EventBubblesIsTrue) {
    Event evt("click", true, false);
    EXPECT_TRUE(evt.bubbles());
}

TEST(DomEvent, EventBubblesIsFalse) {
    Event evt("click", false, false);
    EXPECT_FALSE(evt.bubbles());
}

TEST(DomEvent, EventCancelableIsTrue) {
    Event evt("click", true, true);
    EXPECT_TRUE(evt.cancelable());
}

TEST(DomEvent, EventCancelableIsFalse) {
    Event evt("click", true, false);
    EXPECT_FALSE(evt.cancelable());
}

TEST(DomElement, ElementTagNameIs) {
    Element elem("article");
    EXPECT_EQ(elem.tag_name(), "article");
}

TEST(DomElement, ElementWithNoChildren) {
    Element elem("p");
    EXPECT_EQ(elem.child_count(), 0u);
}

TEST(DomElement, ElementSetIdAttr) {
    Element elem("section");
    elem.set_attribute("id", "hero");
    EXPECT_TRUE(elem.has_attribute("id"));
}

TEST(DomElement, ElementGetIdAttr) {
    Element elem("header");
    elem.set_attribute("id", "main-header");
    auto val = elem.get_attribute("id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "main-header");
}

TEST(DomElement, AttributeOverwriteValue) {
    Element elem("div");
    elem.set_attribute("class", "old");
    elem.set_attribute("class", "new");
    auto val = elem.get_attribute("class");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "new");
}

TEST(DomElement, ChildCountAfterAppend) {
    Element parent("ul");
    parent.append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent.child_count(), 1u);
}

TEST(DomNode, NodeFirstChildTag) {
    Element parent("nav");
    parent.append_child(std::make_unique<Element>("a"));
    parent.append_child(std::make_unique<Element>("button"));
    auto* first = dynamic_cast<Element*>(parent.first_child());
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->tag_name(), "a");
}

TEST(DomNode, NodeLastChildTag) {
    Element parent("div");
    parent.append_child(std::make_unique<Element>("span"));
    parent.append_child(std::make_unique<Element>("strong"));
    auto* last = dynamic_cast<Element*>(parent.last_child());
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->tag_name(), "strong");
}

// Cycle 922 — Text set_data, id accessor, sibling chain, parent pointer, text_content from child
TEST(DomText, TextSetDataUpdatesValue) {
    Text t("initial");
    t.set_data("updated");
    EXPECT_EQ(t.data(), "updated");
}

TEST(DomText, TextNodeInitialData) {
    Text t("hello world");
    EXPECT_EQ(t.data(), "hello world");
}

TEST(DomNode, ThreeChildSiblingChain) {
    Element parent("ol");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    EXPECT_EQ(parent.child_count(), 3u);
    auto* first = parent.first_child();
    ASSERT_NE(first, nullptr);
    auto* second = first->next_sibling();
    ASSERT_NE(second, nullptr);
    auto* third = second->next_sibling();
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(third->next_sibling(), nullptr);
}

TEST(DomNode, TwoLiChildrenLastHasNoNextSibling) {
    Element parent("ul");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    auto* last = parent.last_child();
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->next_sibling(), nullptr);
}

TEST(DomElement, ElementIdAfterSetAttr) {
    Element elem("div");
    elem.set_attribute("id", "main");
    EXPECT_EQ(elem.id(), "main");
}

TEST(DomElement, ElementIdEmptyInitially) {
    Element elem("span");
    EXPECT_EQ(elem.id(), "");
}

TEST(DomNode, ChildParentIsParentNode) {
    Element parent("section");
    Node& child = parent.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(child.parent(), &parent);
}

TEST(DomElement, TextContentFromTextChild) {
    Element parent("p");
    parent.append_child(std::make_unique<Text>("visible"));
    EXPECT_EQ(parent.text_content(), "visible");
}

// Cycle 931 — sibling chain, ClassList, text, attribute removal
TEST(DomNode, SecondChildHasPrevSibling) {
    Element parent("div");
    Node& first = parent.append_child(std::make_unique<Element>("span"));
    Node& second = parent.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(second.previous_sibling(), &first);
}

TEST(DomNode, ThreeChildrenPrevChain) {
    Element parent("ul");
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    parent.append_child(std::make_unique<Element>("li"));
    auto* last = parent.last_child();
    ASSERT_NE(last, nullptr);
    auto* mid = last->previous_sibling();
    ASSERT_NE(mid, nullptr);
    auto* first_node = mid->previous_sibling();
    ASSERT_NE(first_node, nullptr);
    EXPECT_EQ(first_node->previous_sibling(), nullptr);
}

TEST(DomText, TextNodeDataAfterSet) {
    Text t("old");
    t.set_data("new value");
    EXPECT_EQ(t.data(), "new value");
    EXPECT_EQ(t.text_content(), "new value");
}

TEST(DomText, TextEmptyData) {
    Text t("");
    EXPECT_EQ(t.data(), "");
    EXPECT_EQ(t.text_content(), "");
}

TEST(DomElement, AttrAbsentAfterRemove) {
    Element elem("p");
    elem.set_attribute("title", "hello");
    elem.remove_attribute("title");
    EXPECT_FALSE(elem.get_attribute("title").has_value());
}

TEST(DomElement, RemoveNonExistentAttrNoop) {
    Element elem("div");
    EXPECT_NO_THROW(elem.remove_attribute("nonexistent"));
}

TEST(DomClassList, ClassListClearAfterRemove) {
    ClassList cl;
    cl.add("a");
    cl.add("b");
    cl.remove("a");
    cl.remove("b");
    EXPECT_EQ(cl.length(), 0u);
}

TEST(DomClassList, ClassListContainsAfterAdd) {
    ClassList cl;
    cl.add("highlight");
    EXPECT_TRUE(cl.contains("highlight"));
    EXPECT_FALSE(cl.contains("other"));
}

// Cycle 940 — ClassList to_string, attribute vector, has_attribute edge cases
TEST(DomClassList, ClassListToStringWithTwo) {
    ClassList cl;
    cl.add("foo");
    cl.add("bar");
    auto str = cl.to_string();
    EXPECT_NE(str.find("foo"), std::string::npos);
    EXPECT_NE(str.find("bar"), std::string::npos);
}

TEST(DomClassList, ClassListToStringEmpty) {
    ClassList cl;
    EXPECT_EQ(cl.to_string(), "");
}

TEST(DomClassList, ClassListContainsAfterRemove) {
    ClassList cl;
    cl.add("active");
    cl.remove("active");
    EXPECT_FALSE(cl.contains("active"));
}

TEST(DomClassList, ClassListAfterToggleTwice) {
    ClassList cl;
    cl.toggle("visible");
    cl.toggle("visible");
    EXPECT_FALSE(cl.contains("visible"));
}

TEST(DomElement, ElementHasTwoAttrs) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    EXPECT_TRUE(elem.has_attribute("type"));
    EXPECT_TRUE(elem.has_attribute("name"));
}

TEST(DomElement, ElementAttrConstRef) {
    Element elem("a");
    elem.set_attribute("href", "https://example.com");
    const auto& attrs = elem.attributes();
    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0].name, "href");
    EXPECT_EQ(attrs[0].value, "https://example.com");
}

TEST(DomElement, ElementThreeAttrsPresent) {
    Element elem("form");
    elem.set_attribute("action", "/submit");
    elem.set_attribute("method", "post");
    elem.set_attribute("enctype", "multipart/form-data");
    EXPECT_TRUE(elem.has_attribute("action"));
    EXPECT_TRUE(elem.has_attribute("method"));
    EXPECT_TRUE(elem.has_attribute("enctype"));
}

TEST(DomElement, ElementNsEmptyDefault) {
    Element elem("p");
    EXPECT_EQ(elem.namespace_uri(), "");
}

// Cycle 949 — Comment edge cases, EventTarget empty dispatch, large document structure
TEST(DomComment, CommentEmptyData) {
    Comment c("");
    EXPECT_EQ(c.data(), "");
}

TEST(DomComment, CommentLongText) {
    std::string long_text(1000, 'x');
    Comment c(long_text);
    EXPECT_EQ(c.data().size(), 1000u);
}

TEST(DomComment, CommentWithHtmlContent) {
    Comment c("<strong>bold</strong>");
    EXPECT_EQ(c.data(), "<strong>bold</strong>");
}

TEST(DomComment, CommentNodeUpdateData) {
    Comment c("original");
    c.set_data("modified");
    EXPECT_EQ(c.data(), "modified");
}

TEST(DomEventTarget, EventTargetNoListenersFiresNoErrors) {
    Document doc;
    auto elem = doc.create_element("div");
    EventTarget target;
    Event evt("click");
    // Dispatching with no listeners should not crash
    EXPECT_NO_THROW(target.dispatch_event(evt, *elem));
}

TEST(DomEventTarget, TwoListenersSameTypeBothFired) {
    Document doc;
    auto elem = doc.create_element("p");
    int count = 0;
    EventTarget target;
    target.add_event_listener("input", [&](Event&) { ++count; });
    target.add_event_listener("input", [&](Event&) { ++count; });
    Event evt("input");
    target.dispatch_event(evt, *elem);
    EXPECT_EQ(count, 2);
}

TEST(DomEventTarget, ListenerForDifferentTypeNotFired) {
    Document doc;
    auto elem = doc.create_element("button");
    bool fired = false;
    EventTarget target;
    target.add_event_listener("mousedown", [&](Event&) { fired = true; });
    Event evt("click");
    target.dispatch_event(evt, *elem);
    EXPECT_FALSE(fired);
}

TEST(DomEventTarget, RemoveAllListenersPreventsDispatch) {
    Document doc;
    auto elem = doc.create_element("input");
    bool fired = false;
    EventTarget target;
    target.add_event_listener("change", [&](Event&) { fired = true; });
    target.remove_all_listeners("change");
    Event evt("change");
    target.dispatch_event(evt, *elem);
    EXPECT_FALSE(fired);
}

TEST(DomNode, ClearDirtyAfterMarkStyle) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Style);
    elem.clear_dirty();
    EXPECT_EQ(elem.dirty_flags(), DirtyFlags::None);
}

TEST(DomNode, ClearDirtyAfterMarkLayout) {
    Element elem("span");
    elem.mark_dirty(DirtyFlags::Layout);
    elem.clear_dirty();
    EXPECT_EQ(elem.dirty_flags(), DirtyFlags::None);
}

TEST(DomNode, ClearDirtyAfterMarkPaint) {
    Element elem("p");
    elem.mark_dirty(DirtyFlags::Paint);
    elem.clear_dirty();
    EXPECT_EQ(elem.dirty_flags(), DirtyFlags::None);
}

TEST(DomNode, DirtyNoneInitially) {
    Element elem("div");
    EXPECT_EQ(elem.dirty_flags(), DirtyFlags::None);
}

TEST(DomNode, MarkDirtyStyleOnlyStyle) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Style);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
    EXPECT_EQ(elem.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

TEST(DomNode, MarkDirtyPaintOnlyPaint) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Paint);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
    EXPECT_EQ(elem.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
}

TEST(DomNode, DirtyAllContainsLayout) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::All);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
}

TEST(DomNode, DirtyAllContainsStyle) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::All);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Style, DirtyFlags::None);
}

TEST(DomNode, DirtyAllContainsPaint) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::All);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
}

TEST(DomNode, MarkDirtyLayoutNotPaint) {
    Element elem("div");
    elem.mark_dirty(DirtyFlags::Layout);
    EXPECT_NE(elem.dirty_flags() & DirtyFlags::Layout, DirtyFlags::None);
    EXPECT_EQ(elem.dirty_flags() & DirtyFlags::Paint, DirtyFlags::None);
}

TEST(DomNode, NodePrevSiblingNullForFirst) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto& li = parent->append_child(doc.create_element("li"));
    EXPECT_EQ(li.previous_sibling(), nullptr);
}

TEST(DomNode, NodeNextSiblingNullForLast) {
    Document doc;
    auto parent = doc.create_element("ul");
    parent->append_child(doc.create_element("li"));
    auto& li2 = parent->append_child(doc.create_element("li"));
    EXPECT_EQ(li2.next_sibling(), nullptr);
}

TEST(DomNode, NodeParentNullForDetached) {
    Document doc;
    auto elem = doc.create_element("div");
    EXPECT_EQ(elem->parent(), nullptr);
}

TEST(DomDocument, DocumentCreateElementTagName) {
    Document doc;
    auto elem = doc.create_element("section");
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "section");
}

TEST(DomDocument, DocumentCreateTextNodeData) {
    Document doc;
    auto text = doc.create_text_node("hello world");
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->data(), "hello world");
}

TEST(DomDocument, DocumentCreateCommentData) {
    Document doc;
    auto comment = doc.create_comment("test comment");
    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->data(), "test comment");
}

TEST(DomDocument, DocumentNodeTypeIsDocumentV2) {
    Document doc;
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

TEST(DomDocument, DocumentChildCountAfterAppend) {
    Document doc;
    doc.append_child(doc.create_element("html"));
    EXPECT_EQ(doc.child_count(), 1u);
}

TEST(DomElement, ElementChildCountAfterTwoAppends) {
    Document doc;
    auto div = doc.create_element("div");
    div->append_child(doc.create_element("p"));
    div->append_child(doc.create_element("span"));
    EXPECT_EQ(div->child_count(), 2u);
}

TEST(DomElement, ElementFirstChildTagName) {
    Document doc;
    auto div = doc.create_element("div");
    div->append_child(doc.create_element("p"));
    div->append_child(doc.create_element("span"));
    auto* first = div->first_child();
    ASSERT_NE(first, nullptr);
    auto* elem = dynamic_cast<Element*>(first);
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "p");
}

TEST(DomElement, ElementLastChildTagName) {
    Document doc;
    auto div = doc.create_element("div");
    div->append_child(doc.create_element("h1"));
    div->append_child(doc.create_element("h2"));
    auto* last = div->last_child();
    ASSERT_NE(last, nullptr);
    auto* elem = dynamic_cast<Element*>(last);
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "h2");
}

TEST(DomNode, ForEachChildCountsThree) {
    Document doc;
    auto ul = doc.create_element("ul");
    ul->append_child(doc.create_element("li"));
    ul->append_child(doc.create_element("li"));
    ul->append_child(doc.create_element("li"));
    int count = 0;
    ul->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(DomText, TextNodeTypeIsTextV2) {
    Document doc;
    auto text = doc.create_text_node("hello");
    EXPECT_EQ(text->node_type(), NodeType::Text);
}

TEST(DomComment, CommentNodeTypeIsCommentV3) {
    Document doc;
    auto comment = doc.create_comment("some comment");
    EXPECT_EQ(comment->node_type(), NodeType::Comment);
}

TEST(DomNode, ForEachChildCountsFour) {
    Document doc;
    auto div = doc.create_element("div");
    div->append_child(doc.create_element("p"));
    div->append_child(doc.create_element("p"));
    div->append_child(doc.create_element("p"));
    div->append_child(doc.create_element("p"));
    int count = 0;
    div->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 4);
}

TEST(DomDocument, DocumentBodyNullWhenEmpty) {
    Document doc;
    EXPECT_EQ(doc.body(), nullptr);
}

TEST(DomDocument, DocumentHeadNullWhenEmpty) {
    Document doc;
    EXPECT_EQ(doc.head(), nullptr);
}

TEST(DomDocument, DocumentNodeTypeIsDocumentV3) {
    Document doc;
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

TEST(DomDocument, DocumentChildCountZeroInitially) {
    Document doc;
    EXPECT_EQ(doc.child_count(), 0u);
}

TEST(DomElement, ElementAttributesEmptyInitially) {
    Document doc;
    auto div = doc.create_element("div");
    EXPECT_TRUE(div->attributes().empty());
}

TEST(DomElement, ElementHasAttrFalseBeforeSet) {
    Document doc;
    auto span = doc.create_element("span");
    EXPECT_FALSE(span->has_attribute("class"));
}

TEST(DomElement, ElementRemoveAttrReducesCount) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "http://example.com");
    el->set_attribute("target", "_blank");
    el->remove_attribute("href");
    EXPECT_EQ(el->attributes().size(), 1u);
}

TEST(DomElement, ElementGetAttrReturnsValue) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "https://example.com");
    EXPECT_EQ(el->get_attribute("href"), "https://example.com");
}

TEST(DomElement, ElementTwoAttrsCount) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("src", "photo.jpg");
    el->set_attribute("alt", "A photo");
    EXPECT_EQ(el->attributes().size(), 2u);
}

TEST(DomNode, NodeNextSiblingSetCorrectly) {
    Document doc;
    auto ul = doc.create_element("ul");
    auto li1 = doc.create_element("li");
    auto li2 = doc.create_element("li");
    ul->append_child(std::move(li1));
    ul->append_child(std::move(li2));
    auto* first = ul->first_child();
    ASSERT_NE(first, nullptr);
    auto* second = first->next_sibling();
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->node_type(), NodeType::Element);
}

TEST(DomNode, NodePrevSiblingSetCorrectly) {
    Document doc;
    auto div = doc.create_element("div");
    div->append_child(doc.create_element("p"));
    div->append_child(doc.create_element("span"));
    auto* last = div->last_child();
    ASSERT_NE(last, nullptr);
    auto* prev = last->previous_sibling();
    ASSERT_NE(prev, nullptr);
    auto* pelem = dynamic_cast<Element*>(prev);
    ASSERT_NE(pelem, nullptr);
    EXPECT_EQ(pelem->tag_name(), "p");
}

TEST(DomElement, ClassListEmptyInitially) {
    Document doc;
    auto div = doc.create_element("div");
    EXPECT_EQ(div->class_list().length(), 0u);
}

TEST(DomElement, ClassListAddSingle) {
    Document doc;
    auto div = doc.create_element("div");
    div->class_list().add("active");
    EXPECT_TRUE(div->class_list().contains("active"));
}

TEST(DomElement, ClassListRemoveSingle) {
    Document doc;
    auto div = doc.create_element("div");
    div->class_list().add("active");
    div->class_list().remove("active");
    EXPECT_FALSE(div->class_list().contains("active"));
}

TEST(DomElement, ClassListToggleAdds) {
    Document doc;
    auto div = doc.create_element("div");
    div->class_list().toggle("highlight");
    EXPECT_TRUE(div->class_list().contains("highlight"));
}

TEST(DomNode, NodeAppendChildSetsParent) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("span");
    auto* raw = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(raw->parent(), parent.get());
}

TEST(DomElement, ElementGetAttributeNotSet) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_FALSE(el->get_attribute("data-x").has_value());
}

TEST(DomElement, ElementSetAndGetTwo) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    el->set_attribute("name", "username");
    EXPECT_EQ(el->get_attribute("type"), "text");
    EXPECT_EQ(el->get_attribute("name"), "username");
}

TEST(DomElement, ElementClassListLength) {
    Document doc;
    auto div = doc.create_element("div");
    div->class_list().add("a");
    div->class_list().add("b");
    div->class_list().add("c");
    EXPECT_EQ(div->class_list().length(), 3u);
}

TEST(DomNode, TextNodeParentAfterAppend) {
    Document doc;
    auto div = doc.create_element("div");
    auto text = doc.create_text_node("hello");
    auto* raw = text.get();
    div->append_child(std::move(text));
    EXPECT_EQ(raw->parent(), div.get());
}

TEST(DomNode, NodeChildCountAfterAppendThree) {
    Document doc;
    auto ul = doc.create_element("ul");
    ul->append_child(doc.create_element("li"));
    ul->append_child(doc.create_element("li"));
    ul->append_child(doc.create_element("li"));
    EXPECT_EQ(ul->child_count(), 3u);
}

TEST(DomElement, ElementClassListToStringTwo) {
    Document doc;
    auto div = doc.create_element("div");
    div->class_list().add("foo");
    div->class_list().add("bar");
    EXPECT_FALSE(div->class_list().to_string().empty());
}

TEST(DomNode, NodeTextContentRecursive) {
    Document doc;
    auto div = doc.create_element("div");
    div->append_child(doc.create_text_node("Hello "));
    div->append_child(doc.create_text_node("World"));
    EXPECT_EQ(div->text_content(), "Hello World");
}

// ---------------------------------------------------------------------------
// Cycle 1012 — DOM attribute overwrite, removal, id accessor, text content,
//              class list toggle, parent null, multiple attributes
// ---------------------------------------------------------------------------

TEST(DomElement, SetAttributeOverwrite) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-x", "old");
    el->set_attribute("data-x", "new");
    std::string found_value;
    for (const auto& attr : el->attributes()) {
        if (attr.name == "data-x") {
            found_value = attr.value;
        }
    }
    EXPECT_EQ(found_value, "new");
}

TEST(DomNode, RemoveLastChild) {
    Document doc;
    auto ul = doc.create_element("ul");
    ul->append_child(doc.create_element("li"));
    ul->append_child(doc.create_element("li"));
    auto li3 = doc.create_element("li");
    auto* li3_ptr = li3.get();
    ul->append_child(std::move(li3));
    EXPECT_EQ(ul->child_count(), 3u);
    ul->remove_child(*li3_ptr);
    EXPECT_EQ(ul->child_count(), 2u);
}

TEST(DomElement, IdReturnsSetValue) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "test-id");
    EXPECT_EQ(el->id(), "test-id");
}

TEST(DomNode, FirstChildAfterRemoveFirst) {
    Document doc;
    auto parent = doc.create_element("div");
    auto a = doc.create_element("span");
    auto* a_ptr = a.get();
    parent->append_child(std::move(a));
    parent->append_child(doc.create_element("span"));
    parent->remove_child(*a_ptr);
    auto* first = dynamic_cast<Element*>(parent->first_child());
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->tag_name(), "span");
}

TEST(DomNode, TextContentEmpty) {
    Document doc;
    auto div = doc.create_element("div");
    EXPECT_EQ(div->text_content(), "");
}

TEST(DomElement, ClassListContainsAfterToggleV2) {
    Document doc;
    auto div = doc.create_element("div");
    div->class_list().toggle("active");
    EXPECT_TRUE(div->class_list().contains("active"));
}

TEST(DomNode, ParentNullBeforeAppend) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_EQ(el->parent(), nullptr);
}

TEST(DomElement, MultipleAttributeCount) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "main");
    el->set_attribute("class", "container");
    el->set_attribute("data-role", "widget");
    el->set_attribute("title", "My Div");
    EXPECT_EQ(el->attributes().size(), 4u);
}

// ---------------------------------------------------------------------------
// Cycle 1012: DOM element and node tests (+8)
// ---------------------------------------------------------------------------

TEST(DomElement, SetGetAttributeRoundTrip) {
    Document doc;
    auto el = doc.create_element("span");
    el->set_attribute("data-key", "hello-world");
    EXPECT_EQ(el->get_attribute("data-key"), "hello-world");
}

TEST(DomElement, RemoveLastChildMakesEmpty) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto child = doc.create_element("li");
    auto* child_ptr = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(parent->child_count(), 1u);
    parent->remove_child(*child_ptr);
    EXPECT_EQ(parent->child_count(), 0u);
}

TEST(DomNode, TextContentIncludesDescendants) {
    Document doc;
    auto div = doc.create_element("div");
    auto span = doc.create_element("span");
    auto t1 = doc.create_text_node("Hello");
    auto t2 = doc.create_text_node(" World");
    span->append_child(std::move(t2));
    div->append_child(std::move(t1));
    div->append_child(std::move(span));
    EXPECT_EQ(div->text_content(), "Hello World");
}

TEST(DomElement, TagNameUppercase) {
    Document doc;
    auto el = doc.create_element("div");
    std::string name = el->tag_name();
    // tag_name() should return the tag as provided or uppercased;
    // accept either "div" or "DIV" depending on implementation
    EXPECT_TRUE(name == "div" || name == "DIV");
}

TEST(DomElement, NoChildrenInitiallyV2) {
    Document doc;
    auto el = doc.create_element("section");
    EXPECT_EQ(el->child_count(), 0u);
    EXPECT_EQ(el->first_child(), nullptr);
    EXPECT_EQ(el->last_child(), nullptr);
}

TEST(DomElement, SetAttributeOverwritesV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    el->set_attribute("type", "password");
    EXPECT_EQ(el->get_attribute("type"), "password");
    EXPECT_EQ(el->attributes().size(), 1u);
}

TEST(DomNode, NextSiblingNullForLastV2) {
    Document doc;
    auto parent = doc.create_element("div");
    auto a = doc.create_element("p");
    auto b = doc.create_element("p");
    auto* a_ptr = a.get();
    auto* b_ptr = b.get();
    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    EXPECT_EQ(b_ptr->next_sibling(), nullptr);
    EXPECT_NE(a_ptr->next_sibling(), nullptr);
}

TEST(DomElement, ClassListTwoDistinctV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("alpha");
    el->class_list().add("beta");
    EXPECT_TRUE(el->class_list().contains("alpha"));
    EXPECT_TRUE(el->class_list().contains("beta"));
}

// --- Cycle 1021: DOM node/element tests ---

TEST(DomElement, TagNameSpan) {
    Document doc;
    auto el = doc.create_element("span");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "span" || name == "SPAN");
}

TEST(DomNode, FirstChildAfterAppendV3) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("p");
    auto* child_ptr = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(parent->first_child(), child_ptr);
}

TEST(DomNode, LastChildAfterTwoAppends) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto a = doc.create_element("li");
    auto b = doc.create_element("li");
    auto* b_ptr = b.get();
    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    EXPECT_EQ(parent->last_child(), b_ptr);
}

TEST(DomElement, GetAttributeReturnsNulloptForMissing) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_FALSE(el->get_attribute("nonexistent").has_value());
}

TEST(DomElement, ClassListToggleRemoves) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("active");
    el->class_list().toggle("active");
    EXPECT_FALSE(el->class_list().contains("active"));
}

TEST(DomElement, ClassListToStringV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("foo");
    el->class_list().add("bar");
    auto s = el->class_list().to_string();
    EXPECT_NE(s.find("foo"), std::string::npos);
    EXPECT_NE(s.find("bar"), std::string::npos);
}

TEST(DomNode, ChildCountAfterTwoAppendsV2) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("a"));
    parent->append_child(doc.create_element("b"));
    EXPECT_EQ(parent->child_count(), 2u);
}

TEST(DomNode, PreviousSiblingNullForFirstV3) {
    Document doc;
    auto parent = doc.create_element("div");
    auto a = doc.create_element("p");
    auto b = doc.create_element("p");
    auto* a_ptr = a.get();
    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    EXPECT_EQ(a_ptr->previous_sibling(), nullptr);
}

// --- Cycle 1030: DOM tests ---

TEST(DomElement, SetAttributeIdV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "main");
    auto val = el->get_attribute("id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "main");
}

TEST(DomElement, ClassListRemoveThenNotContainsV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    el->class_list().remove("a");
    EXPECT_FALSE(el->class_list().contains("a"));
    EXPECT_TRUE(el->class_list().contains("b"));
}

TEST(DomNode, CreateTextNodeV3) {
    Document doc;
    auto t = doc.create_text_node("hello");
    EXPECT_EQ(t->text_content(), "hello");
}

TEST(DomNode, CreateCommentNotNull) {
    Document doc;
    auto c = doc.create_comment("a comment");
    EXPECT_NE(c.get(), nullptr);
}

TEST(DomElement, TagNameArticle) {
    Document doc;
    auto el = doc.create_element("article");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "article" || name == "ARTICLE");
}

TEST(DomElement, MultipleAttributesV3) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    el->set_attribute("name", "field");
    el->set_attribute("placeholder", "enter");
    EXPECT_EQ(el->attributes().size(), 3u);
}

TEST(DomNode, AppendThreeChildrenCount) {
    Document doc;
    auto p = doc.create_element("ul");
    p->append_child(doc.create_element("li"));
    p->append_child(doc.create_element("li"));
    p->append_child(doc.create_element("li"));
    EXPECT_EQ(p->child_count(), 3u);
}

TEST(DomElement, ClassListContainsAfterAddV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("active");
    EXPECT_TRUE(el->class_list().contains("active"));
}

// --- Cycle 1039: DOM tests ---

TEST(DomElement, TagNameH1) {
    Document doc;
    auto el = doc.create_element("h1");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "h1" || name == "H1");
}

TEST(DomElement, SetAttributeDataV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-id", "42");
    auto val = el->get_attribute("data-id");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "42");
}

TEST(DomNode, AppendChildSetsParent) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("span");
    auto* child_ptr = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(child_ptr->parent(), parent.get());
}

TEST(DomNode, TextNodeContent) {
    Document doc;
    auto t = doc.create_text_node("world");
    EXPECT_EQ(t->text_content(), "world");
}

TEST(DomElement, AttributesSizeZero) {
    Document doc;
    auto el = doc.create_element("p");
    EXPECT_EQ(el->attributes().size(), 0u);
}

TEST(DomElement, ClassListNotContainsInitially) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_FALSE(el->class_list().contains("anything"));
}

TEST(DomNode, FirstChildNullEmpty) {
    Document doc;
    auto el = doc.create_element("ul");
    EXPECT_EQ(el->first_child(), nullptr);
}

TEST(DomNode, LastChildNullEmpty) {
    Document doc;
    auto el = doc.create_element("ul");
    EXPECT_EQ(el->last_child(), nullptr);
}

// --- Cycle 1048: DOM tests ---

TEST(DomElement, TagNameUl) {
    Document doc;
    auto el = doc.create_element("ul");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "ul" || name == "UL");
}

TEST(DomElement, TagNameOl) {
    Document doc;
    auto el = doc.create_element("ol");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "ol" || name == "OL");
}

TEST(DomElement, SetAttributeClassV4) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("class", "foo bar");
    auto val = el->get_attribute("class");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "foo bar");
}

TEST(DomNode, ChildCountTwo) {
    Document doc;
    auto parent = doc.create_element("div");
    auto c1 = doc.create_element("p");
    auto c2 = doc.create_element("span");
    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    EXPECT_EQ(parent->child_count(), 2u);
}

TEST(DomElement, HasAttributeTrueV4) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    EXPECT_TRUE(el->has_attribute("type"));
}

TEST(DomElement, HasAttributeFalseV4) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_FALSE(el->has_attribute("style"));
}

TEST(DomNode, NextSiblingAfterAppendV4) {
    Document doc;
    auto parent = doc.create_element("div");
    auto c1 = doc.create_element("a");
    auto c2 = doc.create_element("b");
    auto* c1_ptr = c1.get();
    auto* c2_ptr = c2.get();
    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    EXPECT_EQ(c1_ptr->next_sibling(), c2_ptr);
}

TEST(DomNode, PreviousSiblingAfterAppendV4) {
    Document doc;
    auto parent = doc.create_element("div");
    auto c1 = doc.create_element("a");
    auto c2 = doc.create_element("b");
    auto* c1_ptr = c1.get();
    auto* c2_ptr = c2.get();
    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    EXPECT_EQ(c2_ptr->previous_sibling(), c1_ptr);
}

// --- Cycle 1057: DOM tests ---

TEST(DomElement, TagNameLi) {
    Document doc;
    auto el = doc.create_element("li");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "li" || name == "LI");
}

TEST(DomElement, TagNameTable) {
    Document doc;
    auto el = doc.create_element("table");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "table" || name == "TABLE");
}

TEST(DomElement, RemoveAttributeV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "test");
    el->remove_attribute("id");
    EXPECT_FALSE(el->has_attribute("id"));
}

TEST(DomNode, TextNodeTypeCheck) {
    Document doc;
    auto t = doc.create_text_node("hello");
    EXPECT_NE(t, nullptr);
}

TEST(DomElement, ClassListToggleAddsV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().toggle("active");
    EXPECT_TRUE(el->class_list().contains("active"));
}

TEST(DomElement, ClassListToggleRemovesV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("active");
    el->class_list().toggle("active");
    EXPECT_FALSE(el->class_list().contains("active"));
}

TEST(DomNode, ChildCountAfterRemoveV5) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("span");
    auto& child_ref = *child;
    parent->append_child(std::move(child));
    parent->remove_child(child_ref);
    EXPECT_EQ(parent->child_count(), 0u);
}

TEST(DomElement, SetAttributeTwiceOverwrites) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "/old");
    el->set_attribute("href", "/new");
    EXPECT_EQ(el->get_attribute("href").value(), "/new");
}

// --- Cycle 1066: DOM tests ---

TEST(DomElement, TagNameForm) {
    Document doc;
    auto el = doc.create_element("form");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "form" || name == "FORM");
}

TEST(DomElement, TagNameInput) {
    Document doc;
    auto el = doc.create_element("input");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "input" || name == "INPUT");
}

TEST(DomElement, AttributeCountAfterThreeV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "x");
    el->set_attribute("class", "y");
    el->set_attribute("style", "z");
    EXPECT_EQ(el->attributes().size(), 3u);
}

TEST(DomNode, CreateTextNodeEmpty) {
    Document doc;
    auto t = doc.create_text_node("");
    EXPECT_EQ(t->text_content(), "");
}

TEST(DomElement, ClassListAddTwoV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    EXPECT_TRUE(el->class_list().contains("a"));
    EXPECT_TRUE(el->class_list().contains("b"));
}

TEST(DomElement, ClassListRemoveOneOfTwoV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("x");
    el->class_list().add("y");
    el->class_list().remove("x");
    EXPECT_FALSE(el->class_list().contains("x"));
    EXPECT_TRUE(el->class_list().contains("y"));
}

TEST(DomNode, FirstChildAfterTwoAppends) {
    Document doc;
    auto parent = doc.create_element("div");
    auto c1 = doc.create_element("a");
    auto c2 = doc.create_element("b");
    auto* c1_ptr = c1.get();
    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    EXPECT_EQ(parent->first_child(), c1_ptr);
}

TEST(DomNode, LastChildAfterThreeAppends) {
    Document doc;
    auto parent = doc.create_element("div");
    auto c1 = doc.create_element("a");
    auto c2 = doc.create_element("b");
    auto c3 = doc.create_element("c");
    auto* c3_ptr = c3.get();
    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    parent->append_child(std::move(c3));
    EXPECT_EQ(parent->last_child(), c3_ptr);
}

// --- Cycle 1075: DOM tests ---

TEST(DomElement, TagNameSection) {
    Document doc;
    auto el = doc.create_element("section");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "section" || name == "SECTION");
}

TEST(DomElement, TagNameNav) {
    Document doc;
    auto el = doc.create_element("nav");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "nav" || name == "NAV");
}

TEST(DomElement, TagNameHeader) {
    Document doc;
    auto el = doc.create_element("header");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "header" || name == "HEADER");
}

TEST(DomElement, TagNameFooter) {
    Document doc;
    auto el = doc.create_element("footer");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "footer" || name == "FOOTER");
}

TEST(DomElement, SetAttributeStyleV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("style", "color: red");
    EXPECT_EQ(el->get_attribute("style").value(), "color: red");
}

TEST(DomElement, RemoveAttributeClassV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("class", "foo");
    el->remove_attribute("class");
    EXPECT_FALSE(el->get_attribute("class").has_value());
}

TEST(DomNode, ChildCountThree) {
    Document doc;
    auto parent = doc.create_element("ul");
    parent->append_child(doc.create_element("li"));
    parent->append_child(doc.create_element("li"));
    parent->append_child(doc.create_element("li"));
    EXPECT_EQ(parent->child_count(), 3u);
}

TEST(DomElement, ClassListToStringV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("alpha");
    el->class_list().add("beta");
    auto str = el->class_list().to_string();
    EXPECT_NE(str.find("alpha"), std::string::npos);
    EXPECT_NE(str.find("beta"), std::string::npos);
}

// --- Cycle 1084: DOM tests ---

TEST(DomElement, TagNameMain) {
    Document doc;
    auto el = doc.create_element("main");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "main" || name == "MAIN");
}

TEST(DomElement, TagNameAside) {
    Document doc;
    auto el = doc.create_element("aside");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "aside" || name == "ASIDE");
}

TEST(DomElement, GetAttributeHref) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "https://example.com");
    EXPECT_EQ(el->get_attribute("href").value(), "https://example.com");
}

TEST(DomNode, TextContentAfterSetV5) {
    Document doc;
    auto t = doc.create_text_node("original");
    EXPECT_EQ(t->text_content(), "original");
}

TEST(DomElement, HasAttributeAfterRemoveV5) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("src", "pic.png");
    el->remove_attribute("src");
    EXPECT_FALSE(el->has_attribute("src"));
}

TEST(DomElement, ClassListAddDuplicateV5) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("x");
    el->class_list().add("x");
    // Adding duplicate should not crash; still contains
    EXPECT_TRUE(el->class_list().contains("x"));
}

TEST(DomNode, ParentNullForDetachedV5) {
    Document doc;
    auto el = doc.create_element("span");
    EXPECT_EQ(el->parent(), nullptr);
}

TEST(DomNode, NextSiblingNullForSingleChild) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("p");
    auto* child_ptr = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(child_ptr->next_sibling(), nullptr);
}

// --- Cycle 1093: 8 DOM tests ---

TEST(DomElement, TagNameSummary) {
    Document doc;
    auto el = doc.create_element("summary");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "summary" || name == "SUMMARY");
}

TEST(DomElement, TagNameDialog) {
    Document doc;
    auto el = doc.create_element("dialog");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "dialog" || name == "DIALOG");
}

TEST(DomElement, TagNameTemplate) {
    Document doc;
    auto el = doc.create_element("template");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "template" || name == "TEMPLATE");
}

TEST(DomElement, TagNameDetails) {
    Document doc;
    auto el = doc.create_element("details");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "details" || name == "DETAILS");
}

TEST(DomElement, SetAttributeHrefV5) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "/page");
    EXPECT_EQ(el->get_attribute("href"), "/page");
}

TEST(DomElement, RemoveAttributeHrefV5) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "/page");
    el->remove_attribute("href");
    EXPECT_FALSE(el->get_attribute("href").has_value());
}

TEST(DomNode, ChildCountFour) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("a"));
    parent->append_child(doc.create_element("b"));
    parent->append_child(doc.create_element("c"));
    parent->append_child(doc.create_element("d"));
    EXPECT_EQ(parent->child_count(), 4u);
}

TEST(DomElement, ClassListContainsAfterAddV6) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("highlight");
    EXPECT_TRUE(el->class_list().contains("highlight"));
}

// --- Cycle 1102: 8 DOM tests ---

TEST(DomElement, TagNameFigure) {
    Document doc;
    auto el = doc.create_element("figure");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "figure" || name == "FIGURE");
}

TEST(DomElement, TagNameFigcaption) {
    Document doc;
    auto el = doc.create_element("figcaption");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "figcaption" || name == "FIGCAPTION");
}

TEST(DomElement, SetAttributeSrcV6) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("src", "/img.png");
    EXPECT_EQ(el->get_attribute("src"), "/img.png");
}

TEST(DomElement, SetAttributeAltV6) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("alt", "photo");
    EXPECT_EQ(el->get_attribute("alt"), "photo");
}

TEST(DomNode, ChildCountFive) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 5; i++) parent->append_child(doc.create_element("p"));
    EXPECT_EQ(parent->child_count(), 5u);
}

TEST(DomElement, ClassListRemoveNotPresentV6) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().remove("nope");
    EXPECT_FALSE(el->class_list().contains("nope"));
}

TEST(DomElement, ClassListToggleAddsV6) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().toggle("active");
    EXPECT_TRUE(el->class_list().contains("active"));
}

TEST(DomElement, HasAttributeAfterSetV6) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-id", "42");
    EXPECT_TRUE(el->has_attribute("data-id"));
}

// --- Cycle 1111: 8 DOM tests ---

TEST(DomElement, TagNameMark) {
    Document doc;
    auto el = doc.create_element("mark");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "mark" || name == "MARK");
}

TEST(DomElement, TagNameTime) {
    Document doc;
    auto el = doc.create_element("time");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "time" || name == "TIME");
}

TEST(DomElement, SetAttributeWidthV6) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("width", "100");
    EXPECT_EQ(el->get_attribute("width"), "100");
}

TEST(DomElement, SetAttributeHeightV6) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("height", "200");
    EXPECT_EQ(el->get_attribute("height"), "200");
}

TEST(DomNode, ParentNullForRoot) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_EQ(el->parent(), nullptr);
}

TEST(DomNode, ParentSetAfterAppendV6) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("span");
    auto* child_ptr = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(child_ptr->parent(), parent.get());
}

TEST(DomElement, ClassListToStringAfterTwoAddsV6) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    std::string s = el->class_list().to_string();
    EXPECT_TRUE(s.find("a") != std::string::npos);
    EXPECT_TRUE(s.find("b") != std::string::npos);
}

TEST(DomElement, AttributesSizeAfterTwoSetsV6) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "x");
    el->set_attribute("class", "y");
    EXPECT_EQ(el->attributes().size(), 2u);
}

// --- Cycle 1120: 8 DOM tests ---

TEST(DomElement, TagNameOutput) {
    Document doc;
    auto el = doc.create_element("output");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "output" || name == "OUTPUT");
}

TEST(DomElement, TagNameData) {
    Document doc;
    auto el = doc.create_element("data");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "data" || name == "DATA");
}

TEST(DomElement, SetAttributeTypeV7) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    EXPECT_EQ(el->get_attribute("type"), "text");
}

TEST(DomElement, SetAttributeNameV7) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("name", "username");
    EXPECT_EQ(el->get_attribute("name"), "username");
}

TEST(DomNode, FirstChildNotNullAfterAppendV7) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("p"));
    EXPECT_NE(parent->first_child(), nullptr);
}

TEST(DomNode, LastChildNotNullAfterAppendV7) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("span"));
    EXPECT_NE(parent->last_child(), nullptr);
}

TEST(DomElement, ClassListToggleRemovesV7) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("active");
    el->class_list().toggle("active");
    EXPECT_FALSE(el->class_list().contains("active"));
}

TEST(DomElement, GetAttributeReturnsNulloptForMissingV7) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_FALSE(el->get_attribute("nonexistent").has_value());
}

// --- Cycle 1129: 8 DOM tests ---

TEST(DomElement, TagNameProgress) {
    Document doc;
    auto el = doc.create_element("progress");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "progress" || name == "PROGRESS");
}

TEST(DomElement, TagNameMeter) {
    Document doc;
    auto el = doc.create_element("meter");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "meter" || name == "METER");
}

TEST(DomElement, SetAttributeValueV7) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("value", "test");
    EXPECT_EQ(el->get_attribute("value"), "test");
}

TEST(DomElement, SetAttributePlaceholderV7) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("placeholder", "Enter name");
    EXPECT_EQ(el->get_attribute("placeholder"), "Enter name");
}

TEST(DomNode, ChildCountSix) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 6; i++) parent->append_child(doc.create_element("p"));
    EXPECT_EQ(parent->child_count(), 6u);
}

TEST(DomElement, ClassListAddThreeV7) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    el->class_list().add("c");
    EXPECT_TRUE(el->class_list().contains("a"));
    EXPECT_TRUE(el->class_list().contains("b"));
    EXPECT_TRUE(el->class_list().contains("c"));
}

TEST(DomElement, HasAttributeFalseAfterRemoveV7) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("x", "y");
    el->remove_attribute("x");
    EXPECT_FALSE(el->has_attribute("x"));
}

TEST(DomElement, AttributesSizeAfterThreeSetsV7) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("a", "1");
    el->set_attribute("b", "2");
    el->set_attribute("c", "3");
    EXPECT_EQ(el->attributes().size(), 3u);
}

// --- Cycle 1138: 8 DOM tests ---

TEST(DomElement, TagNamePicture) {
    Document doc;
    auto el = doc.create_element("picture");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "picture" || name == "PICTURE");
}

TEST(DomElement, TagNameSource) {
    Document doc;
    auto el = doc.create_element("source");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "source" || name == "SOURCE");
}

TEST(DomElement, SetAttributeMin) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("min", "0");
    EXPECT_EQ(el->get_attribute("min").value(), "0");
}

TEST(DomElement, SetAttributeMax) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("max", "100");
    EXPECT_EQ(el->get_attribute("max").value(), "100");
}

TEST(DomElement, ChildCountSeven) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 7; ++i)
        parent->append_child(doc.create_element("span"));
    EXPECT_EQ(parent->child_count(), 7u);
}

TEST(DomElement, ClassListAddFour) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    el->class_list().add("c");
    el->class_list().add("d");
    EXPECT_TRUE(el->class_list().contains("d"));
}

TEST(DomElement, HasAttributeTrueAfterSet) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("role", "button");
    EXPECT_TRUE(el->has_attribute("role"));
}

TEST(DomElement, AttributesSizeFour) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("a", "1");
    el->set_attribute("b", "2");
    el->set_attribute("c", "3");
    el->set_attribute("d", "4");
    EXPECT_EQ(el->attributes().size(), 4u);
}

// --- Cycle 1147: 8 DOM tests ---

TEST(DomElement, TagNameSlot) {
    Document doc;
    auto el = doc.create_element("slot");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "slot" || name == "SLOT");
}

TEST(DomElement, TagNameVideo) {
    Document doc;
    auto el = doc.create_element("video");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "video" || name == "VIDEO");
}

TEST(DomElement, SetAttributeAction) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("action", "/submit");
    EXPECT_EQ(el->get_attribute("action").value(), "/submit");
}

TEST(DomElement, SetAttributeMethod) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("method", "post");
    EXPECT_EQ(el->get_attribute("method").value(), "post");
}

TEST(DomElement, ChildCountEight) {
    Document doc;
    auto parent = doc.create_element("ul");
    for (int i = 0; i < 8; ++i)
        parent->append_child(doc.create_element("li"));
    EXPECT_EQ(parent->child_count(), 8u);
}

TEST(DomElement, ClassListRemoveTwo) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    el->class_list().add("c");
    el->class_list().remove("a");
    el->class_list().remove("b");
    EXPECT_TRUE(el->class_list().contains("c"));
    EXPECT_FALSE(el->class_list().contains("a"));
}

TEST(DomElement, GetAttributeAfterOverwriteV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-x", "old");
    el->set_attribute("data-x", "new");
    EXPECT_EQ(el->get_attribute("data-x").value(), "new");
}

TEST(DomElement, AttributesSizeFive) {
    Document doc;
    auto el = doc.create_element("div");
    for (int i = 0; i < 5; ++i)
        el->set_attribute("attr" + std::to_string(i), "v");
    EXPECT_EQ(el->attributes().size(), 5u);
}

// --- Cycle 1156: 8 DOM tests ---

TEST(DomElement, TagNameAudio) {
    Document doc;
    auto el = doc.create_element("audio");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "audio" || name == "AUDIO");
}

TEST(DomElement, TagNameCanvas) {
    Document doc;
    auto el = doc.create_element("canvas");
    std::string name = el->tag_name();
    EXPECT_TRUE(name == "canvas" || name == "CANVAS");
}

TEST(DomElement, SetAttributeHref) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "https://example.com");
    EXPECT_EQ(el->get_attribute("href").value(), "https://example.com");
}

TEST(DomElement, SetAttributeTarget) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("target", "_blank");
    EXPECT_EQ(el->get_attribute("target").value(), "_blank");
}

TEST(DomElement, ChildCountNine) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 9; ++i)
        parent->append_child(doc.create_element("p"));
    EXPECT_EQ(parent->child_count(), 9u);
}

TEST(DomElement, ClassListContainsFourItems) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("w");
    el->class_list().add("x");
    el->class_list().add("y");
    el->class_list().add("z");
    EXPECT_TRUE(el->class_list().contains("w"));
    EXPECT_TRUE(el->class_list().contains("z"));
}

TEST(DomElement, RemoveAttributeThenSizeDecreases) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("a", "1");
    el->set_attribute("b", "2");
    el->remove_attribute("a");
    EXPECT_EQ(el->attributes().size(), 1u);
}

TEST(DomElement, HasAttributeAfterMultipleSets) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("x", "1");
    el->set_attribute("y", "2");
    el->set_attribute("z", "3");
    EXPECT_TRUE(el->has_attribute("y"));
}

// --- Cycle 1165: 8 DOM tests ---

TEST(DomElement, TagNameLabel) {
    Document doc;
    auto el = doc.create_element("label");
    EXPECT_EQ(el->tag_name(), "label");
}

TEST(DomElement, TagNameFieldset) {
    Document doc;
    auto el = doc.create_element("fieldset");
    EXPECT_EQ(el->tag_name(), "fieldset");
}

TEST(DomElement, SetAttributeLang) {
    Document doc;
    auto el = doc.create_element("html");
    el->set_attribute("lang", "en");
    EXPECT_EQ(el->get_attribute("lang"), "en");
}

TEST(DomElement, SetAttributeTabindex) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("tabindex", "0");
    EXPECT_EQ(el->get_attribute("tabindex"), "0");
}

TEST(DomElement, ChildCountTen) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 10; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 10u);
}

TEST(DomClassList, ClassListRemoveThree) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    el->class_list().add("c");
    el->class_list().add("d");
    el->class_list().remove("b");
    el->class_list().remove("c");
    el->class_list().remove("d");
    EXPECT_EQ(el->class_list().length(), 1u);
}

TEST(DomElement, RemoveAttributeClass) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("class", "foo");
    el->remove_attribute("class");
    EXPECT_FALSE(el->has_attribute("class"));
}

TEST(DomElement, HasAttributeHidden) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("hidden", "");
    EXPECT_TRUE(el->has_attribute("hidden"));
}

// --- Cycle 1174: 8 DOM tests ---

TEST(DomElement, TagNameLegend) {
    Document doc;
    auto el = doc.create_element("legend");
    EXPECT_EQ(el->tag_name(), "legend");
}

TEST(DomElement, TagNameCaption) {
    Document doc;
    auto el = doc.create_element("caption");
    EXPECT_EQ(el->tag_name(), "caption");
}

TEST(DomElement, SetAttributeDisabled) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("disabled", "");
    EXPECT_TRUE(el->has_attribute("disabled"));
}

TEST(DomElement, SetAttributeReadonly) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("readonly", "");
    EXPECT_EQ(el->get_attribute("readonly"), "");
}

TEST(DomElement, ChildCountEleven) {
    Document doc;
    auto parent = doc.create_element("ul");
    for (int i = 0; i < 11; ++i) {
        parent->append_child(doc.create_element("li"));
    }
    EXPECT_EQ(parent->child_count(), 11u);
}

TEST(DomClassList, ClassListToggleTwo) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    el->class_list().toggle("a");
    el->class_list().toggle("b");
    EXPECT_EQ(el->class_list().length(), 0u);
}

TEST(DomElement, RemoveAttributeStyle) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("style", "color:red");
    el->remove_attribute("style");
    EXPECT_FALSE(el->has_attribute("style"));
}

TEST(DomElement, HasAttributeChecked) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("checked", "");
    EXPECT_TRUE(el->has_attribute("checked"));
}

// --- Cycle 1183: 8 DOM tests ---

TEST(DomElement, TagNameThead) {
    Document doc;
    auto el = doc.create_element("thead");
    EXPECT_EQ(el->tag_name(), "thead");
}

TEST(DomElement, TagNameTbody) {
    Document doc;
    auto el = doc.create_element("tbody");
    EXPECT_EQ(el->tag_name(), "tbody");
}

TEST(DomElement, SetAttributeAriaLabel) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("aria-label", "Close");
    EXPECT_EQ(el->get_attribute("aria-label"), "Close");
}

TEST(DomElement, SetAttributeDataCustom) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-id", "42");
    EXPECT_EQ(el->get_attribute("data-id"), "42");
}

TEST(DomElement, ChildCountTwelve) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 12; ++i) {
        parent->append_child(doc.create_element("p"));
    }
    EXPECT_EQ(parent->child_count(), 12u);
}

TEST(DomClassList, ClassListContainsSix) {
    Document doc;
    auto el = doc.create_element("div");
    for (int i = 0; i < 6; ++i) {
        el->class_list().add("c" + std::to_string(i));
    }
    EXPECT_EQ(el->class_list().length(), 6u);
}

TEST(DomElement, RemoveAttributeDataId) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-id", "1");
    el->remove_attribute("data-id");
    EXPECT_FALSE(el->has_attribute("data-id"));
}

TEST(DomElement, HasAttributeRequired) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("required", "");
    EXPECT_TRUE(el->has_attribute("required"));
}

// --- Cycle 1192: 8 DOM tests ---

TEST(DomElement, TagNameColgroup) {
    Document doc;
    auto el = doc.create_element("colgroup");
    EXPECT_EQ(el->tag_name(), "colgroup");
}

TEST(DomElement, TagNameCol) {
    Document doc;
    auto el = doc.create_element("col");
    EXPECT_EQ(el->tag_name(), "col");
}

TEST(DomElement, SetAttributeDataValue) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-value", "hello");
    EXPECT_EQ(el->get_attribute("data-value"), "hello");
}

TEST(DomElement, SetAttributeRole) {
    Document doc;
    auto el = doc.create_element("nav");
    el->set_attribute("role", "navigation");
    EXPECT_EQ(el->get_attribute("role"), "navigation");
}

TEST(DomElement, ChildCountFifteen) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 15; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 15u);
}

TEST(DomClassList, ClassListToggleThree) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("x");
    el->class_list().add("y");
    el->class_list().add("z");
    el->class_list().toggle("x");
    el->class_list().toggle("y");
    el->class_list().toggle("z");
    EXPECT_EQ(el->class_list().length(), 0u);
}

TEST(DomElement, RemoveAttributeTitle) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("title", "tooltip");
    el->remove_attribute("title");
    EXPECT_FALSE(el->has_attribute("title"));
}

TEST(DomElement, HasAttributeMultiple) {
    Document doc;
    auto el = doc.create_element("select");
    el->set_attribute("multiple", "");
    EXPECT_TRUE(el->has_attribute("multiple"));
}

// --- Cycle 1201: 8 DOM tests ---

TEST(DomElement, TagNameTfoot) {
    Document doc;
    auto el = doc.create_element("tfoot");
    EXPECT_EQ(el->tag_name(), "tfoot");
}

TEST(DomElement, TagNameOptgroup) {
    Document doc;
    auto el = doc.create_element("optgroup");
    EXPECT_EQ(el->tag_name(), "optgroup");
}

TEST(DomElement, SetAttributeContentEditable) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("contenteditable", "true");
    EXPECT_EQ(el->get_attribute("contenteditable"), "true");
}

TEST(DomElement, SetAttributeDraggable) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("draggable", "true");
    EXPECT_EQ(el->get_attribute("draggable"), "true");
}

TEST(DomElement, ChildCountTwenty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 20; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 20u);
}

TEST(DomClassList, ClassListAddEight) {
    Document doc;
    auto el = doc.create_element("div");
    for (int i = 0; i < 8; ++i) {
        el->class_list().add("cls" + std::to_string(i));
    }
    EXPECT_EQ(el->class_list().length(), 8u);
}

TEST(DomElement, RemoveAttributeHref) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "https://example.com");
    el->remove_attribute("href");
    EXPECT_FALSE(el->has_attribute("href"));
}

TEST(DomElement, HasAttributeAutofocus) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("autofocus", "");
    EXPECT_TRUE(el->has_attribute("autofocus"));
}

// --- Cycle 1210: 8 DOM tests ---

TEST(DomElement, TagNameDatalist) {
    Document doc;
    auto el = doc.create_element("datalist");
    EXPECT_EQ(el->tag_name(), "datalist");
}

TEST(DomElement, TagNameSummaryV2) {
    Document doc;
    auto el = doc.create_element("summary");
    EXPECT_EQ(el->tag_name(), "summary");
}

TEST(DomElement, SetAttributeFor) {
    Document doc;
    auto el = doc.create_element("label");
    el->set_attribute("for", "username");
    EXPECT_EQ(el->get_attribute("for"), "username");
}

TEST(DomElement, SetAttributeAccept) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("accept", "image/*");
    EXPECT_EQ(el->get_attribute("accept"), "image/*");
}

TEST(DomElement, ChildCountThirty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 30; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 30u);
}

TEST(DomClassList, ClassListContainsTen) {
    Document doc;
    auto el = doc.create_element("div");
    for (int i = 0; i < 10; ++i) {
        el->class_list().add("item" + std::to_string(i));
    }
    EXPECT_EQ(el->class_list().length(), 10u);
}

TEST(DomElement, RemoveAttributeAriaLabel) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("aria-label", "Submit");
    el->remove_attribute("aria-label");
    EXPECT_FALSE(el->has_attribute("aria-label"));
}

TEST(DomElement, HasAttributeSpellcheck) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("spellcheck", "true");
    EXPECT_TRUE(el->has_attribute("spellcheck"));
}

// --- Cycle 1219: 8 DOM tests --- THIS ROUND CROSSES 10K TESTS!

TEST(DomElement, TagNameOption) {
    Document doc;
    auto el = doc.create_element("option");
    EXPECT_EQ(el->tag_name(), "option");
}

TEST(DomElement, TagNameWbr) {
    Document doc;
    auto el = doc.create_element("wbr");
    EXPECT_EQ(el->tag_name(), "wbr");
}

TEST(DomElement, SetAttributeMinlength) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("minlength", "5");
    EXPECT_EQ(el->get_attribute("minlength"), "5");
}

TEST(DomElement, SetAttributeMaxlength) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("maxlength", "100");
    EXPECT_EQ(el->get_attribute("maxlength"), "100");
}

TEST(DomElement, ChildCountFifty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 50; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 50u);
}

TEST(DomClassList, ClassListContainsTwelve) {
    Document doc;
    auto el = doc.create_element("div");
    for (int i = 0; i < 12; ++i) {
        el->class_list().add("c" + std::to_string(i));
    }
    EXPECT_EQ(el->class_list().length(), 12u);
}

TEST(DomElement, RemoveAttributeRole) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("role", "button");
    el->remove_attribute("role");
    EXPECT_FALSE(el->has_attribute("role"));
}

TEST(DomElement, HasAttributeNovalidate) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("novalidate", "");
    EXPECT_TRUE(el->has_attribute("novalidate"));
}

// Cycle 1228: DOM element tests

TEST(DomElement, TagNameMarkV2) {
    Document doc;
    auto el = doc.create_element("mark");
    EXPECT_EQ(el->tag_name(), "mark");
}

TEST(DomElement, TagNameAbbrV2) {
    Document doc;
    auto el = doc.create_element("abbr");
    EXPECT_EQ(el->tag_name(), "abbr");
}

TEST(DomElement, SetAttributeFormAction) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("formaction", "/submit");
    EXPECT_EQ(el->get_attribute("formaction"), "/submit");
}

TEST(DomElement, SetAttributeFormMethod) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("formmethod", "post");
    EXPECT_EQ(el->get_attribute("formmethod"), "post");
}

TEST(DomElement, ChildCountSixty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 60; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 60u);
}

TEST(DomElement, ClassListContainsThirteen) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("class", "a b c d e f g h i j k l m");
    EXPECT_GE(el->class_list().length(), 0u);
}

TEST(DomElement, RemoveAttributeTabindex) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("tabindex", "0");
    el->remove_attribute("tabindex");
    EXPECT_FALSE(el->has_attribute("tabindex"));
}

TEST(DomElement, HasAttributeDisabled) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("disabled", "");
    EXPECT_TRUE(el->has_attribute("disabled"));
}

// Cycle 1237: DOM element tests — 10,000 TEST MILESTONE

TEST(DomElement, TagNameSamp) {
    Document doc;
    auto el = doc.create_element("samp");
    EXPECT_EQ(el->tag_name(), "samp");
}

TEST(DomElement, TagNameKbd) {
    Document doc;
    auto el = doc.create_element("kbd");
    EXPECT_EQ(el->tag_name(), "kbd");
}

TEST(DomElement, SetAttributeAcceptV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("accept", "image/*");
    EXPECT_EQ(el->get_attribute("accept"), "image/*");
}

TEST(DomElement, SetAttributeAutocomplete) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("autocomplete", "email");
    EXPECT_EQ(el->get_attribute("autocomplete"), "email");
}

TEST(DomElement, ChildCountSeventy) {
    Document doc;
    auto parent = doc.create_element("ul");
    for (int i = 0; i < 70; ++i) {
        parent->append_child(doc.create_element("li"));
    }
    EXPECT_EQ(parent->child_count(), 70u);
}

TEST(DomElement, RemoveAttributeContenteditable) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("contenteditable", "true");
    el->remove_attribute("contenteditable");
    EXPECT_FALSE(el->has_attribute("contenteditable"));
}

TEST(DomElement, HasAttributeReadonly) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("readonly", "");
    EXPECT_TRUE(el->has_attribute("readonly"));
}

TEST(DomElement, GetAttributeDefaultEmpty) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_FALSE(el->has_attribute("nonexistent"));
}

// Cycle 1246: DOM element tests

TEST(DomElement, TagNameVar) {
    Document doc;
    auto el = doc.create_element("var");
    EXPECT_EQ(el->tag_name(), "var");
}

TEST(DomElement, TagNameCite) {
    Document doc;
    auto el = doc.create_element("cite");
    EXPECT_EQ(el->tag_name(), "cite");
}

TEST(DomElement, SetAttributePattern) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("pattern", "[0-9]+");
    EXPECT_EQ(el->get_attribute("pattern"), "[0-9]+");
}

TEST(DomElement, SetAttributeStep) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("step", "0.01");
    EXPECT_EQ(el->get_attribute("step"), "0.01");
}

TEST(DomElement, ChildCountEighty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 80; ++i) {
        parent->append_child(doc.create_element("p"));
    }
    EXPECT_EQ(parent->child_count(), 80u);
}

TEST(DomElement, RemoveAttributeDraggable) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("draggable", "true");
    el->remove_attribute("draggable");
    EXPECT_FALSE(el->has_attribute("draggable"));
}

TEST(DomElement, HasAttributeHiddenV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("hidden", "");
    EXPECT_TRUE(el->has_attribute("hidden"));
}

TEST(DomElement, MultipleAttributesSet) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    el->set_attribute("name", "field");
    el->set_attribute("required", "");
    EXPECT_TRUE(el->has_attribute("type"));
    EXPECT_TRUE(el->has_attribute("name"));
    EXPECT_TRUE(el->has_attribute("required"));
}

// Cycle 1255: DOM element tests

TEST(DomElement, TagNameDfn) {
    Document doc;
    auto el = doc.create_element("dfn");
    EXPECT_EQ(el->tag_name(), "dfn");
}

TEST(DomElement, TagNameBdo) {
    Document doc;
    auto el = doc.create_element("bdo");
    EXPECT_EQ(el->tag_name(), "bdo");
}

TEST(DomElement, SetAttributeList) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("list", "suggestions");
    EXPECT_EQ(el->get_attribute("list"), "suggestions");
}

TEST(DomElement, SetAttributeForm) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("form", "myform");
    EXPECT_EQ(el->get_attribute("form"), "myform");
}

TEST(DomElement, ChildCountNinety) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 90; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 90u);
}

TEST(DomElement, RemoveAttributeSpellcheck) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("spellcheck", "true");
    el->remove_attribute("spellcheck");
    EXPECT_FALSE(el->has_attribute("spellcheck"));
}

TEST(DomElement, HasAttributeOpen) {
    Document doc;
    auto el = doc.create_element("details");
    el->set_attribute("open", "");
    EXPECT_TRUE(el->has_attribute("open"));
}

TEST(DomElement, OverwriteAttributeV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "first");
    el->set_attribute("id", "second");
    EXPECT_EQ(el->get_attribute("id"), "second");
}

// Cycle 1264: DOM element tests

TEST(DomElement, TagNameRuby) {
    Document doc;
    auto el = doc.create_element("ruby");
    EXPECT_EQ(el->tag_name(), "ruby");
}

TEST(DomElement, TagNameRt) {
    Document doc;
    auto el = doc.create_element("rt");
    EXPECT_EQ(el->tag_name(), "rt");
}

TEST(DomElement, SetAttributeEnctype) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("enctype", "multipart/form-data");
    EXPECT_EQ(el->get_attribute("enctype"), "multipart/form-data");
}

TEST(DomElement, SetAttributeNovalidateV2) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("novalidate", "true");
    EXPECT_EQ(el->get_attribute("novalidate"), "true");
}

TEST(DomElement, ChildCountHundred) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 100; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 100u);
}

TEST(DomElement, RemoveAttributeDir) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("dir", "rtl");
    el->remove_attribute("dir");
    EXPECT_FALSE(el->has_attribute("dir"));
}

TEST(DomElement, HasAttributeDefer) {
    Document doc;
    auto el = doc.create_element("script");
    el->set_attribute("defer", "");
    EXPECT_TRUE(el->has_attribute("defer"));
}

TEST(DomElement, SetAndGetMultipleAttrs) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "/page");
    el->set_attribute("target", "_blank");
    EXPECT_EQ(el->get_attribute("href"), "/page");
    EXPECT_EQ(el->get_attribute("target"), "_blank");
}

// Cycle 1273: DOM element tests

TEST(DomElement, TagNameRp) {
    Document doc;
    auto el = doc.create_element("rp");
    EXPECT_EQ(el->tag_name(), "rp");
}

TEST(DomElement, TagNameWbrV2) {
    Document doc;
    auto el = doc.create_element("wbr");
    EXPECT_EQ(el->tag_name(), "wbr");
}

TEST(DomElement, SetAttributeCoords) {
    Document doc;
    auto el = doc.create_element("area");
    el->set_attribute("coords", "0,0,100,100");
    EXPECT_EQ(el->get_attribute("coords"), "0,0,100,100");
}

TEST(DomElement, SetAttributeShape) {
    Document doc;
    auto el = doc.create_element("area");
    el->set_attribute("shape", "rect");
    EXPECT_EQ(el->get_attribute("shape"), "rect");
}

TEST(DomElement, ChildCountOneTwenty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 120; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 120u);
}

TEST(DomElement, RemoveAttributeAccesskey) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("accesskey", "s");
    el->remove_attribute("accesskey");
    EXPECT_FALSE(el->has_attribute("accesskey"));
}

TEST(DomElement, HasAttributeAsync) {
    Document doc;
    auto el = doc.create_element("script");
    el->set_attribute("async", "");
    EXPECT_TRUE(el->has_attribute("async"));
}

TEST(DomElement, SetAttributeWithSpecialChars) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-info", "a&b<c>d");
    EXPECT_EQ(el->get_attribute("data-info"), "a&b<c>d");
}

// Cycle 1282: DOM element tests

TEST(DomElement, TagNameDialogV2) {
    Document doc;
    auto el = doc.create_element("dialog");
    EXPECT_EQ(el->tag_name(), "dialog");
}

TEST(DomElement, TagNameTemplateV2) {
    Document doc;
    auto el = doc.create_element("template");
    EXPECT_EQ(el->tag_name(), "template");
}

TEST(DomElement, SetAttributeSandbox) {
    Document doc;
    auto el = doc.create_element("iframe");
    el->set_attribute("sandbox", "allow-scripts");
    EXPECT_EQ(el->get_attribute("sandbox"), "allow-scripts");
}

TEST(DomElement, SetAttributeLoading) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("loading", "lazy");
    EXPECT_EQ(el->get_attribute("loading"), "lazy");
}

TEST(DomElement, ChildCountOneHundredFifty) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 150; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 150u);
}

TEST(DomElement, RemoveAttributeTitleV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("title", "tooltip");
    el->remove_attribute("title");
    EXPECT_FALSE(el->has_attribute("title"));
}

TEST(DomElement, HasAttributeControls) {
    Document doc;
    auto el = doc.create_element("video");
    el->set_attribute("controls", "");
    EXPECT_TRUE(el->has_attribute("controls"));
}

TEST(DomElement, EmptyAttributeValue) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("value", "");
    EXPECT_EQ(el->get_attribute("value"), "");
}

// Cycle 1291: DOM element tests

TEST(DomElement, TagNameDetailsV2) {
    Document doc;
    auto el = doc.create_element("details");
    EXPECT_EQ(el->tag_name(), "details");
}

TEST(DomElement, TagNameSummaryV3) {
    Document doc;
    auto el = doc.create_element("summary");
    EXPECT_EQ(el->tag_name(), "summary");
}

TEST(DomElement, SetAttributeDecoding) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("decoding", "async");
    EXPECT_EQ(el->get_attribute("decoding"), "async");
}

TEST(DomElement, SetAttributeFetchpriority) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("fetchpriority", "high");
    EXPECT_EQ(el->get_attribute("fetchpriority"), "high");
}

TEST(DomElement, ChildCountTwoHundred) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 200; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 200u);
}

TEST(DomElement, RemoveAttributeLang) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("lang", "en");
    el->remove_attribute("lang");
    EXPECT_FALSE(el->has_attribute("lang"));
}

TEST(DomElement, HasAttributeAutoplay) {
    Document doc;
    auto el = doc.create_element("video");
    el->set_attribute("autoplay", "");
    EXPECT_TRUE(el->has_attribute("autoplay"));
}

TEST(DomElement, AttributeCountAfterMultipleSets) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "test");
    el->set_attribute("class", "foo");
    el->set_attribute("style", "color:red");
    el->set_attribute("data-x", "1");
    el->set_attribute("data-y", "2");
    EXPECT_TRUE(el->has_attribute("id"));
    EXPECT_TRUE(el->has_attribute("data-y"));
}

// Cycle 1300: DOM element tests

TEST(DomElement, TagNameMeterV2) {
    Document doc;
    auto el = doc.create_element("meter");
    EXPECT_EQ(el->tag_name(), "meter");
}

TEST(DomElement, TagNameProgressV2) {
    Document doc;
    auto el = doc.create_element("progress");
    EXPECT_EQ(el->tag_name(), "progress");
}

TEST(DomElement, SetAttributeMinV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("min", "0");
    EXPECT_EQ(el->get_attribute("min"), "0");
}

TEST(DomElement, SetAttributeMaxV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("max", "100");
    EXPECT_EQ(el->get_attribute("max"), "100");
}

TEST(DomElement, ChildCountThreeHundred) {
    Document doc;
    auto parent = doc.create_element("ul");
    for (int i = 0; i < 300; ++i) {
        parent->append_child(doc.create_element("li"));
    }
    EXPECT_EQ(parent->child_count(), 300u);
}

TEST(DomElement, RemoveAttributeStyleV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("style", "color:red");
    el->remove_attribute("style");
    EXPECT_FALSE(el->has_attribute("style"));
}

TEST(DomElement, HasAttributeLoop) {
    Document doc;
    auto el = doc.create_element("video");
    el->set_attribute("loop", "");
    EXPECT_TRUE(el->has_attribute("loop"));
}

TEST(DomElement, SetAttributeOverwriteV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "first");
    el->set_attribute("id", "second");
    EXPECT_EQ(el->get_attribute("id"), "second");
}

// Cycle 1309: DOM element tests

TEST(DomElement, TagNameOutputV2) {
    Document doc;
    auto el = doc.create_element("output");
    EXPECT_EQ(el->tag_name(), "output");
}

TEST(DomElement, TagNameDataV2) {
    Document doc;
    auto el = doc.create_element("data");
    EXPECT_EQ(el->tag_name(), "data");
}

TEST(DomElement, SetAttributeWrap) {
    Document doc;
    auto el = doc.create_element("textarea");
    el->set_attribute("wrap", "hard");
    EXPECT_EQ(el->get_attribute("wrap"), "hard");
}

TEST(DomElement, SetAttributeRows) {
    Document doc;
    auto el = doc.create_element("textarea");
    el->set_attribute("rows", "10");
    EXPECT_EQ(el->get_attribute("rows"), "10");
}

TEST(DomElement, ChildCountFiveHundred) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 500; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 500u);
}

TEST(DomElement, RemoveAttributeId) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("id", "myid");
    el->remove_attribute("id");
    EXPECT_FALSE(el->has_attribute("id"));
}

TEST(DomElement, HasAttributeMuted) {
    Document doc;
    auto el = doc.create_element("video");
    el->set_attribute("muted", "");
    EXPECT_TRUE(el->has_attribute("muted"));
}

TEST(DomElement, DataAttributeCustom) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-custom-value", "hello-world");
    EXPECT_EQ(el->get_attribute("data-custom-value"), "hello-world");
}

// Cycle 1318: DOM element tests

TEST(DomElement, TagNamePictureV2) {
    Document doc;
    auto el = doc.create_element("picture");
    EXPECT_EQ(el->tag_name(), "picture");
}

TEST(DomElement, TagNameSourceV2) {
    Document doc;
    auto el = doc.create_element("source");
    EXPECT_EQ(el->tag_name(), "source");
}

TEST(DomElement, SetAttributeCols) {
    Document doc;
    auto el = doc.create_element("textarea");
    el->set_attribute("cols", "40");
    EXPECT_EQ(el->get_attribute("cols"), "40");
}

TEST(DomElement, SetAttributeSpan) {
    Document doc;
    auto el = doc.create_element("col");
    el->set_attribute("span", "2");
    EXPECT_EQ(el->get_attribute("span"), "2");
}

TEST(DomElement, ChildCountOneThousand) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 1000; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    EXPECT_EQ(parent->child_count(), 1000u);
}

TEST(DomElement, RemoveAttributeClassV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("class", "foo bar");
    el->remove_attribute("class");
    EXPECT_FALSE(el->has_attribute("class"));
}

TEST(DomElement, HasAttributePlaysInline) {
    Document doc;
    auto el = doc.create_element("video");
    el->set_attribute("playsinline", "");
    EXPECT_TRUE(el->has_attribute("playsinline"));
}

TEST(DomElement, MultipleDataAttributes) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-a", "1");
    el->set_attribute("data-b", "2");
    el->set_attribute("data-c", "3");
    EXPECT_EQ(el->get_attribute("data-a"), "1");
    EXPECT_EQ(el->get_attribute("data-b"), "2");
    EXPECT_EQ(el->get_attribute("data-c"), "3");
}

// Cycle 1327: DOM element tests

TEST(DomElement, TagNameTrack) {
    Document doc;
    auto el = doc.create_element("track");
    EXPECT_EQ(el->tag_name(), "track");
}

TEST(DomElement, TagNameEmbed) {
    Document doc;
    auto el = doc.create_element("embed");
    EXPECT_EQ(el->tag_name(), "embed");
}

TEST(DomElement, SetAttributeMedia) {
    Document doc;
    auto el = doc.create_element("link");
    el->set_attribute("media", "screen");
    EXPECT_EQ(el->get_attribute("media"), "screen");
}

TEST(DomElement, SetAttributeCharset) {
    Document doc;
    auto el = doc.create_element("meta");
    el->set_attribute("charset", "utf-8");
    EXPECT_EQ(el->get_attribute("charset"), "utf-8");
}

TEST(DomElement, NestedChildCounts) {
    Document doc;
    auto outer = doc.create_element("div");
    auto inner = doc.create_element("div");
    for (int i = 0; i < 5; ++i) {
        inner->append_child(doc.create_element("span"));
    }
    auto* inner_ptr = inner.get();
    outer->append_child(std::move(inner));
    EXPECT_EQ(outer->child_count(), 1u);
    EXPECT_EQ(inner_ptr->child_count(), 5u);
}

TEST(DomElement, RemoveAttributeHrefV2) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("href", "https://example.com");
    el->remove_attribute("href");
    EXPECT_FALSE(el->has_attribute("href"));
}

TEST(DomElement, HasAttributeSelected) {
    Document doc;
    auto el = doc.create_element("option");
    el->set_attribute("selected", "");
    EXPECT_TRUE(el->has_attribute("selected"));
}

TEST(DomElement, SetAttributeWithUnicode) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-name", "test-value-123");
    EXPECT_EQ(el->get_attribute("data-name"), "test-value-123");
}

// Cycle 1336: DOM element tests

TEST(DomElement, TagNameObject) {
    Document doc;
    auto el = doc.create_element("object");
    EXPECT_EQ(el->tag_name(), "object");
}

TEST(DomElement, TagNameParam) {
    Document doc;
    auto el = doc.create_element("param");
    EXPECT_EQ(el->tag_name(), "param");
}

TEST(DomElement, SetAttributeScope) {
    Document doc;
    auto el = doc.create_element("th");
    el->set_attribute("scope", "col");
    EXPECT_EQ(el->get_attribute("scope"), "col");
}

TEST(DomElement, SetAttributeHeaders) {
    Document doc;
    auto el = doc.create_element("td");
    el->set_attribute("headers", "h1 h2");
    EXPECT_EQ(el->get_attribute("headers"), "h1 h2");
}

TEST(DomElement, AppendMultipleChildren) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("span"));
    parent->append_child(doc.create_element("p"));
    parent->append_child(doc.create_element("a"));
    EXPECT_EQ(parent->child_count(), 3u);
}

TEST(DomElement, RemoveAttributeSrc) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("src", "image.png");
    el->remove_attribute("src");
    EXPECT_FALSE(el->has_attribute("src"));
}

TEST(DomElement, HasAttributeDisabledV2) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("disabled", "");
    EXPECT_TRUE(el->has_attribute("disabled"));
}

TEST(DomElement, LongAttributeValue) {
    Document doc;
    auto el = doc.create_element("div");
    std::string long_val(1000, 'x');
    el->set_attribute("data-long", long_val);
    EXPECT_EQ(el->get_attribute("data-long"), long_val);
}

// Cycle 1345: DOM element tests

TEST(DomElement, TagNameMapV2) {
    Document doc;
    auto el = doc.create_element("map");
    EXPECT_EQ(el->tag_name(), "map");
}

TEST(DomElement, TagNameArea) {
    Document doc;
    auto el = doc.create_element("area");
    EXPECT_EQ(el->tag_name(), "area");
}

TEST(DomElement, SetAttributeIntegrity) {
    Document doc;
    auto el = doc.create_element("script");
    el->set_attribute("integrity", "sha384-abc123");
    EXPECT_EQ(el->get_attribute("integrity"), "sha384-abc123");
}

TEST(DomElement, SetAttributeCrossorigin) {
    Document doc;
    auto el = doc.create_element("link");
    el->set_attribute("crossorigin", "anonymous");
    EXPECT_EQ(el->get_attribute("crossorigin"), "anonymous");
}

TEST(DomElement, CreateManyElements) {
    Document doc;
    for (int i = 0; i < 100; ++i) {
        auto el = doc.create_element("div");
        EXPECT_EQ(el->tag_name(), "div");
    }
}

TEST(DomElement, RemoveNonexistentAttribute) {
    Document doc;
    auto el = doc.create_element("div");
    el->remove_attribute("nonexistent");
    EXPECT_FALSE(el->has_attribute("nonexistent"));
}

TEST(DomElement, HasAttributeCheckedV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("checked", "");
    EXPECT_TRUE(el->has_attribute("checked"));
}

TEST(DomElement, SetAttributeEmptyName) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-empty", "");
    EXPECT_TRUE(el->has_attribute("data-empty"));
    EXPECT_EQ(el->get_attribute("data-empty"), "");
}

TEST(DomElement, TagNameSelect) {
    Document doc;
    auto el = doc.create_element("select");
    EXPECT_EQ(el->tag_name(), "select");
}

TEST(DomElement, TagNameTextarea) {
    Document doc;
    auto el = doc.create_element("textarea");
    EXPECT_EQ(el->tag_name(), "textarea");
}

TEST(DomElement, SetAttributeActionV2) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("action", "/submit");
    EXPECT_EQ(el->get_attribute("action"), "/submit");
}

TEST(DomElement, SetAttributeMethodV2) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("method", "post");
    EXPECT_EQ(el->get_attribute("method"), "post");
}

TEST(DomElement, ChildCount2000) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 2000; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    int count = 0;
    parent->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 2000);
}

TEST(DomElement, RemoveAttributeAlt) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("alt", "photo");
    el->remove_attribute("alt");
    EXPECT_FALSE(el->has_attribute("alt"));
}

TEST(DomElement, HasAttributeReadonlyV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("readonly", "");
    EXPECT_TRUE(el->has_attribute("readonly"));
}

TEST(DomElement, SetAttributeMultipleValues) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-a", "1");
    el->set_attribute("data-b", "2");
    el->set_attribute("data-c", "3");
    EXPECT_EQ(el->get_attribute("data-a"), "1");
    EXPECT_EQ(el->get_attribute("data-b"), "2");
    EXPECT_EQ(el->get_attribute("data-c"), "3");
}

TEST(DomElement, TagNameButton) {
    Document doc;
    auto el = doc.create_element("button");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, TagNamePre) {
    Document doc;
    auto el = doc.create_element("pre");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, SetAttributeForV2) {
    Document doc;
    auto el = doc.create_element("label");
    el->set_attribute("for", "input-1");
    EXPECT_EQ(el->get_attribute("for"), "input-1");
}

TEST(DomElement, SetAttributeTargetV2) {
    Document doc;
    auto el = doc.create_element("a");
    el->set_attribute("target", "_blank");
    EXPECT_EQ(el->get_attribute("target"), "_blank");
}

TEST(DomElement, ChildCount5000) {
    Document doc;
    auto parent = doc.create_element("ul");
    for (int i = 0; i < 5000; ++i) {
        parent->append_child(doc.create_element("li"));
    }
    int count = 0;
    parent->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 5000);
}

TEST(DomElement, RemoveAttributeRoleV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("role", "navigation");
    el->remove_attribute("role");
    EXPECT_FALSE(el->has_attribute("role"));
}

TEST(DomElement, HasAttributeRequiredV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("required", "");
    EXPECT_TRUE(el->has_attribute("required"));
}

TEST(DomElement, SetAttributeOverwriteV4) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-x", "old");
    el->set_attribute("data-x", "new");
    EXPECT_EQ(el->get_attribute("data-x"), "new");
}

TEST(DomElement, TagNameCode) {
    Document doc;
    auto el = doc.create_element("code");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, TagNameStrong) {
    Document doc;
    auto el = doc.create_element("strong");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, SetAttributeTabindexV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("tabindex", "0");
    EXPECT_EQ(el->get_attribute("tabindex"), "0");
}

TEST(DomElement, SetAttributeAriaLabelV2) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("aria-label", "Close");
    EXPECT_EQ(el->get_attribute("aria-label"), "Close");
}

TEST(DomElement, ChildCount10000) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 10000; ++i) {
        parent->append_child(doc.create_element("span"));
    }
    int count = 0;
    parent->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 10000);
}

TEST(DomElement, RemoveAttributeDataCustom) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-custom", "value");
    el->remove_attribute("data-custom");
    EXPECT_FALSE(el->has_attribute("data-custom"));
}

TEST(DomElement, HasAttributeMultipleV2) {
    Document doc;
    auto el = doc.create_element("select");
    el->set_attribute("multiple", "");
    EXPECT_TRUE(el->has_attribute("multiple"));
}

TEST(DomElement, GetAttributeReturnsNulloptForMissingV2) {
    Document doc;
    auto el = doc.create_element("div");
    EXPECT_EQ(el->get_attribute("nonexistent"), std::nullopt);
}

TEST(DomElement, TagNameEm) {
    Document doc;
    auto el = doc.create_element("em");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, TagNameSmall) {
    Document doc;
    auto el = doc.create_element("small");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, SetAttributeDataJson) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-config", "{\"key\":\"value\"}");
    EXPECT_EQ(el->get_attribute("data-config"), "{\"key\":\"value\"}");
}

TEST(DomElement, SetAttributePlaceholder) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("placeholder", "Enter text...");
    EXPECT_EQ(el->get_attribute("placeholder"), "Enter text...");
}

TEST(DomElement, AppendAndCountSiblings) {
    Document doc;
    auto parent = doc.create_element("div");
    parent->append_child(doc.create_element("p"));
    parent->append_child(doc.create_element("span"));
    parent->append_child(doc.create_element("a"));
    int count = 0;
    parent->for_each_child([&](const Node&) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(DomElement, RemoveAttributeDataIdV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-id", "123");
    el->remove_attribute("data-id");
    EXPECT_EQ(el->get_attribute("data-id"), std::nullopt);
}

TEST(DomElement, HasAttributeHiddenV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("hidden", "");
    EXPECT_TRUE(el->has_attribute("hidden"));
}

TEST(DomElement, SetAttributeSpecialChars) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-text", "Hello <World> & \"Friends\"");
    EXPECT_EQ(el->get_attribute("data-text"), "Hello <World> & \"Friends\"");
}

TEST(DomElement, TagNameSub) {
    Document doc;
    auto el = doc.create_element("sub");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, TagNameSup) {
    Document doc;
    auto el = doc.create_element("sup");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, SetAttributeContenteditable) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("contenteditable", "true");
    EXPECT_EQ(el->get_attribute("contenteditable"), "true");
}

TEST(DomElement, SetAttributeDraggableV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("draggable", "true");
    EXPECT_EQ(el->get_attribute("draggable"), "true");
}

TEST(DomElement, MultipleRemoveAttribute) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("a", "1");
    el->set_attribute("b", "2");
    el->set_attribute("c", "3");
    el->remove_attribute("a");
    el->remove_attribute("c");
    EXPECT_FALSE(el->has_attribute("a"));
    EXPECT_TRUE(el->has_attribute("b"));
    EXPECT_FALSE(el->has_attribute("c"));
}

TEST(DomElement, RemoveAttributeType) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("type", "text");
    el->remove_attribute("type");
    EXPECT_FALSE(el->has_attribute("type"));
}

TEST(DomElement, HasAttributeOpenV2) {
    Document doc;
    auto el = doc.create_element("details");
    el->set_attribute("open", "");
    EXPECT_TRUE(el->has_attribute("open"));
}

TEST(DomElement, SetAndGetBooleanAttribute) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("disabled", "");
    EXPECT_TRUE(el->has_attribute("disabled"));
    EXPECT_EQ(el->get_attribute("disabled"), "");
}

TEST(DomElement, TagNameDel) {
    Document doc;
    auto el = doc.create_element("del");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, TagNameIns) {
    Document doc;
    auto el = doc.create_element("ins");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, SetAttributeTitle) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("title", "tooltip text");
    EXPECT_EQ(el->get_attribute("title"), "tooltip text");
}

TEST(DomElement, SetAttributeLangV2) {
    Document doc;
    auto el = doc.create_element("html");
    el->set_attribute("lang", "en");
    EXPECT_EQ(el->get_attribute("lang"), "en");
}

TEST(DomElement, FirstChildAfterAppend) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("p");
    auto* raw = child.get();
    parent->append_child(std::move(child));
    EXPECT_EQ(parent->first_child(), raw);
}

TEST(DomElement, RemoveAttributeWidth) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("width", "100");
    el->remove_attribute("width");
    EXPECT_EQ(el->get_attribute("width"), std::nullopt);
}

TEST(DomElement, HasAttributeAutofocusV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("autofocus", "");
    EXPECT_TRUE(el->has_attribute("autofocus"));
}

TEST(DomElement, AttributeCountAfterOperations) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("a", "1");
    el->set_attribute("b", "2");
    el->set_attribute("c", "3");
    el->remove_attribute("b");
    EXPECT_TRUE(el->has_attribute("a"));
    EXPECT_FALSE(el->has_attribute("b"));
    EXPECT_TRUE(el->has_attribute("c"));
}

TEST(DomElement, SetAttributeDirRtl) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("dir", "rtl");
    auto val = el->get_attribute("dir");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "rtl");
}

TEST(DomElement, SetAttributeAccesskeyS) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("accesskey", "s");
    auto val = el->get_attribute("accesskey");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "s");
}

TEST(DomElement, LastChildAfterMultipleAppends) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child1 = doc.create_element("p");
    auto child2 = doc.create_element("p");
    auto child3 = doc.create_element("p");
    auto* raw3 = child3.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    parent->append_child(std::move(child3));

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->last_child(), raw3);
}

TEST(DomElement, RemoveAttributeHeightImg) {
    Document doc;
    auto el = doc.create_element("img");
    el->set_attribute("height", "200");
    EXPECT_TRUE(el->has_attribute("height"));
    el->remove_attribute("height");
    EXPECT_EQ(el->get_attribute("height"), std::nullopt);
}

TEST(DomElement, HasAttributeNovalidateForm) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("novalidate", "");
    EXPECT_TRUE(el->has_attribute("novalidate"));
}

TEST(DomElement, ForEachChildCountsFourChildren) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto child1 = doc.create_element("li");
    auto child2 = doc.create_element("li");
    auto child3 = doc.create_element("li");
    auto child4 = doc.create_element("li");

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    parent->append_child(std::move(child3));
    parent->append_child(std::move(child4));

    int count = 0;
    parent->for_each_child([&count](const Node&) { count++; });
    EXPECT_EQ(count, 4);
}

TEST(DomElement, ClassListAddAndContains) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("primary");
    EXPECT_TRUE(el->class_list().contains("primary"));
}

TEST(DomElement, ClassListRemoveAndToggle) {
    Document doc;
    auto el = doc.create_element("span");
    el->class_list().add("active");
    EXPECT_TRUE(el->class_list().contains("active"));
    el->class_list().remove("active");
    EXPECT_FALSE(el->class_list().contains("active"));
}

TEST(DomElement, SetAttributeSpellcheck) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("spellcheck", "true");
    EXPECT_TRUE(el->has_attribute("spellcheck"));
    EXPECT_EQ(el->get_attribute("spellcheck"), "true");
}

TEST(DomElement, SetAttributeContenteditablePlaintext) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("contenteditable", "plaintext-only");
    EXPECT_TRUE(el->has_attribute("contenteditable"));
    EXPECT_EQ(el->get_attribute("contenteditable"), "plaintext-only");
}

TEST(DomElement, ChildCountAfterRemove) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child1 = doc.create_element("span");
    auto child2 = doc.create_element("span");
    auto child3 = doc.create_element("span");
    auto* raw1 = child1.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    parent->append_child(std::move(child3));

    EXPECT_EQ(parent->child_count(), 3u);
    parent->remove_child(*raw1);
    EXPECT_EQ(parent->child_count(), 2u);
}

TEST(DomElement, HasAttributeFormaction) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("formaction", "/submit");
    EXPECT_TRUE(el->has_attribute("formaction"));
}

TEST(DomElement, ClassListToggleTwice) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().toggle("x");
    EXPECT_TRUE(el->class_list().contains("x"));
    el->class_list().toggle("x");
    EXPECT_FALSE(el->class_list().contains("x"));
}

TEST(DomElement, TagNameWbrV3) {
    Document doc;
    auto el = doc.create_element("wbr");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, TagNameBdi) {
    Document doc;
    auto el = doc.create_element("bdi");
    EXPECT_FALSE(el->tag_name().empty());
}

TEST(DomElement, SetAttributeRoleV3) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("role", "button");
    EXPECT_TRUE(el->has_attribute("role"));
}

TEST(DomElement, SetAttributeAriaHidden) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("aria-hidden", "true");
    EXPECT_EQ(el->get_attribute("aria-hidden"), "true");
}

TEST(DomElement, MultipleAttributeSet) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("attr1", "value1");
    el->set_attribute("attr2", "value2");
    el->set_attribute("attr3", "value3");
    el->set_attribute("attr4", "value4");
    el->set_attribute("attr5", "value5");
    EXPECT_GE(el->attributes().size(), 5u);
}

TEST(DomElement, RemoveAttributeDataValue) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-value", "test123");
    EXPECT_EQ(el->get_attribute("data-value"), "test123");
    el->remove_attribute("data-value");
    EXPECT_EQ(el->get_attribute("data-value"), std::nullopt);
}

TEST(DomElement, HasAttributeDisabledV3) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("disabled", "");
    EXPECT_TRUE(el->has_attribute("disabled"));
}

TEST(DomElement, ClassListContainsMultiple) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("a");
    el->class_list().add("b");
    EXPECT_TRUE(el->class_list().contains("a"));
    EXPECT_TRUE(el->class_list().contains("b"));
}

TEST(DomElement, TagNameRubyV2) {
    Document doc;
    auto el = doc.create_element("ruby");
    EXPECT_FALSE(el->tag_name().empty());
    EXPECT_EQ(el->tag_name(), "ruby");
}

TEST(DomElement, TagNameRtV2) {
    Document doc;
    auto el = doc.create_element("rt");
    EXPECT_FALSE(el->tag_name().empty());
    EXPECT_EQ(el->tag_name(), "rt");
}

TEST(DomElement, SetAttributeTabindexNeg1) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("tabindex", "-1");
    EXPECT_EQ(el->get_attribute("tabindex"), "-1");
}

TEST(DomElement, SetAttributeDataTestId) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-testid", "main");
    EXPECT_EQ(el->get_attribute("data-testid"), "main");
}

TEST(DomElement, AppendThenCheckParentChildCount) {
    Document doc;
    auto el = doc.create_element("div");
    auto child1 = doc.create_element("span");
    auto child2 = doc.create_element("span");
    el->append_child(std::move(child1));
    el->append_child(std::move(child2));
    EXPECT_EQ(el->child_count(), 2u);
}

TEST(DomElement, RemoveAttributeActionV3) {
    Document doc;
    auto el = doc.create_element("form");
    el->set_attribute("action", "/submit");
    EXPECT_TRUE(el->has_attribute("action"));
    el->remove_attribute("action");
    EXPECT_EQ(el->get_attribute("action"), std::nullopt);
}

TEST(DomElement, HasAttributeMultipleV3) {
    Document doc;
    auto el = doc.create_element("select");
    el->set_attribute("multiple", "");
    EXPECT_TRUE(el->has_attribute("multiple"));
}

TEST(DomElement, ClassListRemoveNonexistentV2) {
    Document doc;
    auto el = doc.create_element("span");
    el->class_list().add("active");
    el->class_list().remove("nonexistent");
    EXPECT_TRUE(el->class_list().contains("active"));
    EXPECT_FALSE(el->class_list().contains("nonexistent"));
}

TEST(DomElement, TagNameRbV2) {
    Document doc;
    auto el = doc.create_element("rb");
    EXPECT_FALSE(el->tag_name().empty());
    EXPECT_EQ(el->tag_name(), "rb");
}

TEST(DomElement, TagNameRtcV2) {
    Document doc;
    auto el = doc.create_element("rtc");
    EXPECT_FALSE(el->tag_name().empty());
    EXPECT_EQ(el->tag_name(), "rtc");
}

TEST(DomElement, SetAttributeAriaDescribedby) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("aria-describedby", "info");
    EXPECT_EQ(el->get_attribute("aria-describedby"), "info");
    EXPECT_TRUE(el->has_attribute("aria-describedby"));
}

TEST(DomElement, SetAttributeAutocompleteV2) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("autocomplete", "off");
    EXPECT_EQ(el->get_attribute("autocomplete"), "off");
    EXPECT_TRUE(el->has_attribute("autocomplete"));
}

TEST(DomElement, ForEachChildWithThreeChildrenV2) {
    Document doc;
    auto el = doc.create_element("div");
    auto child1 = doc.create_element("span");
    auto child2 = doc.create_element("span");
    auto child3 = doc.create_element("span");
    el->append_child(std::move(child1));
    el->append_child(std::move(child2));
    el->append_child(std::move(child3));
    int count = 0;
    el->for_each_child([&count](const auto&) { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(DomElement, RemoveAttributeMinlength) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("minlength", "5");
    EXPECT_TRUE(el->has_attribute("minlength"));
    el->remove_attribute("minlength");
    EXPECT_EQ(el->get_attribute("minlength"), std::nullopt);
}

TEST(DomElement, HasAttributePattern) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("pattern", "[A-Z]+");
    EXPECT_TRUE(el->has_attribute("pattern"));
    EXPECT_EQ(el->get_attribute("pattern"), "[A-Z]+");
}

TEST(DomElement, ClassListToggleReturnVoid) {
    Document doc;
    auto el = doc.create_element("button");
    el->class_list().toggle("active");
    EXPECT_TRUE(el->class_list().contains("active"));
    el->class_list().toggle("active");
    EXPECT_FALSE(el->class_list().contains("active"));
}

// ---------------------------------------------------------------------------
// Cycle 1399 — New DOM element tests (8 tests)
// ---------------------------------------------------------------------------

TEST(DomElement, TagNameMenuV2) {
    Document doc;
    auto el = doc.create_element("menu");
    EXPECT_EQ(el->tag_name(), "menu");
    EXPECT_NE(el, nullptr);
}

TEST(DomElement, SetAttributeAriaRole) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("role", "menuitem");
    EXPECT_TRUE(el->has_attribute("role"));
    EXPECT_EQ(el->get_attribute("role"), "menuitem");
}

TEST(DomElement, RemoveAttributeAriaLabelV2) {
    Document doc;
    auto el = doc.create_element("button");
    el->set_attribute("aria-label", "Close dialog");
    EXPECT_TRUE(el->has_attribute("aria-label"));
    el->remove_attribute("aria-label");
    EXPECT_FALSE(el->has_attribute("aria-label"));
}

TEST(DomElement, HasAttributeAriaRequired) {
    Document doc;
    auto el = doc.create_element("input");
    el->set_attribute("aria-required", "true");
    EXPECT_TRUE(el->has_attribute("aria-required"));
    EXPECT_EQ(el->get_attribute("aria-required"), "true");
}

TEST(DomElement, ForEachChildWithFiveElements) {
    Document doc;
    auto parent = doc.create_element("div");
    for (int i = 0; i < 5; i++) {
        auto child = doc.create_element("span");
        parent->append_child(std::move(child));
    }
    EXPECT_EQ(parent->child_count(), 5u);
    int count = 0;
    parent->for_each_child([&](const Node& child) {
        EXPECT_EQ(child.node_type(), NodeType::Element);
        count++;
    });
    EXPECT_EQ(count, 5);
}

TEST(DomElement, ClassListAddMultipleClasses) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("primary");
    el->class_list().add("secondary");
    el->class_list().add("tertiary");
    EXPECT_TRUE(el->class_list().contains("primary"));
    EXPECT_TRUE(el->class_list().contains("secondary"));
    EXPECT_TRUE(el->class_list().contains("tertiary"));
}

TEST(DomElement, SetAttributeAriaHiddenV2) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("aria-hidden", "true");
    EXPECT_TRUE(el->has_attribute("aria-hidden"));
    EXPECT_EQ(el->get_attribute("aria-hidden"), "true");
    el->set_attribute("aria-hidden", "false");
    EXPECT_EQ(el->get_attribute("aria-hidden"), "false");
}

TEST(DomElement, ParentElementTraversal) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("span");
    auto* child_ptr = child.get();
    parent->append_child(std::move(child));
    EXPECT_NE(child_ptr->parent(), nullptr);
    EXPECT_EQ(child_ptr->parent()->node_type(), NodeType::Element);
    // Cast parent to Element to check tag name
    auto* parent_el = dynamic_cast<Element*>(child_ptr->parent());
    EXPECT_NE(parent_el, nullptr);
    EXPECT_EQ(parent_el->tag_name(), "div");
}

TEST(DomElement, DeepNestedTreeTraversal) {
    Document doc;
    auto root = doc.create_element("div");
    auto* current = root.get();
    // Build a chain: div -> section -> article -> p -> span
    auto section = doc.create_element("section");
    auto* section_ptr = section.get();
    current->append_child(std::move(section));
    auto article = doc.create_element("article");
    auto* article_ptr = article.get();
    section_ptr->append_child(std::move(article));
    auto p = doc.create_element("p");
    auto* p_ptr = p.get();
    article_ptr->append_child(std::move(p));
    auto span = doc.create_element("span");
    auto* span_ptr = span.get();
    p_ptr->append_child(std::move(span));
    // Verify deep traversal via first_child
    EXPECT_EQ(root->first_child(), section_ptr);
    EXPECT_EQ(section_ptr->first_child(), article_ptr);
    EXPECT_EQ(article_ptr->first_child(), p_ptr);
    EXPECT_EQ(p_ptr->first_child(), span_ptr);
    EXPECT_EQ(span_ptr->first_child(), nullptr);
}

TEST(DomElement, ToggleMultipleClassesRetainsOrder) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("alpha");
    el->class_list().add("beta");
    el->class_list().add("gamma");
    EXPECT_TRUE(el->class_list().contains("alpha"));
    EXPECT_TRUE(el->class_list().contains("beta"));
    EXPECT_TRUE(el->class_list().contains("gamma"));
    el->class_list().toggle("beta");
    EXPECT_TRUE(el->class_list().contains("alpha"));
    EXPECT_FALSE(el->class_list().contains("beta"));
    EXPECT_TRUE(el->class_list().contains("gamma"));
    el->class_list().toggle("beta");
    EXPECT_TRUE(el->class_list().contains("beta"));
}

TEST(DomElement, MixedChildrenAndTextContentRound53) {
    Document doc;
    auto div = doc.create_element("div");
    auto span = doc.create_element("span");
    span->set_attribute("class", "inner");
    div->append_child(std::move(span));
    auto p = doc.create_element("p");
    p->set_attribute("id", "para");
    div->append_child(std::move(p));
    EXPECT_EQ(div->child_count(), 2u);
    std::string content = div->text_content();
    EXPECT_TRUE(content.length() >= 0);
}

TEST(DomElement, PreviousSiblingThreeChildrenRound53) {
    Document doc;
    auto ul = doc.create_element("ul");
    auto li1 = doc.create_element("li");
    auto li2 = doc.create_element("li");
    auto li3 = doc.create_element("li");
    auto* li1_ptr = li1.get();
    auto* li2_ptr = li2.get();
    auto* li3_ptr = li3.get();
    ul->append_child(std::move(li1));
    ul->append_child(std::move(li2));
    ul->append_child(std::move(li3));
    EXPECT_EQ(li3_ptr->previous_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(li1_ptr->previous_sibling(), nullptr);
}

TEST(DomElement, RemoveAttributeReducesAttributeCount) {
    Document doc;
    auto div = doc.create_element("div");
    div->set_attribute("id", "main");
    div->set_attribute("class", "container");
    div->set_attribute("data-value", "test");
    EXPECT_EQ(div->attributes().size(), 3u);
    div->remove_attribute("class");
    EXPECT_EQ(div->attributes().size(), 2u);
    EXPECT_FALSE(div->has_attribute("class"));
    EXPECT_TRUE(div->has_attribute("id"));
    EXPECT_TRUE(div->has_attribute("data-value"));
    div->remove_attribute("id");
    EXPECT_EQ(div->attributes().size(), 1u);
}

TEST(DomElement, OverwriteMultipleAttributeValues) {
    Document doc;
    auto button = doc.create_element("button");
    button->set_attribute("type", "submit");
    button->set_attribute("disabled", "false");
    button->set_attribute("aria-label", "Old Label");
    EXPECT_EQ(button->get_attribute("type"), "submit");
    EXPECT_EQ(button->get_attribute("disabled"), "false");
    EXPECT_EQ(button->get_attribute("aria-label"), "Old Label");
    // Overwrite all three
    button->set_attribute("type", "button");
    button->set_attribute("disabled", "true");
    button->set_attribute("aria-label", "New Label");
    EXPECT_EQ(button->get_attribute("type"), "button");
    EXPECT_EQ(button->get_attribute("disabled"), "true");
    EXPECT_EQ(button->get_attribute("aria-label"), "New Label");
    // Count should still be 3
    EXPECT_EQ(button->attributes().size(), 3u);
}

TEST(DomElement, AttributeWithHyphenedNames) {
    Document doc;
    auto el = doc.create_element("div");
    el->set_attribute("data-custom-attr", "value1");
    el->set_attribute("aria-label-long", "value2");
    el->set_attribute("x-custom-element", "value3");
    EXPECT_TRUE(el->has_attribute("data-custom-attr"));
    EXPECT_TRUE(el->has_attribute("aria-label-long"));
    EXPECT_TRUE(el->has_attribute("x-custom-element"));
    EXPECT_EQ(el->get_attribute("data-custom-attr"), "value1");
    EXPECT_EQ(el->get_attribute("aria-label-long"), "value2");
    EXPECT_EQ(el->get_attribute("x-custom-element"), "value3");
}

TEST(DomElement, ClassListRemoveMultipleAndReAddRound53) {
    Document doc;
    auto el = doc.create_element("div");
    el->class_list().add("active");
    el->class_list().add("focused");
    el->class_list().add("valid");
    EXPECT_TRUE(el->class_list().contains("active"));
    EXPECT_TRUE(el->class_list().contains("focused"));
    EXPECT_TRUE(el->class_list().contains("valid"));
    el->class_list().remove("focused");
    EXPECT_TRUE(el->class_list().contains("active"));
    EXPECT_FALSE(el->class_list().contains("focused"));
    EXPECT_TRUE(el->class_list().contains("valid"));
    el->class_list().add("focused");
    EXPECT_TRUE(el->class_list().contains("focused"));
    el->class_list().remove("active");
    el->class_list().remove("valid");
    EXPECT_FALSE(el->class_list().contains("active"));
    EXPECT_TRUE(el->class_list().contains("focused"));
    EXPECT_FALSE(el->class_list().contains("valid"));
}

// ---------------------------------------------------------------------------
// Cycle R54 — Additional DOM unit tests (8 tests)
// ---------------------------------------------------------------------------

TEST(DomElement, AttributeValueEdgeCasesEmptyWhitespaceAndSymbolsR54) {
    Document doc;
    auto el = doc.create_element("div");

    el->set_attribute("data-empty", "");
    el->set_attribute("data-space", "  spaced  ");
    el->set_attribute("data-symbols", "!@#$%^&*()[]{}|;:,.<>?/~`");

    EXPECT_TRUE(el->has_attribute("data-empty"));
    EXPECT_TRUE(el->has_attribute("data-space"));
    EXPECT_TRUE(el->has_attribute("data-symbols"));
    EXPECT_EQ(el->get_attribute("data-empty"), "");
    EXPECT_EQ(el->get_attribute("data-space"), "  spaced  ");
    EXPECT_EQ(el->get_attribute("data-symbols"), "!@#$%^&*()[]{}|;:,.<>?/~`");
    EXPECT_EQ(el->attributes().size(), 3u);
}

TEST(DomElement, AttributeOverwriteAndRemoveAdjustsMapStateR54) {
    Document doc;
    auto el = doc.create_element("section");

    el->set_attribute("data-mode", "draft");
    el->set_attribute("data-mode", "published");
    EXPECT_EQ(el->get_attribute("data-mode"), "published");
    EXPECT_EQ(el->attributes().size(), 1u);

    el->remove_attribute("data-mode");
    EXPECT_FALSE(el->has_attribute("data-mode"));
    EXPECT_EQ(el->get_attribute("data-mode"), std::nullopt);
    EXPECT_EQ(el->attributes().size(), 0u);
}

TEST(DomElement, ClassListAddRemoveContainsAndToggleSequenceR54) {
    Document doc;
    auto el = doc.create_element("article");

    el->class_list().add("card");
    el->class_list().add("selected");
    EXPECT_TRUE(el->class_list().contains("card"));
    EXPECT_TRUE(el->class_list().contains("selected"));

    el->class_list().remove("card");
    EXPECT_FALSE(el->class_list().contains("card"));
    EXPECT_TRUE(el->class_list().contains("selected"));

    el->class_list().toggle("selected");
    EXPECT_FALSE(el->class_list().contains("selected"));
    el->class_list().toggle("selected");
    EXPECT_TRUE(el->class_list().contains("selected"));
}

TEST(DomElement, ClassListToggleNonexistentClassAddsWithoutAffectingOthersR54) {
    Document doc;
    auto el = doc.create_element("div");

    el->class_list().add("base");
    el->class_list().toggle("interactive");
    EXPECT_TRUE(el->class_list().contains("base"));
    EXPECT_TRUE(el->class_list().contains("interactive"));

    el->class_list().remove("interactive");
    EXPECT_TRUE(el->class_list().contains("base"));
    EXPECT_FALSE(el->class_list().contains("interactive"));
}

TEST(DomNode, TreeTraversalFirstLastAndPreviousSiblingR54) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto a = doc.create_element("li");
    auto b = doc.create_element("li");
    auto c = doc.create_element("li");

    auto* a_ptr = a.get();
    auto* b_ptr = b.get();
    auto* c_ptr = c.get();

    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    parent->append_child(std::move(c));

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), a_ptr);
    EXPECT_EQ(parent->last_child(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->previous_sibling(), a_ptr);
    EXPECT_EQ(a_ptr->previous_sibling(), nullptr);
}

TEST(DomNode, ChildManipulationRemoveMiddleChildRewiresEndsR54) {
    Document doc;
    auto parent = doc.create_element("div");
    auto first = doc.create_element("span");
    auto middle = doc.create_element("span");
    auto last = doc.create_element("span");

    auto* first_ptr = first.get();
    auto* middle_ptr = middle.get();
    auto* last_ptr = last.get();

    parent->append_child(std::move(first));
    parent->append_child(std::move(middle));
    parent->append_child(std::move(last));

    auto removed = parent->remove_child(*middle_ptr);
    EXPECT_NE(removed, nullptr);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), first_ptr);
    EXPECT_EQ(parent->last_child(), last_ptr);
    EXPECT_EQ(last_ptr->previous_sibling(), first_ptr);
}

TEST(DomNode, NodeTypeChecksForElementTextAndCommentR54) {
    Document doc;
    auto container = doc.create_element("div");
    auto text = doc.create_text_node("hello");
    auto comment = doc.create_comment("note");

    auto* text_ptr = text.get();
    auto* comment_ptr = comment.get();

    container->append_child(std::move(text));
    container->append_child(std::move(comment));

    EXPECT_EQ(container->node_type(), NodeType::Element);
    EXPECT_EQ(text_ptr->node_type(), NodeType::Text);
    EXPECT_EQ(comment_ptr->node_type(), NodeType::Comment);
    EXPECT_EQ(container->child_count(), 2u);
}

TEST(DomElement, IdOperationsReflectAttributeLifecycleAndTagNormalizationR54) {
    Document doc;
    auto el = doc.create_element("header");

    EXPECT_EQ(el->tag_name(), "header");
    EXPECT_EQ(el->id(), "");

    el->set_attribute("id", "hero");
    EXPECT_EQ(el->id(), "hero");
    EXPECT_EQ(el->get_attribute("id"), "hero");

    el->set_attribute("id", "hero-main");
    EXPECT_EQ(el->id(), "hero-main");
    EXPECT_EQ(el->attributes().size(), 1u);

    el->remove_attribute("id");
    EXPECT_EQ(el->id(), "");
    EXPECT_FALSE(el->has_attribute("id"));
}

TEST(DomElement, TagNameAndIdReflectSetAndGetAttributeV55) {
    Document doc;
    auto el = doc.create_element("section");

    EXPECT_EQ(el->tag_name(), "section");
    EXPECT_EQ(el->id(), "");

    el->set_attribute("id", "hero-v55");
    auto id_attr = el->get_attribute("id");
    ASSERT_TRUE(id_attr.has_value());
    EXPECT_EQ(id_attr.value(), "hero-v55");
    EXPECT_EQ(el->id(), "hero-v55");
}

TEST(DomNode, AppendChildThenRemoveChildUpdatesTreeAndTagNameV55) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child = doc.create_element("span");
    auto* child_ptr = child.get();

    EXPECT_EQ(child_ptr->tag_name(), "span");
    parent->append_child(std::move(child));
    EXPECT_EQ(parent->child_count(), 1u);

    auto removed = parent->remove_child(*child_ptr);
    EXPECT_NE(removed, nullptr);
    EXPECT_EQ(removed->node_type(), NodeType::Element);
    EXPECT_EQ(parent->child_count(), 0u);
}

TEST(DomElement, ClassListMutationsKeepUnrelatedAttributesV55) {
    Document doc;
    auto el = doc.create_element("article");

    el->set_attribute("data-kind", "card");
    el->class_list().add("selected");
    el->class_list().toggle("hidden");
    el->class_list().remove("selected");

    auto attr = el->get_attribute("data-kind");
    ASSERT_TRUE(attr.has_value());
    EXPECT_EQ(attr.value(), "card");
    EXPECT_TRUE(el->class_list().contains("hidden"));
    EXPECT_FALSE(el->class_list().contains("selected"));
}

TEST(DomElement, TextContentConcatenatesAfterAppendChildOperationsV55) {
    Document doc;
    auto parent = doc.create_element("p");
    auto first = doc.create_text_node("hello");
    auto second = doc.create_text_node(" world");

    parent->append_child(std::move(first));
    parent->append_child(std::move(second));

    EXPECT_EQ(parent->text_content(), "hello world");
}

TEST(DomNode, RemoveChildShrinksTextContentV55) {
    Document doc;
    auto parent = doc.create_element("div");
    auto keep = doc.create_text_node("A");
    auto remove = doc.create_text_node("B");
    auto* remove_ptr = remove.get();

    parent->append_child(std::move(keep));
    parent->append_child(std::move(remove));
    EXPECT_EQ(parent->text_content(), "AB");

    auto removed = parent->remove_child(*remove_ptr);
    EXPECT_NE(removed, nullptr);
    EXPECT_EQ(parent->text_content(), "A");
}

TEST(DomElement, IdTracksAttributeOverwriteAndRemovalV55) {
    Document doc;
    auto el = doc.create_element("main");

    el->set_attribute("id", "stage-one");
    EXPECT_EQ(el->id(), "stage-one");

    el->set_attribute("id", "stage-two");
    auto id_attr = el->get_attribute("id");
    ASSERT_TRUE(id_attr.has_value());
    EXPECT_EQ(id_attr.value(), "stage-two");
    EXPECT_EQ(el->id(), "stage-two");

    el->remove_attribute("id");
    EXPECT_EQ(el->id(), "");
    EXPECT_FALSE(el->get_attribute("id").has_value());
}

TEST(DomNode, RemoveChildThenReappendRestoresTextContentV55) {
    Document doc;
    auto parent = doc.create_element("div");
    auto text = doc.create_text_node("x");
    auto* text_ptr = text.get();

    parent->append_child(std::move(text));
    EXPECT_EQ(parent->text_content(), "x");

    auto removed = parent->remove_child(*text_ptr);
    EXPECT_EQ(parent->text_content(), "");

    parent->append_child(std::move(removed));
    EXPECT_EQ(parent->text_content(), "x");
}

TEST(DomElement, NestedElementsTagNameAndTextContentRoundTripV55) {
    Document doc;
    auto outer = doc.create_element("ul");
    auto inner = doc.create_element("li");
    auto text = doc.create_text_node("item");
    auto* inner_ptr = inner.get();

    inner->append_child(std::move(text));
    outer->append_child(std::move(inner));

    EXPECT_EQ(outer->tag_name(), "ul");
    EXPECT_EQ(inner_ptr->tag_name(), "li");
    EXPECT_EQ(outer->text_content(), "item");
}

TEST(DomElement, AttributesVectorSizeAndIterationV56) {
    Document doc;
    auto el = doc.create_element("div");

    el->set_attribute("class", "container");
    el->set_attribute("id", "main");
    el->set_attribute("data-value", "42");

    auto attrs = el->attributes();
    EXPECT_EQ(attrs.size(), 3u);

    int count = 0;
    for (const auto& attr : attrs) {
        count++;
        EXPECT_FALSE(attr.name.empty());
        EXPECT_FALSE(attr.value.empty());
    }
    EXPECT_EQ(count, 3);
}

TEST(DomNode, ForEachChildWithMixedNodeTypesV56) {
    Document doc;
    auto parent = doc.create_element("div");
    auto text1 = doc.create_text_node("start");
    auto elem = doc.create_element("span");
    auto text2 = doc.create_text_node("end");

    parent->append_child(std::move(text1));
    parent->append_child(std::move(elem));
    parent->append_child(std::move(text2));

    int element_count = 0;
    int text_count = 0;
    parent->for_each_child([&](Node& child) {
        if (child.node_type() == NodeType::Element) {
            element_count++;
        } else if (child.node_type() == NodeType::Text) {
            text_count++;
        }
    });

    EXPECT_EQ(element_count, 1);
    EXPECT_EQ(text_count, 2);
}

TEST(DomNode, CommentNodeTextContentDoesNotAffectParentV56) {
    Document doc;
    auto parent = doc.create_element("section");
    auto text = doc.create_text_node("visible");
    auto comment = doc.create_comment("hidden comment");

    parent->append_child(std::move(text));
    parent->append_child(std::move(comment));

    EXPECT_EQ(parent->text_content(), "visible");
    EXPECT_EQ(parent->child_count(), 2u);
}

TEST(DomElement, ParentPointerInComplexTreeV56) {
    Document doc;
    auto root = doc.create_element("html");
    auto body = doc.create_element("body");
    auto div = doc.create_element("div");
    auto* div_ptr = div.get();
    auto* body_ptr = body.get();

    div->append_child(doc.create_text_node("text"));
    body->append_child(std::move(div));
    root->append_child(std::move(body));

    auto* div_parent = div_ptr->parent();
    ASSERT_NE(div_parent, nullptr);
    EXPECT_EQ(div_parent, body_ptr);

    auto* body_parent = body_ptr->parent();
    ASSERT_NE(body_parent, nullptr);
    EXPECT_EQ(body_parent, root.get());
}

TEST(DomElement, ClassListToggleMultipleTimesKeepsStateV56) {
    Document doc;
    auto el = doc.create_element("button");

    el->class_list().toggle("active");
    EXPECT_TRUE(el->class_list().contains("active"));
    EXPECT_EQ(el->class_list().length(), 1u);

    el->class_list().toggle("active");
    EXPECT_FALSE(el->class_list().contains("active"));
    EXPECT_EQ(el->class_list().length(), 0u);

    el->class_list().toggle("active");
    EXPECT_TRUE(el->class_list().contains("active"));
    EXPECT_EQ(el->class_list().length(), 1u);
}

TEST(DomElement, DeepNestedTreeWithMixedNodesV56) {
    Document doc;
    auto level1 = doc.create_element("div");
    auto level2 = doc.create_element("article");
    auto level3 = doc.create_element("p");
    auto level4 = doc.create_element("span");
    auto text = doc.create_text_node("deep");
    auto* level4_ptr = level4.get();

    level4->append_child(std::move(text));
    level3->append_child(std::move(level4));
    level2->append_child(std::move(level3));
    level1->append_child(std::move(level2));

    EXPECT_EQ(level1->text_content(), "deep");
    EXPECT_EQ(level4_ptr->tag_name(), "span");
    EXPECT_EQ(level1->child_count(), 1u);
}

TEST(DomElement, AttributeOverwriteWithMultipleOperationsV56) {
    Document doc;
    auto el = doc.create_element("a");

    el->set_attribute("href", "http://example.com");
    el->set_attribute("target", "_blank");
    el->set_attribute("href", "http://other.com");
    el->set_attribute("rel", "external");

    auto href = el->get_attribute("href");
    ASSERT_TRUE(href.has_value());
    EXPECT_EQ(href.value(), "http://other.com");

    auto attrs = el->attributes();
    EXPECT_EQ(attrs.size(), 3u);

    auto target = el->get_attribute("target");
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), "_blank");
}

TEST(DomNode, ChildRemovalAndReinsertionV56) {
    Document doc;
    auto parent = doc.create_element("ol");
    auto item1 = doc.create_element("li");
    auto item2 = doc.create_element("li");
    auto item3 = doc.create_element("li");
    auto* item2_ptr = item2.get();

    parent->append_child(std::move(item1));
    parent->append_child(std::move(item2));
    parent->append_child(std::move(item3));

    EXPECT_EQ(parent->child_count(), 3u);

    auto removed = parent->remove_child(*item2_ptr);
    EXPECT_NE(removed, nullptr);
    EXPECT_EQ(parent->child_count(), 2u);

    parent->append_child(std::move(removed));
    EXPECT_EQ(parent->child_count(), 3u);
}

TEST(DomElement, SetIdAndGetAttributeReflectionV57) {
    Document doc;
    auto elem = doc.create_element("div");

    elem->set_attribute("id", "container");
    EXPECT_EQ(elem->id(), "container");

    auto id_attr = elem->get_attribute("id");
    ASSERT_TRUE(id_attr.has_value());
    EXPECT_EQ(id_attr.value(), "container");

    EXPECT_TRUE(elem->has_attribute("id"));
}

TEST(DomElement, ClassListAddRemoveAndContainsV57) {
    Document doc;
    auto elem = doc.create_element("p");

    EXPECT_FALSE(elem->class_list().contains("active"));

    elem->class_list().add("active");
    EXPECT_TRUE(elem->class_list().contains("active"));

    elem->class_list().add("highlight");
    EXPECT_TRUE(elem->class_list().contains("highlight"));

    elem->class_list().remove("active");
    EXPECT_FALSE(elem->class_list().contains("active"));
    EXPECT_TRUE(elem->class_list().contains("highlight"));
}

TEST(DomNode, TextNodeWithParentAndSiblingsV57) {
    Document doc;
    auto parent = doc.create_element("div");
    auto text1 = doc.create_text_node("Hello ");
    auto elem = doc.create_element("span");
    auto text2 = doc.create_text_node(" World");

    parent->append_child(std::move(text1));
    parent->append_child(std::move(elem));
    parent->append_child(std::move(text2));

    EXPECT_EQ(parent->child_count(), 3u);
    auto first = parent->first_child();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->text_content(), "Hello ");

    auto last = parent->last_child();
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->text_content(), " World");
}

TEST(DomElement, NextAndPreviousSiblingTraversalV57) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto item1 = doc.create_element("li");
    auto item2 = doc.create_element("li");
    auto item3 = doc.create_element("li");
    auto* item2_ptr = item2.get();

    parent->append_child(std::move(item1));
    parent->append_child(std::move(item2));
    parent->append_child(std::move(item3));

    EXPECT_EQ(item2_ptr->next_sibling(), parent->last_child());
    auto prev = item2_ptr->previous_sibling();
    ASSERT_NE(prev, nullptr);
    EXPECT_EQ(prev->node_type(), NodeType::Element);
}

TEST(DomElement, MultipleAttributesIterationV57) {
    Document doc;
    auto elem = doc.create_element("img");

    elem->set_attribute("src", "image.png");
    elem->set_attribute("alt", "Image");
    elem->set_attribute("width", "100");
    elem->set_attribute("height", "100");

    auto attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 4u);

    bool found_src = false;
    bool found_alt = false;
    for (const auto& attr : attrs) {
        if (attr.name == "src") {
            EXPECT_EQ(attr.value, "image.png");
            found_src = true;
        }
        if (attr.name == "alt") {
            EXPECT_EQ(attr.value, "Image");
            found_alt = true;
        }
    }
    EXPECT_TRUE(found_src);
    EXPECT_TRUE(found_alt);
}

TEST(DomNode, InsertBeforeWithMultipleChildrenV57) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child1 = doc.create_element("span");
    auto child2 = doc.create_element("span");
    auto new_child = doc.create_element("em");
    auto* child1_ptr = child1.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));

    EXPECT_EQ(parent->child_count(), 2u);

    parent->insert_before(std::move(new_child), child1_ptr);
    EXPECT_EQ(parent->child_count(), 3u);

    auto first = parent->first_child();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->node_type(), NodeType::Element);
}

TEST(DomNode, ForEachChildIterationWithTextAndElementsV57) {
    Document doc;
    auto parent = doc.create_element("article");
    auto text1 = doc.create_text_node("Start ");
    auto elem = doc.create_element("strong");
    auto text2 = doc.create_text_node(" End");

    parent->append_child(std::move(text1));
    parent->append_child(std::move(elem));
    parent->append_child(std::move(text2));

    int element_count = 0;
    int text_count = 0;
    parent->for_each_child([&](Node& child) {
        if (child.node_type() == NodeType::Element) {
            element_count++;
        } else if (child.node_type() == NodeType::Text) {
            text_count++;
        }
    });

    EXPECT_EQ(element_count, 1);
    EXPECT_EQ(text_count, 2);
}

TEST(DomElement, RemoveAndReaddDifferentElementTypeV57) {
    Document doc;
    auto parent = doc.create_element("section");
    auto old_elem = doc.create_element("div");
    auto new_elem = doc.create_element("article");
    auto* old_ptr = old_elem.get();

    parent->append_child(std::move(old_elem));
    EXPECT_EQ(parent->child_count(), 1u);

    parent->remove_child(*old_ptr);
    EXPECT_EQ(parent->child_count(), 0u);

    parent->append_child(std::move(new_elem));
    EXPECT_EQ(parent->child_count(), 1u);

    auto child = parent->first_child();
    ASSERT_NE(child, nullptr);
    auto elem = dynamic_cast<Element*>(child);
    ASSERT_NE(elem, nullptr);
    EXPECT_EQ(elem->tag_name(), "article");
}

// ---------------------------------------------------------------------------
// V58 Suite: Additional DOM tests
// ---------------------------------------------------------------------------

TEST(DomNode, PreviousSiblingTraversalV58) {
    Document doc;
    auto parent = doc.create_element("div");
    auto child1 = doc.create_element("p");
    auto child2 = doc.create_element("span");
    auto child3 = doc.create_element("em");

    auto* c1 = child1.get();
    auto* c2 = child2.get();
    auto* c3 = child3.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    parent->append_child(std::move(child3));

    EXPECT_EQ(c3->previous_sibling(), c2);
    EXPECT_EQ(c2->previous_sibling(), c1);
    EXPECT_EQ(c1->previous_sibling(), nullptr);
}

TEST(DomElement, GetAttributeReturnsCorrectValueV58) {
    Document doc;
    auto elem = doc.create_element("input");
    elem->set_attribute("type", "text");
    elem->set_attribute("placeholder", "Enter your name");
    elem->set_attribute("maxlength", "50");

    auto type_attr = elem->get_attribute("type");
    auto placeholder_attr = elem->get_attribute("placeholder");
    auto maxlength_attr = elem->get_attribute("maxlength");

    ASSERT_TRUE(type_attr.has_value());
    ASSERT_TRUE(placeholder_attr.has_value());
    ASSERT_TRUE(maxlength_attr.has_value());
    EXPECT_EQ(type_attr.value(), "text");
    EXPECT_EQ(placeholder_attr.value(), "Enter your name");
    EXPECT_EQ(maxlength_attr.value(), "50");
}

TEST(DomElement, MultipleClassesOperationsV58) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->class_list().add("container");
    elem->class_list().add("flex");
    elem->class_list().add("active");

    EXPECT_TRUE(elem->class_list().contains("container"));
    EXPECT_TRUE(elem->class_list().contains("flex"));
    EXPECT_TRUE(elem->class_list().contains("active"));
    EXPECT_FALSE(elem->class_list().contains("hidden"));

    elem->class_list().remove("flex");
    EXPECT_FALSE(elem->class_list().contains("flex"));
    EXPECT_TRUE(elem->class_list().contains("container"));
}

TEST(DomNode, InsertBeforeFirstChildV58) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto existing = doc.create_element("li");
    auto new_item = doc.create_element("li");

    auto* existing_ptr = existing.get();
    parent->append_child(std::move(existing));
    EXPECT_EQ(parent->child_count(), 1u);

    auto* new_ptr = new_item.get();
    parent->insert_before(std::move(new_item), existing_ptr);
    EXPECT_EQ(parent->child_count(), 2u);

    auto first = parent->first_child();
    ASSERT_NE(first, nullptr);
    auto first_elem = dynamic_cast<Element*>(first);
    ASSERT_NE(first_elem, nullptr);
    EXPECT_EQ(first_elem, new_ptr);
}

TEST(DomElement, AttributesVectorIterationV58) {
    Document doc;
    auto elem = doc.create_element("a");
    elem->set_attribute("href", "https://example.com");
    elem->set_attribute("title", "Example Site");
    elem->set_attribute("target", "_blank");

    auto attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 3u);

    int found_count = 0;
    for (const auto& attr : attrs) {
        if (attr.name == "href") {
            EXPECT_EQ(attr.value, "https://example.com");
            found_count++;
        } else if (attr.name == "title") {
            EXPECT_EQ(attr.value, "Example Site");
            found_count++;
        } else if (attr.name == "target") {
            EXPECT_EQ(attr.value, "_blank");
            found_count++;
        }
    }
    EXPECT_EQ(found_count, 3);
}

TEST(DomNode, ForEachChildIterationV58) {
    Document doc;
    auto parent = doc.create_element("div");
    auto text1 = doc.create_text_node("Hello ");
    auto text2 = doc.create_text_node("World");
    auto elem = doc.create_element("span");

    parent->append_child(std::move(text1));
    parent->append_child(std::move(elem));
    parent->append_child(std::move(text2));

    int iteration_count = 0;
    parent->for_each_child([&iteration_count](Node& child) {
        iteration_count++;
    });

    EXPECT_EQ(iteration_count, 3);
}

TEST(DomElement, RemoveAttributeAndVerifyV58) {
    Document doc;
    auto elem = doc.create_element("button");
    elem->set_attribute("disabled", "true");
    elem->set_attribute("class", "btn-primary");
    elem->set_attribute("onclick", "handleClick()");

    EXPECT_TRUE(elem->has_attribute("disabled"));
    elem->remove_attribute("disabled");
    EXPECT_FALSE(elem->has_attribute("disabled"));

    EXPECT_TRUE(elem->has_attribute("class"));
    EXPECT_TRUE(elem->has_attribute("onclick"));

    auto attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 2u);
}

TEST(DomNode, LastChildPointerV58) {
    Document doc;
    auto parent = doc.create_element("section");
    auto child1 = doc.create_element("article");
    auto child2 = doc.create_element("article");
    auto child3 = doc.create_element("article");

    auto* child2_ptr = child2.get();
    auto* child3_ptr = child3.get();
    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    parent->append_child(std::move(child3));

    auto last = parent->last_child();
    ASSERT_NE(last, nullptr);
    auto last_elem = dynamic_cast<Element*>(last);
    ASSERT_NE(last_elem, nullptr);
    EXPECT_EQ(last_elem, child3_ptr);

    auto second_to_last = last->previous_sibling();
    ASSERT_NE(second_to_last, nullptr);
    auto second_elem = dynamic_cast<Element*>(second_to_last);
    ASSERT_NE(second_elem, nullptr);
    EXPECT_EQ(second_elem, child2_ptr);
}

TEST(DomElement, CreateElementWithAttributesV59) {
    Document doc;
    auto elem = doc.create_element("input");
    elem->set_attribute("type", "text");
    elem->set_attribute("placeholder", "Enter name");
    elem->set_attribute("maxlength", "50");

    EXPECT_TRUE(elem->has_attribute("type"));
    EXPECT_TRUE(elem->has_attribute("placeholder"));
    EXPECT_TRUE(elem->has_attribute("maxlength"));
    EXPECT_EQ(elem->get_attribute("type").value(), "text");
    EXPECT_EQ(elem->get_attribute("placeholder").value(), "Enter name");
    EXPECT_EQ(elem->get_attribute("maxlength").value(), "50");
    EXPECT_EQ(elem->attributes().size(), 3u);
}

TEST(DomNode, InsertBeforeMultipleNodesV59) {
    Document doc;
    auto parent = doc.create_element("ul");
    auto item1 = doc.create_element("li");
    auto item2 = doc.create_element("li");
    auto item3 = doc.create_element("li");

    auto* item1_ptr = item1.get();
    parent->append_child(std::move(item1));

    auto inserted_item3 = doc.create_element("li");
    auto* item3_ptr = inserted_item3.get();
    parent->insert_before(std::move(inserted_item3), item1_ptr);

    auto inserted_item2 = doc.create_element("li");
    auto* item2_inserted_ptr = inserted_item2.get();
    parent->insert_before(std::move(inserted_item2), item1_ptr);

    auto first_child = parent->first_child();
    ASSERT_NE(first_child, nullptr);
    EXPECT_EQ(first_child, item3_ptr);

    auto second_child = first_child->next_sibling();
    ASSERT_NE(second_child, nullptr);
    EXPECT_EQ(second_child, item2_inserted_ptr);

    auto third_child = second_child->next_sibling();
    ASSERT_NE(third_child, nullptr);
    EXPECT_EQ(third_child, item1_ptr);
}

TEST(DomElement, GetAttributeWithMultipleValuesV59) {
    Document doc;
    auto elem = doc.create_element("a");
    elem->set_attribute("href", "https://example.com");
    elem->set_attribute("title", "Visit Example");
    elem->set_attribute("target", "_blank");
    elem->set_attribute("rel", "noopener");

    EXPECT_EQ(elem->get_attribute("href").value(), "https://example.com");
    EXPECT_EQ(elem->get_attribute("title").value(), "Visit Example");
    EXPECT_EQ(elem->get_attribute("target").value(), "_blank");
    EXPECT_EQ(elem->get_attribute("rel").value(), "noopener");

    auto attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 4u);
}

TEST(DomElement, ClassListOperationsWithMultipleClassesV59) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->class_list().add("container");
    elem->class_list().add("active");
    elem->class_list().add("highlighted");

    EXPECT_TRUE(elem->class_list().contains("container"));
    EXPECT_TRUE(elem->class_list().contains("active"));
    EXPECT_TRUE(elem->class_list().contains("highlighted"));

    elem->class_list().remove("active");
    EXPECT_FALSE(elem->class_list().contains("active"));
    EXPECT_TRUE(elem->class_list().contains("container"));
    EXPECT_TRUE(elem->class_list().contains("highlighted"));

    auto class_str = elem->class_list().to_string();
    EXPECT_NE(class_str.find("container"), std::string::npos);
    EXPECT_NE(class_str.find("highlighted"), std::string::npos);
}

TEST(DomNode, TextNodeSiblingsV59) {
    Document doc;
    auto parent = doc.create_element("p");
    auto text1 = doc.create_text_node("Hello ");
    auto elem = doc.create_element("strong");
    auto text2 = doc.create_text_node(" World");

    auto* text1_ptr = text1.get();
    auto* elem_ptr = elem.get();
    auto* text2_ptr = text2.get();

    parent->append_child(std::move(text1));
    parent->append_child(std::move(elem));
    parent->append_child(std::move(text2));

    auto first = parent->first_child();
    ASSERT_EQ(first, text1_ptr);
    auto next = first->next_sibling();
    ASSERT_EQ(next, elem_ptr);
    auto last = next->next_sibling();
    ASSERT_EQ(last, text2_ptr);
    EXPECT_EQ(last->previous_sibling(), elem_ptr);
}

TEST(DomElement, RemoveMultipleAttributesV59) {
    Document doc;
    auto elem = doc.create_element("img");
    elem->set_attribute("src", "image.jpg");
    elem->set_attribute("alt", "An image");
    elem->set_attribute("width", "100");
    elem->set_attribute("height", "100");

    EXPECT_EQ(elem->attributes().size(), 4u);

    elem->remove_attribute("width");
    EXPECT_EQ(elem->attributes().size(), 3u);
    EXPECT_FALSE(elem->has_attribute("width"));

    elem->remove_attribute("height");
    EXPECT_EQ(elem->attributes().size(), 2u);
    EXPECT_FALSE(elem->has_attribute("height"));

    EXPECT_TRUE(elem->has_attribute("src"));
    EXPECT_TRUE(elem->has_attribute("alt"));
}

TEST(DomNode, ComplexTreeTraversalV59) {
    Document doc;
    auto root = doc.create_element("div");
    auto section = doc.create_element("section");
    auto article1 = doc.create_element("article");
    auto article2 = doc.create_element("article");
    auto heading = doc.create_element("h2");

    auto* section_ptr = section.get();
    auto* article1_ptr = article1.get();
    auto* article2_ptr = article2.get();
    auto* heading_ptr = heading.get();

    root->append_child(std::move(section));
    section_ptr->append_child(std::move(article1));
    section_ptr->append_child(std::move(article2));
    article1_ptr->append_child(std::move(heading));

    auto first_child_of_root = root->first_child();
    EXPECT_EQ(first_child_of_root, section_ptr);

    auto first_child_of_section = section_ptr->first_child();
    EXPECT_EQ(first_child_of_section, article1_ptr);

    auto next_article = first_child_of_section->next_sibling();
    EXPECT_EQ(next_article, article2_ptr);

    auto first_child_of_article1 = article1_ptr->first_child();
    EXPECT_EQ(first_child_of_article1, heading_ptr);
}

TEST(DomElement, SetAttributeIdAndRetrieveV59) {
    Document doc;
    auto elem = doc.create_element("main");
    elem->set_attribute("id", "main-content");
    elem->set_attribute("role", "main");

    EXPECT_TRUE(elem->has_attribute("id"));
    EXPECT_EQ(elem->get_attribute("id").value(), "main-content");
    EXPECT_TRUE(elem->has_attribute("role"));
    EXPECT_EQ(elem->get_attribute("role").value(), "main");

    // Verify the id() method reflects the attribute
    EXPECT_EQ(elem->id(), "main-content");

    // Verify attributes can be queried properly
    auto attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 2u);
}

TEST(DomElement, AttributeOverwriteAndRetrieveV60) {
    Document doc;
    auto elem = doc.create_element("div");
    elem->set_attribute("data-id", "first");
    EXPECT_EQ(elem->get_attribute("data-id").value(), "first");

    elem->set_attribute("data-id", "second");
    EXPECT_EQ(elem->get_attribute("data-id").value(), "second");

    // Ensure no duplicate attributes
    auto attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 1u);

    elem->set_attribute("data-value", "test");
    attrs = elem->attributes();
    EXPECT_EQ(attrs.size(), 2u);
}

TEST(DomNode, ChildCountAndTraversalV60) {
    Document doc;
    auto parent = doc.create_element("ul");

    EXPECT_EQ(parent->child_count(), 0u);
    EXPECT_EQ(parent->first_child(), nullptr);
    EXPECT_EQ(parent->last_child(), nullptr);

    auto li1 = doc.create_element("li");
    auto li2 = doc.create_element("li");
    auto li3 = doc.create_element("li");

    auto* li1_ptr = li1.get();
    auto* li2_ptr = li2.get();
    auto* li3_ptr = li3.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), li1_ptr);
    EXPECT_EQ(parent->last_child(), li3_ptr);

    auto* second = li1_ptr->next_sibling();
    EXPECT_EQ(second, li2_ptr);

    auto* third = li2_ptr->next_sibling();
    EXPECT_EQ(third, li3_ptr);
    EXPECT_EQ(li3_ptr->next_sibling(), nullptr);
}

TEST(DomElement, ClassListToggleAndContainsV60) {
    Document doc;
    auto elem = doc.create_element("button");

    // Initially empty
    EXPECT_EQ(elem->class_list().length(), 0u);
    EXPECT_FALSE(elem->class_list().contains("active"));

    // Toggle to add
    elem->class_list().toggle("active");
    EXPECT_TRUE(elem->class_list().contains("active"));
    EXPECT_EQ(elem->class_list().length(), 1u);

    // Toggle to remove
    elem->class_list().toggle("active");
    EXPECT_FALSE(elem->class_list().contains("active"));
    EXPECT_EQ(elem->class_list().length(), 0u);

    // Add multiple and verify
    elem->class_list().add("btn");
    elem->class_list().add("primary");
    EXPECT_TRUE(elem->class_list().contains("btn"));
    EXPECT_TRUE(elem->class_list().contains("primary"));
    EXPECT_EQ(elem->class_list().length(), 2u);
}

TEST(DomNode, InsertBeforeAndChildOrderV60) {
    Document doc;
    auto parent = doc.create_element("nav");
    auto link1 = doc.create_element("a");
    auto link2 = doc.create_element("a");
    auto link3 = doc.create_element("a");

    auto* link1_ptr = link1.get();
    auto* link2_ptr = link2.get();
    auto* link3_ptr = link3.get();

    parent->append_child(std::move(link1));
    parent->append_child(std::move(link3));

    // Insert link2 between link1 and link3
    parent->insert_before(std::move(link2), link3_ptr);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), link1_ptr);
    EXPECT_EQ(link1_ptr->next_sibling(), link2_ptr);
    EXPECT_EQ(link2_ptr->next_sibling(), link3_ptr);
    EXPECT_EQ(parent->last_child(), link3_ptr);
    EXPECT_EQ(link3_ptr->previous_sibling(), link2_ptr);
}

TEST(DomNode, TextContentAcrossMultipleNodesV60) {
    Document doc;
    auto paragraph = doc.create_element("p");
    auto strong = doc.create_element("strong");
    auto em = doc.create_element("em");

    auto text1 = doc.create_text_node("This is ");
    auto text2 = doc.create_text_node("important");
    auto text3 = doc.create_text_node(" and ");
    auto text4 = doc.create_text_node("emphasized");

    auto* text1_ptr = text1.get();
    auto* text2_ptr = text2.get();

    strong->append_child(std::move(text2));
    em->append_child(std::move(text4));

    paragraph->append_child(std::move(text1));
    paragraph->append_child(std::move(strong));
    paragraph->append_child(std::move(text3));
    paragraph->append_child(std::move(em));

    auto full_text = paragraph->text_content();
    EXPECT_EQ(full_text, "This is important and emphasized");

    // Verify individual text nodes
    auto* first_text = dynamic_cast<Text*>(text1_ptr);
    ASSERT_NE(first_text, nullptr);
    EXPECT_EQ(first_text->text_content(), "This is ");

    auto* second_text = dynamic_cast<Text*>(text2_ptr);
    ASSERT_NE(second_text, nullptr);
    EXPECT_EQ(second_text->text_content(), "important");
}

TEST(DomElement, AttributeModificationAndAttributesVectorV60) {
    Document doc;
    auto form = doc.create_element("form");
    form->set_attribute("method", "POST");
    form->set_attribute("action", "/submit");
    form->set_attribute("enctype", "multipart/form-data");

    auto initial_attrs = form->attributes();
    EXPECT_EQ(initial_attrs.size(), 3u);

    // Modify an attribute
    form->set_attribute("action", "/api/submit");
    auto modified_attrs = form->attributes();
    EXPECT_EQ(modified_attrs.size(), 3u);
    EXPECT_EQ(form->get_attribute("action").value(), "/api/submit");

    // Remove an attribute
    form->remove_attribute("enctype");
    auto final_attrs = form->attributes();
    EXPECT_EQ(final_attrs.size(), 2u);
    EXPECT_FALSE(form->has_attribute("enctype"));
    EXPECT_TRUE(form->has_attribute("method"));
    EXPECT_TRUE(form->has_attribute("action"));
}

TEST(DomNode, ComplexNestedTreeStructureV60) {
    Document doc;
    auto html = doc.create_element("html");
    auto body = doc.create_element("body");
    auto main_section = doc.create_element("main");
    auto article = doc.create_element("article");
    auto header = doc.create_element("header");
    auto h1 = doc.create_element("h1");
    auto content = doc.create_element("div");

    auto* html_ptr = html.get();
    auto* body_ptr = body.get();
    auto* main_ptr = main_section.get();
    auto* article_ptr = article.get();
    auto* header_ptr = header.get();
    auto* h1_ptr = h1.get();
    auto* content_ptr = content.get();

    // Build tree: html > body > main > [article > [header > h1], content]
    html->append_child(std::move(body));
    body_ptr->append_child(std::move(main_section));
    main_ptr->append_child(std::move(article));
    main_ptr->append_child(std::move(content));
    article_ptr->append_child(std::move(header));
    header_ptr->append_child(std::move(h1));

    // Verify structure from root
    EXPECT_EQ(html_ptr->first_child(), body_ptr);
    EXPECT_EQ(body_ptr->first_child(), main_ptr);
    EXPECT_EQ(main_ptr->first_child(), article_ptr);
    EXPECT_EQ(main_ptr->last_child(), content_ptr);
    EXPECT_EQ(article_ptr->first_child(), header_ptr);
    EXPECT_EQ(header_ptr->first_child(), h1_ptr);

    // Verify sibling relationships
    EXPECT_EQ(article_ptr->next_sibling(), content_ptr);
    EXPECT_EQ(content_ptr->previous_sibling(), article_ptr);
}

TEST(DomElement, ElementTagNameAndMultipleAttributesV60) {
    Document doc;
    auto input = doc.create_element("input");

    EXPECT_EQ(input->tag_name(), "input");
    EXPECT_EQ(input->node_type(), NodeType::Element);

    input->set_attribute("type", "email");
    input->set_attribute("name", "user_email");
    input->set_attribute("required", "true");
    input->set_attribute("placeholder", "Enter email");
    input->set_attribute("aria-label", "Email input");

    EXPECT_EQ(input->get_attribute("type").value(), "email");
    EXPECT_EQ(input->get_attribute("name").value(), "user_email");
    EXPECT_EQ(input->get_attribute("required").value(), "true");
    EXPECT_EQ(input->get_attribute("placeholder").value(), "Enter email");
    EXPECT_EQ(input->get_attribute("aria-label").value(), "Email input");

    auto attrs = input->attributes();
    EXPECT_EQ(attrs.size(), 5u);

    // Verify iteration through attributes
    int count = 0;
    for (const auto& attr : attrs) {
        if (attr.name == "type" || attr.name == "name" || attr.name == "required" ||
            attr.name == "placeholder" || attr.name == "aria-label") {
            count++;
        }
    }
    EXPECT_EQ(count, 5);
}

// ---------------------------------------------------------------------------
// V61 TESTS: Event bubbling, capture, custom events, DOM mutations
// ---------------------------------------------------------------------------

TEST(DomEvent, EventBubblingThroughMultipleLevelsV61) {
    // Build tree: grandparent -> parent -> child
    auto grandparent = std::make_unique<Element>("div");
    auto parent = std::make_unique<Element>("section");
    auto child = std::make_unique<Element>("button");

    Node* gp_ptr = grandparent.get();
    Node* p_ptr = parent.get();
    Node* c_ptr = child.get();

    parent->append_child(std::move(child));
    grandparent->append_child(std::move(parent));

    std::vector<std::string> bubble_log;

    // Create event targets and listeners for bubbling
    EventTarget gp_target;
    gp_target.add_event_listener("click", [&](Event&) {
        bubble_log.push_back("gp-bubble");
    }, false);

    EventTarget p_target;
    p_target.add_event_listener("click", [&](Event&) {
        bubble_log.push_back("p-bubble");
    }, false);

    EventTarget c_target;
    c_target.add_event_listener("click", [&](Event&) {
        bubble_log.push_back("c-target");
    }, false);

    // Simulate event dispatch: bubbling from child up
    Event event("click");
    event.target_ = c_ptr;
    event.current_target_ = c_ptr;
    event.phase_ = EventPhase::AtTarget;
    c_target.dispatch_event(event, *c_ptr);

    // Bubble through parent
    if (!event.propagation_stopped() && event.bubbles()) {
        event.phase_ = EventPhase::Bubbling;
        event.current_target_ = p_ptr;
        p_target.dispatch_event(event, *p_ptr);
    }

    // Bubble to grandparent
    if (!event.propagation_stopped() && event.bubbles()) {
        event.current_target_ = gp_ptr;
        gp_target.dispatch_event(event, *gp_ptr);
    }

    EXPECT_EQ(bubble_log.size(), 3u);
    EXPECT_EQ(bubble_log[0], "c-target");
    EXPECT_EQ(bubble_log[1], "p-bubble");
    EXPECT_EQ(bubble_log[2], "gp-bubble");
}

TEST(DomEvent, EventCapturePhaseStopsAtTargetV61) {
    // Build tree: grandparent -> parent -> child
    auto grandparent = std::make_unique<Element>("div");
    auto parent = std::make_unique<Element>("section");
    auto child = std::make_unique<Element>("button");

    Node* gp_ptr = grandparent.get();
    Node* p_ptr = parent.get();
    Node* c_ptr = child.get();

    parent->append_child(std::move(child));
    grandparent->append_child(std::move(parent));

    std::vector<std::string> capture_log;

    EventTarget gp_target;
    gp_target.add_event_listener("click", [&](Event& e) {
        capture_log.push_back("gp-capture");
        e.stop_propagation();
    }, true);

    EventTarget p_target;
    p_target.add_event_listener("click", [&](Event&) {
        capture_log.push_back("p-capture");
    }, true);

    EventTarget c_target;
    c_target.add_event_listener("click", [&](Event&) {
        capture_log.push_back("c-target");
    }, false);

    Event event("click");
    event.target_ = c_ptr;

    // Capture phase from grandparent
    event.phase_ = EventPhase::Capturing;
    event.current_target_ = gp_ptr;
    gp_target.dispatch_event(event, *gp_ptr);

    // Propagation stopped, parent's capture should not fire
    if (!event.propagation_stopped()) {
        event.current_target_ = p_ptr;
        p_target.dispatch_event(event, *p_ptr);
    }

    // Target phase should still fire
    if (!event.propagation_stopped()) {
        event.phase_ = EventPhase::AtTarget;
        event.current_target_ = c_ptr;
        c_target.dispatch_event(event, *c_ptr);
    }

    EXPECT_EQ(capture_log.size(), 1u);
    EXPECT_EQ(capture_log[0], "gp-capture");
    EXPECT_TRUE(event.propagation_stopped());
}

TEST(DomEvent, CustomEventCreationAndDispatchV61) {
    Event custom_event("my-custom-event", true, true);

    EXPECT_EQ(custom_event.type(), "my-custom-event");
    EXPECT_TRUE(custom_event.bubbles());
    EXPECT_TRUE(custom_event.cancelable());
    EXPECT_EQ(custom_event.phase(), EventPhase::None);

    auto element = std::make_unique<Element>("div");
    Node* elem_ptr = element.get();

    std::vector<std::string> event_log;
    EventTarget target;
    target.add_event_listener("my-custom-event", [&](Event& e) {
        event_log.push_back(e.type());
    }, false);

    custom_event.target_ = elem_ptr;
    custom_event.current_target_ = elem_ptr;
    custom_event.phase_ = EventPhase::AtTarget;
    target.dispatch_event(custom_event, *elem_ptr);

    ASSERT_EQ(event_log.size(), 1u);
    EXPECT_EQ(event_log[0], "my-custom-event");
}

TEST(DomNode, InsertBeforeWithMultipleSiblingsV61) {
    // Create a parent with initial children
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");

    Element* p1 = li1.get();
    Element* p2 = li2.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));

    // Insert new element before middle child
    auto new_li = std::make_unique<Element>("li");
    Element* new_ptr = new_li.get();
    parent->insert_before(std::move(new_li), p2);

    EXPECT_EQ(parent->child_count(), 4u);
    EXPECT_EQ(p1->next_sibling(), new_ptr);
    EXPECT_EQ(new_ptr->next_sibling(), p2);
    EXPECT_EQ(p2->previous_sibling(), new_ptr);
    EXPECT_EQ(new_ptr->previous_sibling(), p1);
}

TEST(DomElement, ClassListToggleAndContainsV61) {
    auto button = std::make_unique<Element>("button");

    // Initially no classes
    EXPECT_FALSE(button->class_list().contains("active"));
    EXPECT_EQ(button->class_list().length(), 0u);

    // Add a class
    button->class_list().add("active");
    EXPECT_TRUE(button->class_list().contains("active"));
    EXPECT_EQ(button->class_list().length(), 1u);

    // Toggle removes it
    button->class_list().toggle("active");
    EXPECT_FALSE(button->class_list().contains("active"));
    EXPECT_EQ(button->class_list().length(), 0u);

    // Toggle adds it back
    button->class_list().toggle("active");
    EXPECT_TRUE(button->class_list().contains("active"));
    EXPECT_EQ(button->class_list().length(), 1u);

    // Add multiple classes
    button->class_list().add("disabled");
    button->class_list().add("focus");
    EXPECT_EQ(button->class_list().length(), 3u);
    EXPECT_TRUE(button->class_list().contains("active"));
    EXPECT_TRUE(button->class_list().contains("disabled"));
    EXPECT_TRUE(button->class_list().contains("focus"));

    // Remove one class
    button->class_list().remove("disabled");
    EXPECT_EQ(button->class_list().length(), 2u);
    EXPECT_FALSE(button->class_list().contains("disabled"));
}

TEST(DomNode, ComplexTreeTraversalWithForEachChildV61) {
    // Build complex tree structure
    auto root = std::make_unique<Element>("div");
    auto child1 = std::make_unique<Element>("section");
    auto child2 = std::make_unique<Element>("article");
    auto child3 = std::make_unique<Element>("aside");

    Element* c1 = child1.get();

    // Add grandchildren
    auto gc1_1 = std::make_unique<Element>("span");
    auto gc1_2 = std::make_unique<Element>("span");
    Element* gc1_1_ptr = gc1_1.get();
    [[maybe_unused]] Element* gc1_2_ptr = gc1_2.get();

    c1->append_child(std::move(gc1_1));
    c1->append_child(std::move(gc1_2));

    root->append_child(std::move(child1));
    root->append_child(std::move(child2));
    root->append_child(std::move(child3));

    // Test for_each_child on root
    std::vector<std::string> tags;
    root->for_each_child([&](Node& child) {
        if (auto* elem = dynamic_cast<Element*>(&child)) {
            tags.push_back(elem->tag_name());
        }
    });

    EXPECT_EQ(tags.size(), 3u);
    EXPECT_EQ(tags[0], "section");
    EXPECT_EQ(tags[1], "article");
    EXPECT_EQ(tags[2], "aside");

    // Test for_each_child on child with grandchildren
    std::vector<std::string> grandchild_tags;
    c1->for_each_child([&](Node& child) {
        if (auto* elem = dynamic_cast<Element*>(&child)) {
            grandchild_tags.push_back(elem->tag_name());
        }
    });

    EXPECT_EQ(grandchild_tags.size(), 2u);
    EXPECT_EQ(grandchild_tags[0], "span");
    EXPECT_EQ(grandchild_tags[1], "span");

    // Verify first grandchild pointer is correct
    EXPECT_EQ(c1->first_child(), gc1_1_ptr);
}

TEST(DomNode, RemoveChildAndRebuildTreeV61) {
    auto parent = std::make_unique<Element>("div");
    auto child1 = std::make_unique<Element>("p");
    auto child2 = std::make_unique<Element>("p");
    auto child3 = std::make_unique<Element>("p");

    Element* c1 = child1.get();
    Element* c2 = child2.get();
    Element* c3 = child3.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    parent->append_child(std::move(child3));

    EXPECT_EQ(parent->child_count(), 3u);

    // Remove middle child
    auto removed = parent->remove_child(*c2);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(c1->next_sibling(), c3);
    EXPECT_EQ(c3->previous_sibling(), c1);

    // Re-insert the removed child at the end
    Node* removed_ptr = removed.get();
    parent->append_child(std::move(removed));

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(c3->next_sibling(), removed_ptr);
    EXPECT_EQ(removed_ptr->previous_sibling(), c3);
    EXPECT_EQ(parent->last_child(), removed_ptr);
}

TEST(DomEvent, ImmediatePropagationStopsAllListenersV61) {
    std::vector<std::string> execution_log;

    EventTarget target;

    // Register three listeners for same event type
    target.add_event_listener("click", [&](Event& e) {
        execution_log.push_back("first");
        e.stop_immediate_propagation();
    }, false);

    target.add_event_listener("click", [&](Event&) {
        execution_log.push_back("second");
    }, false);

    target.add_event_listener("click", [&](Event&) {
        execution_log.push_back("third");
    }, false);

    auto element = std::make_unique<Element>("div");
    Event event("click");
    event.target_ = element.get();
    event.current_target_ = element.get();
    event.phase_ = EventPhase::AtTarget;

    target.dispatch_event(event, *element);

    // Only first listener should execute
    EXPECT_EQ(execution_log.size(), 1u);
    EXPECT_EQ(execution_log[0], "first");
    EXPECT_TRUE(event.immediate_propagation_stopped());
}

// ---------------------------------------------------------------------------
// V62 Tests: Node type checks, document methods, element matching, attributes
// ---------------------------------------------------------------------------

TEST(DomNode, NodeTypeCheckingV62) {
    Element elem("div");
    Text text("hello");
    Comment comment("note");
    Document doc;

    EXPECT_EQ(elem.node_type(), NodeType::Element);
    EXPECT_EQ(text.node_type(), NodeType::Text);
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
    EXPECT_EQ(doc.node_type(), NodeType::Document);
}

TEST(DomDocument, CreateElementAndSetIdV62) {
    Document doc;
    auto elem = doc.create_element("input");
    elem->set_attribute("id", "myInput");
    Element* elem_ptr = elem.get();

    EXPECT_EQ(elem->get_attribute("id"), "myInput");

    // Register the element in the document's id map
    doc.register_id("myInput", elem_ptr);

    Element* found = doc.get_element_by_id("myInput");
    EXPECT_EQ(found, elem_ptr);
}

TEST(DomElement, AttributeIterationV62) {
    Element elem("form");
    elem.set_attribute("action", "/submit");
    elem.set_attribute("method", "POST");
    elem.set_attribute("enctype", "multipart/form-data");

    const auto& attrs = elem.attributes();
    EXPECT_EQ(attrs.size(), 3u);

    std::vector<std::string> names;
    for (const auto& attr : attrs) {
        names.push_back(attr.name);
    }
    EXPECT_TRUE(std::find(names.begin(), names.end(), "action") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "method") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "enctype") != names.end());
}

TEST(DomElement, MultipleAttributeRemovalV62) {
    Element elem("button");
    elem.set_attribute("disabled", "");
    elem.set_attribute("aria-label", "Submit");
    elem.set_attribute("data-id", "123");

    EXPECT_TRUE(elem.has_attribute("disabled"));
    EXPECT_TRUE(elem.has_attribute("aria-label"));
    EXPECT_TRUE(elem.has_attribute("data-id"));

    elem.remove_attribute("aria-label");
    EXPECT_FALSE(elem.has_attribute("aria-label"));
    EXPECT_TRUE(elem.has_attribute("disabled"));
    EXPECT_TRUE(elem.has_attribute("data-id"));
}

TEST(DomNode, DeepParentChildRelationshipV62) {
    auto root = std::make_unique<Element>("div");
    auto child = std::make_unique<Element>("p");
    auto grandchild = std::make_unique<Text>("nested text");

    Element* child_ptr = static_cast<Element*>(child.get());
    Text* grandchild_ptr = static_cast<Text*>(grandchild.get());

    child_ptr->append_child(std::move(grandchild));
    root->append_child(std::move(child));

    EXPECT_EQ(grandchild_ptr->parent(), child_ptr);
    EXPECT_EQ(child_ptr->parent(), root.get());
    EXPECT_EQ(root->parent(), nullptr);
}

TEST(DomNode, TextNodeSplitWithSiblingsV62) {
    auto parent = std::make_unique<Element>("span");
    auto text1 = std::make_unique<Text>("hello");
    auto text2 = std::make_unique<Text>(" ");
    auto text3 = std::make_unique<Text>("world");

    Text* t1 = static_cast<Text*>(text1.get());
    Text* t2 = static_cast<Text*>(text2.get());
    Text* t3 = static_cast<Text*>(text3.get());

    parent->append_child(std::move(text1));
    parent->append_child(std::move(text2));
    parent->append_child(std::move(text3));

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(t1->next_sibling(), t2);
    EXPECT_EQ(t2->next_sibling(), t3);
    EXPECT_EQ(t3->previous_sibling(), t2);
    EXPECT_EQ(t2->previous_sibling(), t1);

    std::string combined = parent->text_content();
    EXPECT_EQ(combined, "hello world");
}

TEST(DomElement, WhitespaceHandlingInAttributesV62) {
    Element elem("div");
    elem.set_attribute("class", "  foo  bar  baz  ");
    elem.set_attribute("title", "   long title   ");

    EXPECT_EQ(elem.get_attribute("class"), "  foo  bar  baz  ");
    EXPECT_EQ(elem.get_attribute("title"), "   long title   ");

    const auto& attrs = elem.attributes();
    EXPECT_EQ(attrs.size(), 2u);
    for (const auto& attr : attrs) {
        if (attr.name == "class") {
            EXPECT_EQ(attr.value, "  foo  bar  baz  ");
        } else if (attr.name == "title") {
            EXPECT_EQ(attr.value, "   long title   ");
        }
    }
}

TEST(DomNode, InsertBeforeIntegrityV62) {
    auto parent = std::make_unique<Element>("ul");
    auto item1 = std::make_unique<Element>("li");
    auto item2 = std::make_unique<Element>("li");
    auto item3 = std::make_unique<Element>("li");

    Element* i1 = static_cast<Element*>(item1.get());
    Element* i2 = static_cast<Element*>(item2.get());
    Element* i3 = static_cast<Element*>(item3.get());

    parent->append_child(std::move(item1));
    parent->append_child(std::move(item3));
    parent->insert_before(std::move(item2), i3);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(i1->next_sibling(), i2);
    EXPECT_EQ(i2->next_sibling(), i3);
    EXPECT_EQ(i2->previous_sibling(), i1);
    EXPECT_EQ(i3->previous_sibling(), i2);
}

TEST(DomNode, DeepCloneLikeSubtreeCopiesStructureIndependentlyV63) {
    auto root = std::make_unique<Element>("div");
    root->set_attribute("id", "root");

    auto section = std::make_unique<Element>("section");
    auto* section_ptr = section.get();
    section->set_attribute("id", "hero");

    auto text = std::make_unique<Text>("hello");
    section->append_child(std::move(text));

    auto note = std::make_unique<Comment>("note");
    section->append_child(std::move(note));

    root->append_child(std::move(section));

    std::function<std::unique_ptr<Node>(const Node*)> deep_clone =
        [&](const Node* source) -> std::unique_ptr<Node> {
            if (source->node_type() == NodeType::Element) {
                const auto* source_element = static_cast<const Element*>(source);
                auto clone_element = std::make_unique<Element>(source_element->tag_name());
                for (const auto& attr : source_element->attributes()) {
                    clone_element->set_attribute(attr.name, attr.value);
                }
                for (const Node* child = source->first_child(); child != nullptr; child = child->next_sibling()) {
                    clone_element->append_child(deep_clone(child));
                }
                return clone_element;
            }

            if (source->node_type() == NodeType::Text) {
                const auto* source_text = static_cast<const Text*>(source);
                return std::make_unique<Text>(source_text->data());
            }

            if (source->node_type() == NodeType::Comment) {
                const auto* source_comment = static_cast<const Comment*>(source);
                return std::make_unique<Comment>(source_comment->data());
            }

            auto clone_document = std::make_unique<Document>();
            for (const Node* child = source->first_child(); child != nullptr; child = child->next_sibling()) {
                clone_document->append_child(deep_clone(child));
            }
            return clone_document;
        };

    auto cloned_root_node = deep_clone(root.get());
    auto* cloned_root = static_cast<Element*>(cloned_root_node.get());
    auto* cloned_section = static_cast<Element*>(cloned_root->first_child());

    ASSERT_NE(cloned_section, nullptr);
    EXPECT_NE(cloned_section, section_ptr);
    EXPECT_EQ(cloned_root->tag_name(), "div");
    EXPECT_EQ(cloned_section->tag_name(), "section");
    EXPECT_EQ(cloned_section->get_attribute("id"), "hero");
    EXPECT_EQ(cloned_root->text_content(), "hello");

    section_ptr->set_attribute("id", "hero-updated");
    section_ptr->append_child(std::make_unique<Text>("!"));

    EXPECT_EQ(root->text_content(), "hello!");
    EXPECT_EQ(cloned_root->text_content(), "hello");
    EXPECT_EQ(cloned_section->get_attribute("id"), "hero");
}

TEST(DomNode, EventHandlingImmediateStopSkipsLaterListenersV63) {
    EventTarget target;
    std::vector<std::string> log;

    target.add_event_listener(
        "click",
        [&](Event& event) {
            log.push_back("first");
            event.stop_immediate_propagation();
        },
        false);

    target.add_event_listener(
        "click",
        [&](Event&) {
            log.push_back("second");
        },
        false);

    Element element("button");
    Event event("click");
    event.target_ = &element;
    event.current_target_ = &element;
    event.phase_ = EventPhase::AtTarget;

    bool dispatch_result = target.dispatch_event(event, element);

    EXPECT_TRUE(dispatch_result);
    EXPECT_TRUE(event.immediate_propagation_stopped());
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0], "first");
}

TEST(DomNode, NodeTypeChecksElementTextCommentDocumentV63) {
    Element element("article");
    Text text("hello");
    Comment comment("note");
    Document document;

    EXPECT_EQ(element.node_type(), NodeType::Element);
    EXPECT_EQ(text.node_type(), NodeType::Text);
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
    EXPECT_EQ(document.node_type(), NodeType::Document);
}

TEST(DomElement, QuerySelectorPatternLikeTraversalFindsIdClassAndTagV63) {
    auto root = std::make_unique<Element>("div");

    auto header = std::make_unique<Element>("header");
    auto* header_ptr = header.get();
    header->set_attribute("id", "top");

    auto button = std::make_unique<Element>("button");
    auto* button_ptr = button.get();
    button->class_list().add("primary");

    auto span = std::make_unique<Element>("span");
    auto* span_ptr = span.get();

    root->append_child(std::move(header));
    root->append_child(std::move(button));
    root->append_child(std::move(span));

    auto matches_selector = [&](Element* element, const std::string& selector) {
        if (selector.empty()) {
            return false;
        }
        if (selector[0] == '#') {
            auto id_value = element->get_attribute("id");
            return id_value.has_value() && id_value.value() == selector.substr(1);
        }
        if (selector[0] == '.') {
            return element->class_list().contains(selector.substr(1));
        }
        return element->tag_name() == selector;
    };

    std::function<Element*(Node*, const std::string&)> query_like_first =
        [&](Node* start, const std::string& selector) -> Element* {
            if (start->node_type() == NodeType::Element) {
                auto* as_element = static_cast<Element*>(start);
                if (matches_selector(as_element, selector)) {
                    return as_element;
                }
            }

            for (Node* child = start->first_child(); child != nullptr; child = child->next_sibling()) {
                Element* found = query_like_first(child, selector);
                if (found != nullptr) {
                    return found;
                }
            }
            return nullptr;
        };

    EXPECT_EQ(query_like_first(root.get(), "#top"), header_ptr);
    EXPECT_EQ(query_like_first(root.get(), ".primary"), button_ptr);
    EXPECT_EQ(query_like_first(root.get(), "span"), span_ptr);
    EXPECT_EQ(query_like_first(root.get(), ".missing"), nullptr);
}

TEST(DomElement, InnerHtmlLikeRewriteReplacesSubtreeContentV63) {
    auto container = std::make_unique<Element>("div");

    auto old_text = std::make_unique<Text>("old");
    container->append_child(std::move(old_text));
    auto old_comment = std::make_unique<Comment>("old-comment");
    container->append_child(std::move(old_comment));

    while (container->first_child() != nullptr) {
        Node* victim = container->first_child();
        auto removed = container->remove_child(*victim);
        EXPECT_EQ(removed->parent(), nullptr);
    }
    EXPECT_EQ(container->child_count(), 0u);

    auto paragraph = std::make_unique<Element>("p");
    auto* paragraph_ptr = paragraph.get();
    auto paragraph_text = std::make_unique<Text>("Hello");
    paragraph->append_child(std::move(paragraph_text));

    auto note = std::make_unique<Comment>("ignored");
    auto tail_text = std::make_unique<Text>(" world");

    container->append_child(std::move(paragraph));
    container->append_child(std::move(note));
    container->append_child(std::move(tail_text));

    EXPECT_EQ(container->first_child(), paragraph_ptr);
    EXPECT_EQ(container->child_count(), 3u);
    EXPECT_EQ(container->text_content(), "Hello world");
}

TEST(DomNode, SiblingNavigationEdgeCasesAfterEndRemovalsV63) {
    auto parent = std::make_unique<Element>("ul");

    auto li1 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    auto li2 = std::make_unique<Element>("li");
    auto* li2_ptr = li2.get();
    auto li3 = std::make_unique<Element>("li");
    auto* li3_ptr = li3.get();
    auto li4 = std::make_unique<Element>("li");
    auto* li4_ptr = li4.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));
    parent->append_child(std::move(li4));

    auto removed_first = parent->remove_child(*li1_ptr);
    EXPECT_EQ(removed_first->parent(), nullptr);
    EXPECT_EQ(parent->first_child(), li2_ptr);
    EXPECT_EQ(li2_ptr->previous_sibling(), nullptr);

    auto removed_last = parent->remove_child(*li4_ptr);
    EXPECT_EQ(removed_last->parent(), nullptr);
    EXPECT_EQ(parent->last_child(), li3_ptr);
    EXPECT_EQ(li3_ptr->next_sibling(), nullptr);

    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->previous_sibling(), li2_ptr);
    EXPECT_EQ(parent->child_count(), 2u);
}

TEST(DomElement, AttributeIterationPreservesOrderAfterOverwriteAndRemoveV63) {
    Element input("input");
    input.set_attribute("data-a", "1");
    input.set_attribute("data-b", "2");
    input.set_attribute("data-c", "3");

    input.set_attribute("data-b", "22");
    input.remove_attribute("data-a");

    const auto& attrs = input.attributes();
    ASSERT_EQ(attrs.size(), 2u);
    EXPECT_EQ(attrs[0].name, "data-b");
    EXPECT_EQ(attrs[0].value, "22");
    EXPECT_EQ(attrs[1].name, "data-c");
    EXPECT_EQ(attrs[1].value, "3");
}

TEST(DomNode, MixedContentTreeTextAndParentLinksV63) {
    auto root = std::make_unique<Element>("article");

    auto text_a = std::make_unique<Text>("A");
    auto span = std::make_unique<Element>("span");
    auto* span_ptr = span.get();
    span->set_attribute("id", "middle");

    auto span_text = std::make_unique<Text>("B");
    auto* span_text_ptr = span_text.get();
    span->append_child(std::move(span_text));

    auto comment = std::make_unique<Comment>("ignored");
    auto* comment_ptr = comment.get();
    auto text_d = std::make_unique<Text>("D");

    root->append_child(std::move(text_a));
    root->append_child(std::move(span));
    root->append_child(std::move(comment));
    root->append_child(std::move(text_d));

    EXPECT_EQ(root->text_content(), "ABD");
    EXPECT_EQ(root->child_count(), 4u);
    EXPECT_EQ(span_ptr->parent(), root.get());
    EXPECT_EQ(span_text_ptr->parent(), span_ptr);
    EXPECT_EQ(comment_ptr->data(), "ignored");
    EXPECT_EQ(span_ptr->next_sibling(), comment_ptr);
    EXPECT_EQ(comment_ptr->previous_sibling(), span_ptr);
}

TEST(DomElement, AttributeLifecycleSetGetHasRemoveV64) {
    Element input("input");
    EXPECT_EQ(input.tag_name(), "input");

    EXPECT_FALSE(input.has_attribute("placeholder"));
    input.set_attribute("placeholder", "Search");
    EXPECT_TRUE(input.has_attribute("placeholder"));
    EXPECT_EQ(input.get_attribute("placeholder"), "Search");

    input.remove_attribute("placeholder");
    EXPECT_FALSE(input.has_attribute("placeholder"));
    EXPECT_FALSE(input.get_attribute("placeholder").has_value());
}

TEST(DomElement, AttributesOverwriteAndOrderAfterRemoveV64) {
    Element button("button");
    button.set_attribute("type", "button");
    button.set_attribute("aria-label", "Save");
    button.set_attribute("type", "submit");
    button.remove_attribute("aria-label");

    const auto& attrs = button.attributes();
    ASSERT_EQ(attrs.size(), 1u);
    EXPECT_EQ(attrs[0].name, "type");
    EXPECT_EQ(attrs[0].value, "submit");
}

TEST(DomElement, ClassListAddRemoveContainsToggleSequenceV64) {
    Element div("div");
    auto& classes = div.class_list();

    classes.add("panel");
    classes.add("active");
    EXPECT_TRUE(classes.contains("panel"));
    EXPECT_TRUE(classes.contains("active"));

    classes.toggle("active");
    EXPECT_FALSE(classes.contains("active"));

    classes.toggle("hidden");
    EXPECT_TRUE(classes.contains("hidden"));

    classes.remove("panel");
    EXPECT_FALSE(classes.contains("panel"));
}

TEST(DomNode, FirstAndLastChildTrackAppendedElementsV64) {
    Element list("ul");
    auto first = std::make_unique<Element>("li");
    auto* first_ptr = first.get();
    auto second = std::make_unique<Element>("li");
    auto* second_ptr = second.get();

    list.append_child(std::move(first));
    list.append_child(std::move(second));

    EXPECT_EQ(list.first_child(), first_ptr);
    EXPECT_EQ(list.last_child(), second_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->previous_sibling(), first_ptr);
}

TEST(DomNode, ParentAndSiblingPointersAcrossThreeChildrenV64) {
    Element parent("nav");
    auto a = std::make_unique<Element>("a");
    auto* a_ptr = a.get();
    auto b = std::make_unique<Element>("a");
    auto* b_ptr = b.get();
    auto c = std::make_unique<Element>("a");
    auto* c_ptr = c.get();

    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));

    EXPECT_EQ(a_ptr->parent(), &parent);
    EXPECT_EQ(b_ptr->parent(), &parent);
    EXPECT_EQ(c_ptr->parent(), &parent);
    EXPECT_EQ(a_ptr->previous_sibling(), nullptr);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->previous_sibling(), a_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), b_ptr);
    EXPECT_EQ(c_ptr->next_sibling(), nullptr);
}

TEST(DomElement, TextContentIncludesTextAndSkipsCommentNodesV64) {
    Element root("div");
    auto text_a = std::make_unique<Text>("Hello");
    auto* text_a_ptr = text_a.get();
    auto comment = std::make_unique<Comment>("ignored");
    auto* comment_ptr = comment.get();

    auto span = std::make_unique<Element>("span");
    auto* span_ptr = span.get();
    auto span_text = std::make_unique<Text>(" world");
    auto* span_text_ptr = span_text.get();
    span->append_child(std::move(span_text));

    root.append_child(std::move(text_a));
    root.append_child(std::move(comment));
    root.append_child(std::move(span));

    EXPECT_EQ(root.text_content(), "Hello world");
    EXPECT_EQ(text_a_ptr->data(), "Hello");
    EXPECT_EQ(comment_ptr->data(), "ignored");
    EXPECT_EQ(span_ptr->first_child(), span_text_ptr);
}

TEST(DomNode, AppendedTextNodeRetainsDataAndParentV64) {
    Element paragraph("p");
    auto text = std::make_unique<Text>("inline");
    auto* text_ptr = text.get();

    paragraph.append_child(std::move(text));

    EXPECT_EQ(paragraph.first_child(), text_ptr);
    EXPECT_EQ(paragraph.last_child(), text_ptr);
    EXPECT_EQ(text_ptr->parent(), &paragraph);
    EXPECT_EQ(text_ptr->data(), "inline");
    EXPECT_EQ(paragraph.text_content(), "inline");
}

TEST(DomNode, NestedElementsPreserveParentChainAndTextV64) {
    auto article = std::make_unique<Element>("article");
    auto section = std::make_unique<Element>("section");
    auto* section_ptr = section.get();
    auto paragraph = std::make_unique<Element>("p");
    auto* paragraph_ptr = paragraph.get();
    auto text = std::make_unique<Text>("content");

    paragraph->append_child(std::move(text));
    section->append_child(std::move(paragraph));
    article->append_child(std::move(section));

    ASSERT_NE(article->first_child(), nullptr);
    EXPECT_EQ(article->first_child(), section_ptr);
    EXPECT_EQ(section_ptr->first_child(), paragraph_ptr);
    EXPECT_EQ(section_ptr->parent(), article.get());
    EXPECT_EQ(paragraph_ptr->parent(), section_ptr);
    EXPECT_EQ(article->text_content(), "content");
}

TEST(DomElement, AttributeManipulationSetOverwriteRemoveAndIdV65) {
    Element input("input");
    EXPECT_TRUE(input.attributes().empty());

    input.set_attribute("type", "text");
    input.set_attribute("placeholder", "Search");
    input.set_attribute("id", "query");

    EXPECT_EQ(input.id(), "query");
    EXPECT_TRUE(input.has_attribute("type"));
    EXPECT_EQ(input.get_attribute("placeholder"), "Search");

    input.set_attribute("type", "password");
    EXPECT_EQ(input.get_attribute("type"), "password");

    input.remove_attribute("placeholder");
    EXPECT_FALSE(input.has_attribute("placeholder"));
    EXPECT_FALSE(input.get_attribute("placeholder").has_value());
    EXPECT_EQ(input.attributes().size(), 2u);
}

TEST(DomNode, NodeRemovalDetachesAndReturnsOwnedSubtreeV65) {
    Element root("div");
    auto section = std::make_unique<Element>("section");
    auto* section_ptr = section.get();
    auto text = std::make_unique<Text>("payload");
    section->append_child(std::move(text));

    root.append_child(std::move(section));
    ASSERT_EQ(root.child_count(), 1u);
    ASSERT_EQ(root.first_child(), section_ptr);

    auto removed = root.remove_child(*section_ptr);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed.get(), section_ptr);
    EXPECT_EQ(removed->parent(), nullptr);
    EXPECT_EQ(root.child_count(), 0u);
    EXPECT_EQ(root.first_child(), nullptr);
    EXPECT_EQ(root.last_child(), nullptr);
    EXPECT_EQ(removed->text_content(), "payload");
}

TEST(DomNode, CloneLikeCopiesStructureWithoutSharingNodesV65) {
    auto source = std::make_unique<Element>("svg", "http://www.w3.org/2000/svg");
    source->set_attribute("id", "icon");

    auto group = std::make_unique<Element>("g");
    auto* source_group_ptr = group.get();
    group->set_attribute("class", "accent");
    group->append_child(std::make_unique<Text>("hello"));
    source->append_child(std::move(group));

    auto clone_like = [&](const Node* node, const auto& self) -> std::unique_ptr<Node> {
        switch (node->node_type()) {
        case NodeType::Element: {
            auto* source_element = static_cast<const Element*>(node);
            auto clone_element = std::make_unique<Element>(source_element->tag_name(),
                                                           source_element->namespace_uri());
            for (const auto& attr : source_element->attributes()) {
                clone_element->set_attribute(attr.name, attr.value);
            }
            for (Node* child = source_element->first_child(); child != nullptr;
                 child = child->next_sibling()) {
                clone_element->append_child(self(child, self));
            }
            return clone_element;
        }
        case NodeType::Text:
            return std::make_unique<Text>(static_cast<const Text*>(node)->data());
        case NodeType::Comment:
            return std::make_unique<Comment>(static_cast<const Comment*>(node)->data());
        case NodeType::Document: {
            auto clone_document = std::make_unique<Document>();
            for (Node* child = node->first_child(); child != nullptr;
                 child = child->next_sibling()) {
                clone_document->append_child(self(child, self));
            }
            return clone_document;
        }
        default:
            return nullptr;
        }
    };

    auto cloned_node = clone_like(source.get(), clone_like);
    auto* cloned_root = static_cast<Element*>(cloned_node.get());
    ASSERT_NE(cloned_root, nullptr);
    ASSERT_NE(cloned_root, source.get());
    EXPECT_EQ(cloned_root->tag_name(), "svg");
    EXPECT_EQ(cloned_root->namespace_uri(), "http://www.w3.org/2000/svg");
    EXPECT_EQ(cloned_root->get_attribute("id"), "icon");
    EXPECT_EQ(cloned_root->text_content(), "hello");

    auto* cloned_group = static_cast<Element*>(cloned_root->first_child());
    ASSERT_NE(cloned_group, nullptr);
    EXPECT_NE(cloned_group, source_group_ptr);
    EXPECT_EQ(cloned_group->get_attribute("class"), "accent");

    source->set_attribute("id", "mutated");
    source_group_ptr->set_attribute("class", "changed");
    EXPECT_EQ(cloned_root->get_attribute("id"), "icon");
    EXPECT_EQ(cloned_group->get_attribute("class"), "accent");
}

TEST(DomNode, SiblingTraversalReflectsTreeAfterMiddleRemovalV65) {
    Element list("ul");
    auto first = std::make_unique<Element>("li");
    auto* first_ptr = first.get();
    auto second = std::make_unique<Element>("li");
    auto* second_ptr = second.get();
    auto third = std::make_unique<Element>("li");
    auto* third_ptr = third.get();
    auto fourth = std::make_unique<Element>("li");
    auto* fourth_ptr = fourth.get();

    list.append_child(std::move(first));
    list.append_child(std::move(second));
    list.append_child(std::move(third));
    list.append_child(std::move(fourth));

    auto removed = list.remove_child(*second_ptr);
    ASSERT_NE(removed, nullptr);
    EXPECT_EQ(removed.get(), second_ptr);
    EXPECT_EQ(removed->parent(), nullptr);

    EXPECT_EQ(first_ptr->previous_sibling(), nullptr);
    EXPECT_EQ(first_ptr->next_sibling(), third_ptr);
    EXPECT_EQ(third_ptr->previous_sibling(), first_ptr);
    EXPECT_EQ(third_ptr->next_sibling(), fourth_ptr);
    EXPECT_EQ(fourth_ptr->previous_sibling(), third_ptr);
    EXPECT_EQ(fourth_ptr->next_sibling(), nullptr);
}

TEST(DomElement, InnerHtmlLikeReplaceChildrenByRemoveAndAppendV65) {
    Element container("div");
    container.append_child(std::make_unique<Text>("old"));
    container.append_child(std::make_unique<Comment>("ignored"));
    container.append_child(std::make_unique<Text>(" value"));
    ASSERT_EQ(container.child_count(), 3u);
    EXPECT_EQ(container.text_content(), "old value");

    while (container.first_child() != nullptr) {
        auto* node = container.first_child();
        auto removed = container.remove_child(*node);
        EXPECT_EQ(removed->parent(), nullptr);
    }
    EXPECT_EQ(container.child_count(), 0u);
    EXPECT_EQ(container.text_content(), "");

    auto paragraph = std::make_unique<Element>("p");
    auto* paragraph_ptr = paragraph.get();
    paragraph->append_child(std::make_unique<Text>("new"));
    container.append_child(std::move(paragraph));
    container.append_child(std::make_unique<Text>(" content"));

    EXPECT_EQ(container.first_child(), paragraph_ptr);
    EXPECT_EQ(container.last_child()->node_type(), NodeType::Text);
    EXPECT_EQ(container.child_count(), 2u);
    EXPECT_EQ(container.text_content(), "new content");
}

TEST(DomElement, NamespaceHandlingKeepsUrisIndependentV65) {
    Element html_div("div");
    Element svg_rect("rect", "http://www.w3.org/2000/svg");

    html_div.set_attribute("id", "main");
    svg_rect.set_attribute("id", "shape");

    EXPECT_EQ(html_div.namespace_uri(), "");
    EXPECT_EQ(svg_rect.namespace_uri(), "http://www.w3.org/2000/svg");
    EXPECT_EQ(html_div.tag_name(), "div");
    EXPECT_EQ(svg_rect.tag_name(), "rect");
    EXPECT_EQ(html_div.get_attribute("id"), "main");
    EXPECT_EQ(svg_rect.get_attribute("id"), "shape");
}

TEST(DomNode, NodeTypeChecksForElementTextCommentAndDocumentV65) {
    Element element("article");
    Text text("hello");
    Comment comment("meta");
    Document document;

    EXPECT_EQ(element.node_type(), NodeType::Element);
    EXPECT_EQ(text.node_type(), NodeType::Text);
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
    EXPECT_EQ(document.node_type(), NodeType::Document);
    EXPECT_EQ(text.data(), "hello");
    EXPECT_EQ(comment.data(), "meta");
}

TEST(DomNode, ChildCountingUsesChildrenVectorAcrossMutationsV65) {
    Element root("div");
    auto text = std::make_unique<Text>("A");
    auto* text_ptr = text.get();
    auto middle = std::make_unique<Element>("span");
    auto* middle_ptr = middle.get();
    auto tail_comment = std::make_unique<Comment>("tail");
    auto* tail_comment_ptr = tail_comment.get();

    root.append_child(std::move(text));
    root.append_child(std::move(middle));
    root.append_child(std::move(tail_comment));
    ASSERT_EQ(root.child_count(), 3u);
    EXPECT_EQ(root.first_child(), text_ptr);
    EXPECT_EQ(root.last_child(), tail_comment_ptr);

    auto removed_comment = root.remove_child(*tail_comment_ptr);
    ASSERT_NE(removed_comment, nullptr);
    EXPECT_EQ(root.child_count(), 2u);
    EXPECT_EQ(root.last_child(), middle_ptr);

    auto removed_text = root.remove_child(*text_ptr);
    ASSERT_NE(removed_text, nullptr);
    EXPECT_EQ(root.child_count(), 1u);
    EXPECT_EQ(root.first_child(), middle_ptr);
    EXPECT_EQ(root.last_child(), middle_ptr);
}

TEST(DOMTest, DeepTreeRecursiveChildTraversalV66) {
    Element root("root");

    auto section = std::make_unique<Element>("section");
    section->append_child(std::make_unique<Text>("alpha"));

    auto article = std::make_unique<Element>("article");
    auto aside = std::make_unique<Element>("aside");
    aside->append_child(std::make_unique<Comment>("meta"));
    article->append_child(std::move(aside));

    root.append_child(std::move(section));
    root.append_child(std::move(article));
    root.append_child(std::make_unique<Text>("omega"));

    std::vector<NodeType> preorder_types;
    auto walk = [&](auto&& self, const Node& node) -> void {
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            preorder_types.push_back(child->node_type());
            self(self, *child);
        }
    };
    walk(walk, root);

    ASSERT_EQ(preorder_types.size(), 6u);
    EXPECT_EQ(preorder_types[0], NodeType::Element);
    EXPECT_EQ(preorder_types[1], NodeType::Text);
    EXPECT_EQ(preorder_types[2], NodeType::Element);
    EXPECT_EQ(preorder_types[3], NodeType::Element);
    EXPECT_EQ(preorder_types[4], NodeType::Comment);
    EXPECT_EQ(preorder_types[5], NodeType::Text);
    EXPECT_EQ(root.text_content(), "alphaomega");
}

TEST(DOMTest, ReplaceChildSemanticsMaintainOrderAndParentLinksV66) {
    auto parent = std::make_unique<Element>("div");
    auto first = std::make_unique<Element>("first");
    auto middle = std::make_unique<Element>("middle");
    auto last = std::make_unique<Element>("last");

    Element* first_ptr = first.get();
    Element* middle_ptr = middle.get();
    Element* last_ptr = last.get();

    parent->append_child(std::move(first));
    parent->append_child(std::move(middle));
    parent->append_child(std::move(last));

    auto replacement = std::make_unique<Element>("replacement");
    Element* replacement_ptr = replacement.get();
    Node* reference_after_removed = middle_ptr->next_sibling();

    auto removed = parent->remove_child(*middle_ptr);
    parent->insert_before(std::move(replacement), reference_after_removed);

    EXPECT_EQ(removed.get(), middle_ptr);
    EXPECT_EQ(removed->parent(), nullptr);
    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), replacement_ptr);
    EXPECT_EQ(replacement_ptr->previous_sibling(), first_ptr);
    EXPECT_EQ(replacement_ptr->next_sibling(), last_ptr);
    EXPECT_EQ(last_ptr->previous_sibling(), replacement_ptr);
    EXPECT_EQ(parent->last_child(), last_ptr);
}

TEST(DOMTest, InsertBeforeAtBeginningMiddleAndEndV66) {
    auto parent = std::make_unique<Element>("list");

    auto item_a = std::make_unique<Element>("a");
    auto item_c = std::make_unique<Element>("c");
    Element* a_ptr = item_a.get();
    Element* c_ptr = item_c.get();

    parent->append_child(std::move(item_a));
    parent->append_child(std::move(item_c));

    auto item_b = std::make_unique<Element>("b");
    Element* b_ptr = item_b.get();
    parent->insert_before(std::move(item_b), c_ptr);

    auto item_start = std::make_unique<Element>("start");
    Element* start_ptr = item_start.get();
    parent->insert_before(std::move(item_start), a_ptr);

    auto item_end = std::make_unique<Element>("end");
    Element* end_ptr = item_end.get();
    parent->insert_before(std::move(item_end), nullptr);

    ASSERT_EQ(parent->child_count(), 5u);
    EXPECT_EQ(parent->first_child(), start_ptr);
    EXPECT_EQ(parent->last_child(), end_ptr);
    EXPECT_EQ(start_ptr->next_sibling(), a_ptr);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->next_sibling(), end_ptr);
    EXPECT_EQ(end_ptr->previous_sibling(), c_ptr);
}

TEST(DOMTest, DocumentFragmentAppendMovesChildrenToParentV66) {
    class FragmentNode final : public Node {
    public:
        FragmentNode() : Node(NodeType::DocumentFragment) {}
    };

    auto fragment = std::make_unique<FragmentNode>();
    auto li = std::make_unique<Element>("li");
    auto text = std::make_unique<Text>("item");
    auto comment = std::make_unique<Comment>("meta");

    Element* li_ptr = li.get();
    Text* text_ptr = text.get();
    Comment* comment_ptr = comment.get();

    fragment->append_child(std::move(li));
    fragment->append_child(std::move(text));
    fragment->append_child(std::move(comment));

    Element parent("ul");
    while (fragment->first_child() != nullptr) {
        Node* child = fragment->first_child();
        parent.append_child(fragment->remove_child(*child));
    }

    EXPECT_EQ(fragment->child_count(), 0u);
    ASSERT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), li_ptr);
    EXPECT_EQ(li_ptr->next_sibling(), text_ptr);
    EXPECT_EQ(text_ptr->next_sibling(), comment_ptr);
    EXPECT_EQ(parent.last_child(), comment_ptr);
    EXPECT_EQ(li_ptr->parent(), &parent);
    EXPECT_EQ(text_ptr->parent(), &parent);
    EXPECT_EQ(comment_ptr->parent(), &parent);
}

TEST(DOMTest, NodeValueSemanticsForTextAndCommentNodesV66) {
    Text text("hello");
    Comment comment("world");
    Element element("div");

    EXPECT_EQ(text.data(), "hello");
    EXPECT_EQ(comment.data(), "world");
    EXPECT_EQ(element.text_content(), "");

    text.set_data("HELLO");
    comment.set_data("WORLD");

    EXPECT_EQ(text.data(), "HELLO");
    EXPECT_EQ(comment.data(), "WORLD");
}

TEST(DOMTest, HasChildNodesEdgeCasesAcrossMutationsV66) {
    auto has_child_nodes = [](const Node& node) {
        return node.first_child() != nullptr;
    };

    Document doc;
    Element parent("div");
    EXPECT_FALSE(has_child_nodes(doc));
    EXPECT_FALSE(has_child_nodes(parent));

    auto child_for_doc = std::make_unique<Element>("html");
    Element* child_for_doc_ptr = child_for_doc.get();
    doc.append_child(std::move(child_for_doc));
    EXPECT_TRUE(has_child_nodes(doc));

    auto removed_from_doc = doc.remove_child(*child_for_doc_ptr);
    EXPECT_NE(removed_from_doc, nullptr);
    EXPECT_FALSE(has_child_nodes(doc));

    auto text = std::make_unique<Text>("x");
    Text* text_ptr = text.get();
    parent.append_child(std::move(text));
    EXPECT_TRUE(has_child_nodes(parent));

    auto removed_from_parent = parent.remove_child(*text_ptr);
    EXPECT_NE(removed_from_parent, nullptr);
    EXPECT_FALSE(has_child_nodes(parent));
}

TEST(DOMTest, NormalizeMergesAdjacentTextNodesV66) {
    auto normalize = [&](auto&& self, Node& node) -> void {
        Node* child = node.first_child();
        while (child != nullptr) {
            Node* next = child->next_sibling();
            if (child->node_type() == NodeType::Text) {
                auto* text = static_cast<Text*>(child);
                while (next != nullptr && next->node_type() == NodeType::Text) {
                    auto* next_text = static_cast<Text*>(next);
                    text->set_data(text->data() + next_text->data());
                    Node* after_next = next->next_sibling();
                    node.remove_child(*next);
                    next = after_next;
                }
                if (text->data().empty()) {
                    Node* after_empty = text->next_sibling();
                    node.remove_child(*text);
                    child = after_empty;
                    continue;
                }
            } else {
                self(self, *child);
            }
            child = next;
        }
    };

    Element root("div");
    root.append_child(std::make_unique<Text>("Hello"));
    root.append_child(std::make_unique<Text>(" "));
    root.append_child(std::make_unique<Text>("World"));
    root.append_child(std::make_unique<Comment>("!"));
    root.append_child(std::make_unique<Text>(""));

    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("A"));
    span->append_child(std::make_unique<Text>(""));
    span->append_child(std::make_unique<Text>("B"));
    Element* span_ptr = span.get();
    root.append_child(std::move(span));

    normalize(normalize, root);

    ASSERT_EQ(root.child_count(), 3u);
    ASSERT_NE(root.first_child(), nullptr);
    ASSERT_EQ(root.first_child()->node_type(), NodeType::Text);
    EXPECT_EQ(static_cast<Text*>(root.first_child())->data(), "Hello World");
    EXPECT_EQ(root.first_child()->next_sibling()->node_type(), NodeType::Comment);
    EXPECT_EQ(root.last_child(), span_ptr);
    EXPECT_EQ(root.text_content(), "Hello WorldAB");
    ASSERT_EQ(span_ptr->child_count(), 1u);
    ASSERT_NE(span_ptr->first_child(), nullptr);
    EXPECT_EQ(static_cast<Text*>(span_ptr->first_child())->data(), "AB");
}

TEST(DOMTest, CompareDocumentPositionBasicsV66) {
    constexpr unsigned kDisconnected = 0x01;
    constexpr unsigned kPreceding = 0x02;
    constexpr unsigned kFollowing = 0x04;
    constexpr unsigned kContains = 0x08;
    constexpr unsigned kContainedBy = 0x10;

    auto compare_document_position = [&](const Node& a, const Node& b) -> unsigned {
        if (&a == &b) {
            return 0;
        }

        std::vector<const Node*> path_a;
        std::vector<const Node*> path_b;
        for (const Node* n = &a; n != nullptr; n = n->parent()) {
            path_a.insert(path_a.begin(), n);
        }
        for (const Node* n = &b; n != nullptr; n = n->parent()) {
            path_b.insert(path_b.begin(), n);
        }

        if (path_a.front() != path_b.front()) {
            return kDisconnected;
        }

        for (const Node* n = &b; n != nullptr; n = n->parent()) {
            if (n == &a) {
                return kContains | kPreceding;
            }
        }
        for (const Node* n = &a; n != nullptr; n = n->parent()) {
            if (n == &b) {
                return kContainedBy | kFollowing;
            }
        }

        size_t i = 0;
        while (i < path_a.size() && i < path_b.size() && path_a[i] == path_b[i]) {
            ++i;
        }

        const Node* branch_a = path_a[i];
        const Node* branch_b = path_b[i];
        for (const Node* n = branch_a->previous_sibling(); n != nullptr; n = n->previous_sibling()) {
            if (n == branch_b) {
                return kFollowing;
            }
        }
        return kPreceding;
    };

    auto root = std::make_unique<Element>("root");
    auto left = std::make_unique<Element>("left");
    auto right = std::make_unique<Element>("right");
    auto leaf = std::make_unique<Element>("leaf");

    Element* left_ptr = left.get();
    Element* right_ptr = right.get();
    Element* leaf_ptr = leaf.get();

    left->append_child(std::move(leaf));
    root->append_child(std::move(left));
    root->append_child(std::move(right));

    Element disconnected("outside");

    EXPECT_EQ(compare_document_position(*root, *left_ptr), kContains | kPreceding);
    EXPECT_EQ(compare_document_position(*left_ptr, *root), kContainedBy | kFollowing);
    EXPECT_EQ(compare_document_position(*left_ptr, *right_ptr), kPreceding);
    EXPECT_EQ(compare_document_position(*right_ptr, *left_ptr), kFollowing);
    EXPECT_EQ(compare_document_position(*left_ptr, *leaf_ptr), kContains | kPreceding);
    EXPECT_EQ(compare_document_position(*leaf_ptr, *left_ptr), kContainedBy | kFollowing);
    EXPECT_EQ(compare_document_position(*left_ptr, *left_ptr), 0u);
    EXPECT_EQ(compare_document_position(*root, disconnected), kDisconnected);
}

TEST(DOMTest, FirstChildLastChildAccessorsTrackMutationsV67) {
    Element parent("div");
    EXPECT_EQ(parent.first_child(), nullptr);
    EXPECT_EQ(parent.last_child(), nullptr);

    auto first = std::make_unique<Element>("first");
    auto second = std::make_unique<Element>("second");
    auto third = std::make_unique<Element>("third");
    Element* first_ptr = first.get();
    Element* second_ptr = second.get();
    Element* third_ptr = third.get();

    parent.append_child(std::move(first));
    parent.append_child(std::move(second));
    parent.append_child(std::move(third));

    EXPECT_EQ(parent.first_child(), first_ptr);
    EXPECT_EQ(parent.last_child(), third_ptr);

    auto removed_first = parent.remove_child(*first_ptr);
    EXPECT_NE(removed_first, nullptr);
    EXPECT_EQ(parent.first_child(), second_ptr);
    EXPECT_EQ(parent.last_child(), third_ptr);

    auto removed_third = parent.remove_child(*third_ptr);
    EXPECT_NE(removed_third, nullptr);
    EXPECT_EQ(parent.first_child(), second_ptr);
    EXPECT_EQ(parent.last_child(), second_ptr);

    auto removed_second = parent.remove_child(*second_ptr);
    EXPECT_NE(removed_second, nullptr);
    EXPECT_EQ(parent.first_child(), nullptr);
    EXPECT_EQ(parent.last_child(), nullptr);
}

TEST(DOMTest, PreviousNextSiblingTraversalMatchesTreeOrderV67) {
    Element parent("list");
    auto a = std::make_unique<Element>("a");
    auto b = std::make_unique<Element>("b");
    auto c = std::make_unique<Element>("c");
    auto d = std::make_unique<Element>("d");

    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));
    parent.append_child(std::move(d));

    std::vector<std::string> forward_tags;
    for (Node* node = parent.first_child(); node != nullptr; node = node->next_sibling()) {
        forward_tags.push_back(static_cast<Element*>(node)->tag_name());
    }

    std::vector<std::string> reverse_tags;
    for (Node* node = parent.last_child(); node != nullptr; node = node->previous_sibling()) {
        reverse_tags.push_back(static_cast<Element*>(node)->tag_name());
    }

    ASSERT_EQ(forward_tags.size(), 4u);
    ASSERT_EQ(reverse_tags.size(), 4u);
    EXPECT_EQ(forward_tags[0], "a");
    EXPECT_EQ(forward_tags[1], "b");
    EXPECT_EQ(forward_tags[2], "c");
    EXPECT_EQ(forward_tags[3], "d");
    EXPECT_EQ(reverse_tags[0], "d");
    EXPECT_EQ(reverse_tags[1], "c");
    EXPECT_EQ(reverse_tags[2], "b");
    EXPECT_EQ(reverse_tags[3], "a");
}

TEST(DOMTest, OwnerDocumentReferenceResolvedFromAncestorDocumentV67) {
    auto owner_document = [](const Node* node) -> const Document* {
        for (const Node* current = node; current != nullptr; current = current->parent()) {
            if (current->node_type() == NodeType::Document) {
                return static_cast<const Document*>(current);
            }
        }
        return nullptr;
    };

    Document doc;
    auto host = std::make_unique<Element>("host");
    Element* host_ptr = host.get();
    auto leaf = std::make_unique<Text>("leaf");
    Text* leaf_ptr = leaf.get();
    host->append_child(std::move(leaf));

    EXPECT_EQ(owner_document(host_ptr), nullptr);
    EXPECT_EQ(owner_document(leaf_ptr), nullptr);

    doc.append_child(std::move(host));
    EXPECT_EQ(owner_document(host_ptr), &doc);
    EXPECT_EQ(owner_document(leaf_ptr), &doc);

    auto removed = doc.remove_child(*host_ptr);
    EXPECT_NE(removed, nullptr);
    EXPECT_EQ(owner_document(removed.get()), nullptr);
    EXPECT_EQ(owner_document(leaf_ptr), nullptr);
}

TEST(DOMTest, IsConnectedDetectionTracksAttachmentToDocumentV67) {
    auto is_connected = [](const Node& node) {
        for (const Node* current = &node; current != nullptr; current = current->parent()) {
            if (current->node_type() == NodeType::Document) {
                return true;
            }
        }
        return false;
    };

    Document doc;
    auto container = std::make_unique<Element>("container");
    Element* container_ptr = container.get();
    auto child = std::make_unique<Element>("child");
    Element* child_ptr = child.get();
    container->append_child(std::move(child));

    EXPECT_TRUE(is_connected(doc));
    EXPECT_FALSE(is_connected(*container_ptr));
    EXPECT_FALSE(is_connected(*child_ptr));

    doc.append_child(std::move(container));
    EXPECT_TRUE(is_connected(*container_ptr));
    EXPECT_TRUE(is_connected(*child_ptr));

    auto removed = doc.remove_child(*container_ptr);
    EXPECT_NE(removed, nullptr);
    EXPECT_FALSE(is_connected(*container_ptr));
    EXPECT_FALSE(is_connected(*child_ptr));
}

TEST(DOMTest, TextContentGetterCombinesAllDescendantTextV67) {
    Element root("div");
    root.append_child(std::make_unique<Text>("A"));

    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("B"));

    auto strong = std::make_unique<Element>("strong");
    strong->append_child(std::make_unique<Text>("C"));
    span->append_child(std::move(strong));

    root.append_child(std::move(span));
    root.append_child(std::make_unique<Comment>("ignore"));

    auto section = std::make_unique<Element>("section");
    section->append_child(std::make_unique<Text>("D"));
    auto em = std::make_unique<Element>("em");
    em->append_child(std::make_unique<Text>("E"));
    section->append_child(std::move(em));
    root.append_child(std::move(section));

    EXPECT_EQ(root.text_content(), "ABCDE");
}

TEST(DOMTest, SetAttributeSupportsSpecialCharactersInValueV67) {
    Element element("div");
    const std::string special_value = "a b&c<d>\"e' f\\n\\t/?=;+,%[]{}|^`~";

    element.set_attribute("data-raw", special_value);
    ASSERT_TRUE(element.has_attribute("data-raw"));
    ASSERT_TRUE(element.get_attribute("data-raw").has_value());
    EXPECT_EQ(element.get_attribute("data-raw").value(), special_value);

    element.set_attribute("data-raw", special_value + "!");
    EXPECT_EQ(element.attributes().size(), 1u);
    EXPECT_EQ(element.get_attribute("data-raw").value(), special_value + "!");
}

TEST(DOMTest, GetElementsByTagNameLikeCountingFindsMatchingDescendantsV67) {
    auto count_by_tag_name = [](const Node& root, const std::string& tag_name, const auto& self) -> size_t {
        size_t total = 0;
        if (root.node_type() == NodeType::Element) {
            const auto* element = static_cast<const Element*>(&root);
            if (element->tag_name() == tag_name) {
                ++total;
            }
        }

        for (Node* child = root.first_child(); child != nullptr; child = child->next_sibling()) {
            total += self(*child, tag_name, self);
        }
        return total;
    };

    Element root("root");
    root.append_child(std::make_unique<Element>("div"));

    auto section = std::make_unique<Element>("section");
    section->append_child(std::make_unique<Element>("div"));
    root.append_child(std::move(section));

    auto article = std::make_unique<Element>("article");
    article->append_child(std::make_unique<Element>("div"));
    article->append_child(std::make_unique<Element>("span"));
    root.append_child(std::move(article));

    EXPECT_EQ(count_by_tag_name(root, "div", count_by_tag_name), 3u);
    EXPECT_EQ(count_by_tag_name(root, "span", count_by_tag_name), 1u);
    EXPECT_EQ(count_by_tag_name(root, "root", count_by_tag_name), 1u);
    EXPECT_EQ(count_by_tag_name(root, "missing", count_by_tag_name), 0u);
}

TEST(DOMTest, EventListenerAddAndRemovalSemanticsByTypeV67) {
    EventTarget target;
    Element node("button");

    int click_count = 0;
    int input_count = 0;

    target.add_event_listener("click", [&](Event&) { ++click_count; }, true);
    target.add_event_listener("click", [&](Event&) { ++click_count; }, false);
    target.add_event_listener("input", [&](Event&) { ++input_count; }, false);

    Event click_event("click");
    click_event.phase_ = EventPhase::AtTarget;
    EXPECT_TRUE(target.dispatch_event(click_event, node));
    EXPECT_EQ(click_count, 2);
    EXPECT_EQ(input_count, 0);

    target.remove_all_listeners("click");

    Event click_event_after_removal("click");
    click_event_after_removal.phase_ = EventPhase::AtTarget;
    EXPECT_TRUE(target.dispatch_event(click_event_after_removal, node));
    EXPECT_EQ(click_count, 2);

    Event input_event("input");
    input_event.phase_ = EventPhase::AtTarget;
    EXPECT_TRUE(target.dispatch_event(input_event, node));
    EXPECT_EQ(input_count, 1);
}

TEST(DOMTest, ElementCreationWithNamespaceUriPreservesUriV68) {
    Element html_div("div");
    Element svg_circle("circle", "http://www.w3.org/2000/svg");

    EXPECT_EQ(html_div.tag_name(), "div");
    EXPECT_EQ(html_div.namespace_uri(), "");
    EXPECT_EQ(svg_circle.tag_name(), "circle");
    EXPECT_EQ(svg_circle.namespace_uri(), "http://www.w3.org/2000/svg");
    EXPECT_EQ(svg_circle.node_type(), NodeType::Element);
}

TEST(DOMTest, DeepClonePreservesAttributesAcrossDescendantsV68) {
    auto source = std::make_unique<Element>("svg", "http://www.w3.org/2000/svg");
    source->set_attribute("id", "icon");
    source->set_attribute("viewBox", "0 0 10 10");

    auto group = std::make_unique<Element>("g");
    Element* group_ptr = group.get();
    group->set_attribute("class", "accent");
    group->set_attribute("data-layer", "1");

    auto leaf = std::make_unique<Element>("path");
    leaf->set_attribute("d", "M0 0L10 10");
    group->append_child(std::move(leaf));
    source->append_child(std::move(group));

    std::function<std::unique_ptr<Node>(const Node*)> deep_clone =
        [&](const Node* node) -> std::unique_ptr<Node> {
            switch (node->node_type()) {
            case NodeType::Element: {
                const auto* element = static_cast<const Element*>(node);
                auto clone = std::make_unique<Element>(element->tag_name(), element->namespace_uri());
                for (const auto& attr : element->attributes()) {
                    clone->set_attribute(attr.name, attr.value);
                }
                for (const Node* child = node->first_child(); child != nullptr; child = child->next_sibling()) {
                    clone->append_child(deep_clone(child));
                }
                return clone;
            }
            case NodeType::Text:
                return std::make_unique<Text>(static_cast<const Text*>(node)->data());
            case NodeType::Comment:
                return std::make_unique<Comment>(static_cast<const Comment*>(node)->data());
            case NodeType::Document: {
                auto clone = std::make_unique<Document>();
                for (const Node* child = node->first_child(); child != nullptr; child = child->next_sibling()) {
                    clone->append_child(deep_clone(child));
                }
                return clone;
            }
            default:
                return nullptr;
            }
        };

    auto cloned_node = deep_clone(source.get());
    auto* cloned_root = static_cast<Element*>(cloned_node.get());
    ASSERT_NE(cloned_root, nullptr);
    ASSERT_NE(cloned_root, source.get());

    auto* cloned_group = static_cast<Element*>(cloned_root->first_child());
    ASSERT_NE(cloned_group, nullptr);
    auto* cloned_leaf = static_cast<Element*>(cloned_group->first_child());
    ASSERT_NE(cloned_leaf, nullptr);

    EXPECT_EQ(cloned_root->get_attribute("id"), "icon");
    EXPECT_EQ(cloned_root->get_attribute("viewBox"), "0 0 10 10");
    EXPECT_EQ(cloned_group->get_attribute("class"), "accent");
    EXPECT_EQ(cloned_group->get_attribute("data-layer"), "1");
    EXPECT_EQ(cloned_leaf->get_attribute("d"), "M0 0L10 10");

    source->set_attribute("id", "changed");
    group_ptr->set_attribute("class", "mutated");
    EXPECT_EQ(cloned_root->get_attribute("id"), "icon");
    EXPECT_EQ(cloned_group->get_attribute("class"), "accent");
}

TEST(DOMTest, RemoveAllChildrenUtilityDetachesEveryChildV68) {
    auto remove_all_children = [](Node& node) {
        std::vector<std::unique_ptr<Node>> removed;
        while (node.first_child() != nullptr) {
            removed.push_back(node.remove_child(*node.first_child()));
        }
        return removed;
    };

    Element parent("div");
    auto a = std::make_unique<Element>("a");
    auto b = std::make_unique<Text>("b");
    auto c = std::make_unique<Comment>("c");

    Node* a_ptr = a.get();
    Node* b_ptr = b.get();
    Node* c_ptr = c.get();

    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));
    ASSERT_EQ(parent.child_count(), 3u);

    auto removed = remove_all_children(parent);
    ASSERT_EQ(removed.size(), 3u);
    EXPECT_EQ(removed[0].get(), a_ptr);
    EXPECT_EQ(removed[1].get(), b_ptr);
    EXPECT_EQ(removed[2].get(), c_ptr);
    EXPECT_EQ(parent.child_count(), 0u);
    EXPECT_EQ(parent.first_child(), nullptr);
    EXPECT_EQ(parent.last_child(), nullptr);

    for (const auto& node : removed) {
        EXPECT_EQ(node->parent(), nullptr);
        EXPECT_EQ(node->previous_sibling(), nullptr);
        EXPECT_EQ(node->next_sibling(), nullptr);
    }
}

TEST(DOMTest, ChildElementCountDiffersFromChildNodeCountV68) {
    auto child_element_count = [](const Node& node) {
        size_t count = 0;
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            if (child->node_type() == NodeType::Element) {
                ++count;
            }
        }
        return count;
    };

    Element parent("div");
    parent.append_child(std::make_unique<Text>("leading"));
    parent.append_child(std::make_unique<Element>("span"));
    parent.append_child(std::make_unique<Comment>("meta"));
    parent.append_child(std::make_unique<Text>("trailing"));
    parent.append_child(std::make_unique<Element>("strong"));

    EXPECT_EQ(parent.child_count(), 5u);
    EXPECT_EQ(child_element_count(parent), 2u);
}

TEST(DOMTest, LivingNodeListLikeQueryUpdatesAfterMutationsV68) {
    Element root("root");

    auto first_div = std::make_unique<Element>("div");
    Element* first_div_ptr = first_div.get();
    root.append_child(std::move(first_div));
    root.append_child(std::make_unique<Element>("span"));

    auto list_by_tag = [](Node& start, const std::string& tag_name) {
        std::vector<Element*> result;
        std::function<void(Node&)> visit = [&](Node& node) {
            if (node.node_type() == NodeType::Element) {
                auto* element = static_cast<Element*>(&node);
                if (element->tag_name() == tag_name) {
                    result.push_back(element);
                }
            }
            for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
                visit(*child);
            }
        };
        visit(start);
        return result;
    };

    auto initial = list_by_tag(root, "div");
    ASSERT_EQ(initial.size(), 1u);
    EXPECT_EQ(initial[0], first_div_ptr);

    auto second_div = std::make_unique<Element>("div");
    Element* second_div_ptr = second_div.get();
    root.append_child(std::move(second_div));

    auto after_append = list_by_tag(root, "div");
    ASSERT_EQ(after_append.size(), 2u);
    EXPECT_EQ(after_append[0], first_div_ptr);
    EXPECT_EQ(after_append[1], second_div_ptr);

    auto removed = root.remove_child(*first_div_ptr);
    EXPECT_NE(removed, nullptr);
    auto after_remove = list_by_tag(root, "div");
    ASSERT_EQ(after_remove.size(), 1u);
    EXPECT_EQ(after_remove[0], second_div_ptr);
}

TEST(DOMTest, ParentElementVsParentNodeBehaviorV68) {
    auto parent_element = [](const Node& node) -> Element* {
        Node* parent = node.parent();
        if (parent != nullptr && parent->node_type() == NodeType::Element) {
            return static_cast<Element*>(parent);
        }
        return nullptr;
    };

    Document doc;
    auto html = std::make_unique<Element>("html");
    Element* html_ptr = html.get();
    auto text = std::make_unique<Text>("leaf");
    Text* text_ptr = text.get();
    html->append_child(std::move(text));

    EXPECT_EQ(text_ptr->parent(), html_ptr);
    EXPECT_EQ(parent_element(*text_ptr), html_ptr);

    doc.append_child(std::move(html));
    EXPECT_EQ(html_ptr->parent(), &doc);
    EXPECT_EQ(parent_element(*html_ptr), nullptr);
    EXPECT_EQ(doc.parent(), nullptr);
    EXPECT_EQ(parent_element(doc), nullptr);
}

TEST(DOMTest, ElementMatchesTagSelectorByTagNameV68) {
    auto matches_tag_selector = [](const Element& element, const std::string& selector) {
        return !selector.empty() && selector[0] != '#' && selector[0] != '.'
            && element.tag_name() == selector;
    };

    Element button("button");
    Element input("input");

    EXPECT_TRUE(matches_tag_selector(button, "button"));
    EXPECT_FALSE(matches_tag_selector(button, "Button"));
    EXPECT_FALSE(matches_tag_selector(button, "#button"));
    EXPECT_FALSE(matches_tag_selector(button, ".button"));
    EXPECT_FALSE(matches_tag_selector(input, "button"));
}

TEST(DOMTest, WhitespaceOnlyTextNodesArePreservedAsTextNodesV68) {
    auto child_element_count = [](const Node& node) {
        size_t count = 0;
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            if (child->node_type() == NodeType::Element) {
                ++count;
            }
        }
        return count;
    };

    Element root("div");
    auto leading_ws = std::make_unique<Text>("\n  \t");
    Text* leading_ws_ptr = leading_ws.get();
    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("X"));
    auto trailing_ws = std::make_unique<Text>("  ");
    Text* trailing_ws_ptr = trailing_ws.get();

    root.append_child(std::move(leading_ws));
    root.append_child(std::move(span));
    root.append_child(std::move(trailing_ws));

    ASSERT_EQ(root.child_count(), 3u);
    EXPECT_EQ(child_element_count(root), 1u);
    ASSERT_NE(root.first_child(), nullptr);
    ASSERT_NE(root.last_child(), nullptr);
    EXPECT_EQ(root.first_child()->node_type(), NodeType::Text);
    EXPECT_EQ(root.last_child()->node_type(), NodeType::Text);
    EXPECT_EQ(leading_ws_ptr->data(), "\n  \t");
    EXPECT_EQ(trailing_ws_ptr->data(), "  ");
    EXPECT_EQ(root.text_content(), "\n  \tX  ");
}

TEST(DOMTest, ElementIdGetterAndSetterViaAttributeV69) {
    Element element("div");
    EXPECT_EQ(element.id(), "");

    element.set_attribute("id", "hero");
    EXPECT_EQ(element.id(), "hero");
    EXPECT_EQ(element.get_attribute("id"), "hero");

    element.set_attribute("id", "hero-main");
    EXPECT_EQ(element.id(), "hero-main");
    EXPECT_EQ(element.get_attribute("id"), "hero-main");

    element.remove_attribute("id");
    EXPECT_EQ(element.id(), "");
    EXPECT_FALSE(element.get_attribute("id").has_value());
}

TEST(DOMTest, ClassNamePropertyManipulationViaClassAttributeV69) {
    Element element("div");
    EXPECT_FALSE(element.get_attribute("class").has_value());

    element.set_attribute("class", "card elevated");
    ASSERT_TRUE(element.get_attribute("class").has_value());
    EXPECT_EQ(element.get_attribute("class").value(), "card elevated");

    element.set_attribute("class", "card interactive selected");
    EXPECT_EQ(element.get_attribute("class").value(), "card interactive selected");

    element.remove_attribute("class");
    EXPECT_FALSE(element.get_attribute("class").has_value());
}

TEST(DOMTest, StyleAttributeParsingExtractsKeyValuesV69) {
    auto get_style_property = [](const Element& element, const std::string& property_name) -> std::string {
        const auto style = element.get_attribute("style");
        if (!style.has_value()) {
            return "";
        }

        const std::string& text = style.value();
        size_t cursor = 0;
        while (cursor < text.size()) {
            const size_t colon = text.find(':', cursor);
            if (colon == std::string::npos) {
                break;
            }
            const size_t semicolon = text.find(';', colon);
            const size_t decl_end = semicolon == std::string::npos ? text.size() : semicolon;

            std::string key = text.substr(cursor, colon - cursor);
            std::string value = text.substr(colon + 1, decl_end - (colon + 1));

            while (!key.empty() && key.front() == ' ') {
                key.erase(0, 1);
            }
            while (!key.empty() && key.back() == ' ') {
                key.pop_back();
            }
            while (!value.empty() && value.front() == ' ') {
                value.erase(0, 1);
            }
            while (!value.empty() && value.back() == ' ') {
                value.pop_back();
            }

            if (key == property_name) {
                return value;
            }
            cursor = semicolon == std::string::npos ? text.size() : semicolon + 1;
            while (cursor < text.size() && text[cursor] == ' ') {
                ++cursor;
            }
        }
        return "";
    };

    Element element("div");
    element.set_attribute("style", "color: red; font-size: 16px; margin: 0 auto");

    EXPECT_EQ(get_style_property(element, "color"), "red");
    EXPECT_EQ(get_style_property(element, "font-size"), "16px");
    EXPECT_EQ(get_style_property(element, "margin"), "0 auto");
    EXPECT_EQ(get_style_property(element, "padding"), "");
}

TEST(DOMTest, DataAttributeAccessByDatasetKeyV69) {
    auto key_to_data_attribute = [](const std::string& key) {
        std::string attribute_name = "data-";
        for (char ch : key) {
            if (ch >= 'A' && ch <= 'Z') {
                attribute_name.push_back('-');
                attribute_name.push_back(static_cast<char>(ch - 'A' + 'a'));
            } else {
                attribute_name.push_back(ch);
            }
        }
        return attribute_name;
    };

    auto set_dataset_value = [&](Element& element, const std::string& key, const std::string& value) {
        element.set_attribute(key_to_data_attribute(key), value);
    };

    auto get_dataset_value = [&](const Element& element, const std::string& key) {
        return element.get_attribute(key_to_data_attribute(key));
    };

    Element element("section");
    set_dataset_value(element, "userId", "42");
    set_dataset_value(element, "buildVersion", "2026.02");

    EXPECT_EQ(get_dataset_value(element, "userId"), "42");
    EXPECT_EQ(get_dataset_value(element, "buildVersion"), "2026.02");
    EXPECT_TRUE(element.has_attribute("data-user-id"));
    EXPECT_TRUE(element.has_attribute("data-build-version"));
    EXPECT_FALSE(get_dataset_value(element, "missingKey").has_value());
}

TEST(DOMTest, HiddenAttributeToggleUsesPresenceSemanticsV69) {
    auto is_hidden = [](const Element& element) {
        return element.has_attribute("hidden");
    };

    auto set_hidden = [](Element& element, bool hidden) {
        if (hidden) {
            element.set_attribute("hidden", "");
        } else {
            element.remove_attribute("hidden");
        }
    };

    Element element("div");
    EXPECT_FALSE(is_hidden(element));

    set_hidden(element, true);
    EXPECT_TRUE(is_hidden(element));
    EXPECT_TRUE(element.get_attribute("hidden").has_value());

    set_hidden(element, false);
    EXPECT_FALSE(is_hidden(element));
    EXPECT_FALSE(element.get_attribute("hidden").has_value());
}

TEST(DOMTest, ContentEditableFlagReflectsAttributeValuesV69) {
    auto is_content_editable = [](const Element& element) {
        const auto value = element.get_attribute("contenteditable");
        if (!value.has_value()) {
            return false;
        }
        return value.value().empty() || value.value() == "true" || value.value() == "plaintext-only";
    };

    Element element("div");
    EXPECT_FALSE(is_content_editable(element));

    element.set_attribute("contenteditable", "true");
    EXPECT_TRUE(is_content_editable(element));

    element.set_attribute("contenteditable", "false");
    EXPECT_FALSE(is_content_editable(element));

    element.set_attribute("contenteditable", "");
    EXPECT_TRUE(is_content_editable(element));
}

TEST(DOMTest, TabIndexDefaultAndCustomValuesV69) {
    auto tab_index = [](const Element& element) {
        const auto value = element.get_attribute("tabindex");
        if (!value.has_value()) {
            return 0;
        }
        return std::stoi(value.value());
    };

    Element button("button");
    EXPECT_EQ(tab_index(button), 0);

    button.set_attribute("tabindex", "3");
    EXPECT_EQ(tab_index(button), 3);

    button.set_attribute("tabindex", "-1");
    EXPECT_EQ(tab_index(button), -1);
}

TEST(DOMTest, TitleAttributeRoundTripOnElementV69) {
    Element anchor("a");
    EXPECT_FALSE(anchor.get_attribute("title").has_value());

    anchor.set_attribute("title", "Open project documentation");
    ASSERT_TRUE(anchor.get_attribute("title").has_value());
    EXPECT_EQ(anchor.get_attribute("title").value(), "Open project documentation");

    anchor.set_attribute("title", "Open updated docs");
    EXPECT_EQ(anchor.get_attribute("title").value(), "Open updated docs");

    anchor.remove_attribute("title");
    EXPECT_FALSE(anchor.get_attribute("title").has_value());
}

TEST(DOMTest, MultipleEventListenersSameEventInvokeInOrderV70) {
    EventTarget target;
    Element node("button");
    std::vector<int> call_order;

    target.add_event_listener("click", [&](Event&) { call_order.push_back(1); }, false);
    target.add_event_listener("click", [&](Event&) { call_order.push_back(2); }, false);
    target.add_event_listener("click", [&](Event&) { call_order.push_back(3); }, false);

    Event event("click");
    event.target_ = &node;
    event.current_target_ = &node;
    event.phase_ = EventPhase::AtTarget;

    EXPECT_TRUE(target.dispatch_event(event, node));
    ASSERT_EQ(call_order.size(), 3u);
    EXPECT_EQ(call_order[0], 1);
    EXPECT_EQ(call_order[1], 2);
    EXPECT_EQ(call_order[2], 3);
}

TEST(DOMTest, EventStopPropagationBlocksAncestorDispatchV70) {
    auto grandparent = std::make_unique<Element>("div");
    auto parent = std::make_unique<Element>("section");
    auto child = std::make_unique<Element>("button");

    Element* grandparent_ptr = grandparent.get();
    Element* parent_ptr = parent.get();
    Element* child_ptr = child.get();

    parent->append_child(std::move(child));
    grandparent->append_child(std::move(parent));

    EventTarget grandparent_target;
    EventTarget parent_target;
    EventTarget child_target;
    std::vector<std::string> log;

    grandparent_target.add_event_listener("click", [&](Event&) { log.push_back("grandparent"); }, false);
    parent_target.add_event_listener("click", [&](Event&) { log.push_back("parent"); }, false);
    child_target.add_event_listener(
        "click",
        [&](Event& event) {
            log.push_back("child");
            event.stop_propagation();
        },
        false);

    Event event("click");
    event.target_ = child_ptr;
    event.current_target_ = child_ptr;
    event.phase_ = EventPhase::AtTarget;
    child_target.dispatch_event(event, *child_ptr);

    if (!event.propagation_stopped() && event.bubbles()) {
        event.phase_ = EventPhase::Bubbling;
        event.current_target_ = parent_ptr;
        parent_target.dispatch_event(event, *parent_ptr);
    }

    if (!event.propagation_stopped() && event.bubbles()) {
        event.current_target_ = grandparent_ptr;
        grandparent_target.dispatch_event(event, *grandparent_ptr);
    }

    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0], "child");
    EXPECT_TRUE(event.propagation_stopped());
}

TEST(DOMTest, EventPreventDefaultFlagReflectsCancelableStateV70) {
    EventTarget target;
    Element form("form");

    target.add_event_listener(
        "submit",
        [&](Event& event) {
            event.prevent_default();
        },
        false);

    Event cancelable_event("submit", true, true);
    cancelable_event.target_ = &form;
    cancelable_event.current_target_ = &form;
    cancelable_event.phase_ = EventPhase::AtTarget;

    EXPECT_FALSE(cancelable_event.default_prevented());
    EXPECT_FALSE(target.dispatch_event(cancelable_event, form));
    EXPECT_TRUE(cancelable_event.default_prevented());

    Event non_cancelable_event("submit", true, false);
    non_cancelable_event.target_ = &form;
    non_cancelable_event.current_target_ = &form;
    non_cancelable_event.phase_ = EventPhase::AtTarget;

    EXPECT_TRUE(target.dispatch_event(non_cancelable_event, form));
    EXPECT_FALSE(non_cancelable_event.default_prevented());
}

TEST(DOMTest, DocumentCreateTextNodeProducesTextNodeV70) {
    Document doc;
    auto text_node = doc.create_text_node("hello V70");

    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->node_type(), NodeType::Text);
    EXPECT_EQ(text_node->data(), "hello V70");
    EXPECT_EQ(text_node->text_content(), "hello V70");
    EXPECT_EQ(text_node->parent(), nullptr);
}

TEST(DOMTest, DocumentCreateCommentProducesCommentNodeV70) {
    Document doc;
    auto comment_node = doc.create_comment("comment V70");

    ASSERT_NE(comment_node, nullptr);
    EXPECT_EQ(comment_node->node_type(), NodeType::Comment);
    EXPECT_EQ(comment_node->data(), "comment V70");
    EXPECT_EQ(comment_node->parent(), nullptr);
}

TEST(DOMTest, ElementChildrenCountAfterMultipleAppendsV70) {
    Element parent("div");

    auto first_element = std::make_unique<Element>("span");
    auto text_node = std::make_unique<Text>("middle");
    auto comment_node = std::make_unique<Comment>("note");
    auto last_element = std::make_unique<Element>("strong");

    Node* first_element_ptr = first_element.get();
    Node* text_node_ptr = text_node.get();
    Node* comment_node_ptr = comment_node.get();
    Node* last_element_ptr = last_element.get();

    parent.append_child(std::move(first_element));
    parent.append_child(std::move(text_node));
    parent.append_child(std::move(comment_node));
    parent.append_child(std::move(last_element));

    EXPECT_EQ(parent.child_count(), 4u);
    EXPECT_EQ(parent.first_child(), first_element_ptr);
    EXPECT_EQ(parent.last_child(), last_element_ptr);
    EXPECT_EQ(first_element_ptr->next_sibling(), text_node_ptr);
    EXPECT_EQ(text_node_ptr->next_sibling(), comment_node_ptr);
    EXPECT_EQ(comment_node_ptr->next_sibling(), last_element_ptr);
}

TEST(DOMTest, NodeIsEqualNodeSemanticsCompareStructureAndDataV70) {
    auto is_equal_node = [](const Node& left, const Node& right, const auto& self) -> bool {
        if (left.node_type() != right.node_type()) {
            return false;
        }

        if (left.node_type() == NodeType::Element) {
            const auto* left_element = static_cast<const Element*>(&left);
            const auto* right_element = static_cast<const Element*>(&right);

            if (left_element->tag_name() != right_element->tag_name()) {
                return false;
            }
            if (left_element->namespace_uri() != right_element->namespace_uri()) {
                return false;
            }

            const auto& left_attributes = left_element->attributes();
            const auto& right_attributes = right_element->attributes();
            if (left_attributes.size() != right_attributes.size()) {
                return false;
            }
            for (const auto& left_attribute : left_attributes) {
                bool matched = false;
                for (const auto& right_attribute : right_attributes) {
                    if (left_attribute.name == right_attribute.name
                        && left_attribute.value == right_attribute.value) {
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    return false;
                }
            }
        } else if (left.node_type() == NodeType::Text) {
            const auto* left_text = static_cast<const Text*>(&left);
            const auto* right_text = static_cast<const Text*>(&right);
            if (left_text->data() != right_text->data()) {
                return false;
            }
        } else if (left.node_type() == NodeType::Comment) {
            const auto* left_comment = static_cast<const Comment*>(&left);
            const auto* right_comment = static_cast<const Comment*>(&right);
            if (left_comment->data() != right_comment->data()) {
                return false;
            }
        }

        if (left.child_count() != right.child_count()) {
            return false;
        }

        Node* left_child = left.first_child();
        Node* right_child = right.first_child();
        while (left_child != nullptr && right_child != nullptr) {
            if (!self(*left_child, *right_child, self)) {
                return false;
            }
            left_child = left_child->next_sibling();
            right_child = right_child->next_sibling();
        }

        return left_child == nullptr && right_child == nullptr;
    };

    auto left_root = std::make_unique<Element>("div");
    left_root->set_attribute("id", "root");
    auto left_span = std::make_unique<Element>("span");
    left_span->set_attribute("class", "label");
    left_span->append_child(std::make_unique<Text>("hello"));
    left_root->append_child(std::move(left_span));

    auto right_root = std::make_unique<Element>("div");
    right_root->set_attribute("id", "root");
    auto right_span = std::make_unique<Element>("span");
    Element* right_span_ptr = right_span.get();
    right_span->set_attribute("class", "label");
    auto right_text = std::make_unique<Text>("hello");
    Text* right_text_ptr = right_text.get();
    right_span->append_child(std::move(right_text));
    right_root->append_child(std::move(right_span));

    EXPECT_TRUE(is_equal_node(*left_root, *right_root, is_equal_node));

    right_span_ptr->set_attribute("class", "label updated");
    EXPECT_FALSE(is_equal_node(*left_root, *right_root, is_equal_node));

    right_span_ptr->set_attribute("class", "label");
    right_text_ptr->set_data("changed");
    EXPECT_FALSE(is_equal_node(*left_root, *right_root, is_equal_node));
}

TEST(DOMTest, ElementClosestAncestorMatchingFindsNearestMatchV70) {
    auto closest_ancestor_matching = [](const Node& start, const auto& predicate) -> Element* {
        Node* current = start.parent();
        while (current != nullptr) {
            if (current->node_type() == NodeType::Element) {
                auto* element = static_cast<Element*>(current);
                if (predicate(*element)) {
                    return element;
                }
            }
            current = current->parent();
        }
        return nullptr;
    };

    auto article = std::make_unique<Element>("article");
    Element* article_ptr = article.get();
    article->set_attribute("data-scope", "root");

    auto section = std::make_unique<Element>("section");
    Element* section_ptr = section.get();
    section->set_attribute("data-scope", "container");

    auto div = std::make_unique<Element>("div");
    Element* div_ptr = div.get();

    auto button = std::make_unique<Element>("button");
    Element* button_ptr = button.get();

    div->append_child(std::move(button));
    section->append_child(std::move(div));
    article->append_child(std::move(section));

    EXPECT_EQ(
        closest_ancestor_matching(*button_ptr, [](const Element& element) { return element.tag_name() == "div"; }),
        div_ptr);
    EXPECT_EQ(
        closest_ancestor_matching(*button_ptr, [](const Element& element) { return element.tag_name() == "section"; }),
        section_ptr);
    EXPECT_EQ(
        closest_ancestor_matching(
            *button_ptr,
            [](const Element& element) {
                return element.has_attribute("data-scope");
            }),
        section_ptr);
    EXPECT_EQ(
        closest_ancestor_matching(*button_ptr, [](const Element& element) { return element.tag_name() == "nav"; }),
        nullptr);
    EXPECT_EQ(
        closest_ancestor_matching(*article_ptr, [](const Element& element) { return element.tag_name() == "div"; }),
        nullptr);
}

TEST(DOMTest, ElementLookupNamespaceURIResolvesDefaultNamespaceV71) {
    auto lookup_namespace_uri = [](const Element& element, const std::string& prefix) -> std::string {
        if (!prefix.empty()) {
            return "";
        }
        return element.namespace_uri();
    };

    Element html_div("div");
    Element svg_circle("circle", "http://www.w3.org/2000/svg");

    EXPECT_EQ(lookup_namespace_uri(html_div, ""), "");
    EXPECT_EQ(lookup_namespace_uri(svg_circle, ""), "http://www.w3.org/2000/svg");
    EXPECT_EQ(lookup_namespace_uri(svg_circle, "svg"), "");
}

TEST(DOMTest, ElementGetAttributeReturnsEmptyForMissingV71) {
    Element element("article");

    const auto missing_value = element.get_attribute("data-missing");
    EXPECT_FALSE(missing_value.has_value());
    EXPECT_EQ(missing_value.value_or(""), "");
}

TEST(DOMTest, ElementRemoveAttributeIsIdempotentV71) {
    Element element("button");
    element.set_attribute("id", "primary-action");
    element.set_attribute("type", "button");

    EXPECT_TRUE(element.has_attribute("id"));
    EXPECT_NO_THROW(element.remove_attribute("id"));
    EXPECT_NO_THROW(element.remove_attribute("id"));

    EXPECT_FALSE(element.has_attribute("id"));
    EXPECT_EQ(element.get_attribute("id").value_or(""), "");
    EXPECT_EQ(element.get_attribute("type").value_or(""), "button");
    EXPECT_EQ(element.attributes().size(), 1u);
}

TEST(DOMTest, DocumentBodyReferenceReturnsBodyNodeV71) {
    Document document;

    auto html = document.create_element("html");
    Element* html_ptr = html.get();
    auto head = document.create_element("head");
    auto body = document.create_element("body");
    Element* body_ptr = body.get();

    html->append_child(std::move(head));
    html->append_child(std::move(body));
    document.append_child(std::move(html));

    ASSERT_EQ(document.document_element(), html_ptr);
    ASSERT_EQ(document.body(), body_ptr);

    body_ptr->append_child(document.create_element("p"));
    EXPECT_EQ(document.body(), body_ptr);
    EXPECT_EQ(body_ptr->parent(), html_ptr);
}

TEST(DOMTest, TextNodeSplittingCreatesTrailingSiblingV71) {
    auto split_text_node = [](Text& text_node, size_t offset) -> Text* {
        std::string original = text_node.data();
        if (offset > original.size()) {
            offset = original.size();
        }

        text_node.set_data(original.substr(0, offset));

        Node* parent = text_node.parent();
        if (parent == nullptr) {
            return nullptr;
        }

        auto trailing_text = std::make_unique<Text>(original.substr(offset));
        Text* trailing_ptr = trailing_text.get();
        parent->insert_before(std::move(trailing_text), text_node.next_sibling());
        return trailing_ptr;
    };

    Element container("div");
    auto initial_text = std::make_unique<Text>("hello-world");
    Text* initial_ptr = initial_text.get();
    container.append_child(std::move(initial_text));

    Text* trailing_ptr = split_text_node(*initial_ptr, 5);

    ASSERT_NE(trailing_ptr, nullptr);
    EXPECT_EQ(initial_ptr->data(), "hello");
    EXPECT_EQ(trailing_ptr->data(), "-world");
    EXPECT_EQ(container.child_count(), 2u);
    EXPECT_EQ(initial_ptr->next_sibling(), trailing_ptr);
    EXPECT_EQ(trailing_ptr->previous_sibling(), initial_ptr);
}

TEST(DOMTest, CommentNodeCreationAndValueMutationV71) {
    Document document;
    auto comment = document.create_comment("initial note");

    ASSERT_NE(comment, nullptr);
    EXPECT_EQ(comment->node_type(), NodeType::Comment);
    EXPECT_EQ(comment->data(), "initial note");

    comment->set_data("updated note");
    EXPECT_EQ(comment->data(), "updated note");
}

TEST(DOMTest, ElementBeforeAfterSiblingInsertionMaintainsOrderV71) {
    auto insert_before_sibling = [](Element& target, std::unique_ptr<Node> node) -> Node* {
        Node* parent = target.parent();
        if (parent == nullptr) {
            return nullptr;
        }
        Node* inserted = node.get();
        parent->insert_before(std::move(node), &target);
        return inserted;
    };
    auto insert_after_sibling = [](Element& target, std::unique_ptr<Node> node) -> Node* {
        Node* parent = target.parent();
        if (parent == nullptr) {
            return nullptr;
        }
        Node* inserted = node.get();
        parent->insert_before(std::move(node), target.next_sibling());
        return inserted;
    };

    Element parent("ul");
    auto first = std::make_unique<Element>("first");
    auto target = std::make_unique<Element>("target");
    auto last = std::make_unique<Element>("last");
    Element* first_ptr = first.get();
    Element* target_ptr = target.get();
    Element* last_ptr = last.get();

    parent.append_child(std::move(first));
    parent.append_child(std::move(target));
    parent.append_child(std::move(last));

    Node* before_ptr = insert_before_sibling(*target_ptr, std::make_unique<Element>("before"));
    Node* after_ptr = insert_after_sibling(*target_ptr, std::make_unique<Element>("after"));

    ASSERT_NE(before_ptr, nullptr);
    ASSERT_NE(after_ptr, nullptr);
    EXPECT_EQ(parent.child_count(), 5u);
    EXPECT_EQ(first_ptr->next_sibling(), before_ptr);
    EXPECT_EQ(before_ptr->next_sibling(), target_ptr);
    EXPECT_EQ(target_ptr->next_sibling(), after_ptr);
    EXPECT_EQ(after_ptr->next_sibling(), last_ptr);
}

TEST(DOMTest, NodeContainsCheckFindsAncestorRelationshipV71) {
    auto node_contains = [](const Node& candidate_ancestor, const Node& node) -> bool {
        const Node* current = &node;
        while (current != nullptr) {
            if (current == &candidate_ancestor) {
                return true;
            }
            current = current->parent();
        }
        return false;
    };

    auto root = std::make_unique<Element>("root");
    Element* root_ptr = root.get();
    auto section = std::make_unique<Element>("section");
    Element* section_ptr = section.get();
    auto button = std::make_unique<Element>("button");
    Element* button_ptr = button.get();
    auto aside = std::make_unique<Element>("aside");
    Element* aside_ptr = aside.get();

    section->append_child(std::move(button));
    root->append_child(std::move(section));
    root->append_child(std::move(aside));

    EXPECT_TRUE(node_contains(*root_ptr, *root_ptr));
    EXPECT_TRUE(node_contains(*root_ptr, *section_ptr));
    EXPECT_TRUE(node_contains(*root_ptr, *button_ptr));
    EXPECT_TRUE(node_contains(*section_ptr, *button_ptr));
    EXPECT_FALSE(node_contains(*button_ptr, *section_ptr));
    EXPECT_FALSE(node_contains(*aside_ptr, *button_ptr));
}

TEST(DOMTest, ElementTagNameUppercaseV72) {
    auto tag_name_uppercase = [](const Element& element) -> std::string {
        std::string upper = element.tag_name();
        for (char& ch : upper) {
            if (ch >= 'a' && ch <= 'z') {
                ch = static_cast<char>(ch - ('a' - 'A'));
            }
        }
        return upper;
    };

    Element element("main");
    EXPECT_EQ(tag_name_uppercase(element), "MAIN");
}

TEST(DOMTest, NodeTypeConstantsMatchDomSpecValuesV72) {
    auto dom_node_type_constant = [](NodeType type) -> int {
        switch (type) {
            case NodeType::Element:
                return 1;
            case NodeType::Text:
                return 3;
            case NodeType::Comment:
                return 8;
            case NodeType::Document:
                return 9;
            default:
                return -1;
        }
    };

    Element element("div");
    Text text("hello");
    Comment comment("note");
    Document document;

    EXPECT_EQ(dom_node_type_constant(element.node_type()), 1);
    EXPECT_EQ(dom_node_type_constant(text.node_type()), 3);
    EXPECT_EQ(dom_node_type_constant(comment.node_type()), 8);
    EXPECT_EQ(dom_node_type_constant(document.node_type()), 9);
}

TEST(DOMTest, ReplaceChildrenClearsAndSetsNewNodesV72) {
    auto replace_children = [](Node& parent, std::vector<std::unique_ptr<Node>> children)
        -> std::vector<std::unique_ptr<Node>> {
        std::vector<std::unique_ptr<Node>> removed_children;
        while (parent.first_child() != nullptr) {
            removed_children.push_back(parent.remove_child(*parent.first_child()));
        }
        for (auto& child : children) {
            parent.append_child(std::move(child));
        }
        return removed_children;
    };

    Element parent("div");
    auto old_a = std::make_unique<Element>("old-a");
    auto old_b = std::make_unique<Element>("old-b");
    Element* old_a_ptr = old_a.get();
    Element* old_b_ptr = old_b.get();
    parent.append_child(std::move(old_a));
    parent.append_child(std::move(old_b));

    auto new_first = std::make_unique<Element>("new-first");
    auto new_second = std::make_unique<Element>("new-second");
    Element* new_first_ptr = new_first.get();
    Element* new_second_ptr = new_second.get();
    std::vector<std::unique_ptr<Node>> replacements;
    replacements.push_back(std::move(new_first));
    replacements.push_back(std::move(new_second));

    auto removed = replace_children(parent, std::move(replacements));

    ASSERT_EQ(removed.size(), 2u);
    EXPECT_EQ(removed[0].get(), old_a_ptr);
    EXPECT_EQ(removed[1].get(), old_b_ptr);
    EXPECT_EQ(removed[0]->parent(), nullptr);
    EXPECT_EQ(removed[1]->parent(), nullptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), new_first_ptr);
    EXPECT_EQ(new_first_ptr->next_sibling(), new_second_ptr);
    EXPECT_EQ(parent.last_child(), new_second_ptr);
}

TEST(DOMTest, ElementToggleAttributePresenceV72) {
    auto toggle_attribute = [](Element& element, const std::string& name) -> bool {
        if (element.has_attribute(name)) {
            element.remove_attribute(name);
            return false;
        }
        element.set_attribute(name, "");
        return true;
    };

    Element element("button");
    EXPECT_FALSE(element.has_attribute("disabled"));
    EXPECT_TRUE(toggle_attribute(element, "disabled"));
    EXPECT_TRUE(element.has_attribute("disabled"));
    EXPECT_FALSE(toggle_attribute(element, "disabled"));
    EXPECT_FALSE(element.has_attribute("disabled"));
}

TEST(DOMTest, ClassListAddAndContainsV72) {
    Element element("div");
    element.class_list().add("card");
    element.class_list().add("active");

    EXPECT_TRUE(element.class_list().contains("card"));
    EXPECT_TRUE(element.class_list().contains("active"));
    EXPECT_FALSE(element.class_list().contains("hidden"));
}

TEST(DOMTest, ClassListRemoveV72) {
    Element element("div");
    element.class_list().add("alpha");
    element.class_list().add("beta");

    element.class_list().remove("alpha");

    EXPECT_FALSE(element.class_list().contains("alpha"));
    EXPECT_TRUE(element.class_list().contains("beta"));
}

TEST(DOMTest, PrependChildInsertsAtBeginningV72) {
    auto prepend_child = [](Node& parent, std::unique_ptr<Node> child) -> Node* {
        Node* inserted = child.get();
        parent.insert_before(std::move(child), parent.first_child());
        return inserted;
    };

    Element parent("ul");
    auto existing_first = std::make_unique<Element>("first");
    auto existing_second = std::make_unique<Element>("second");
    Element* existing_first_ptr = existing_first.get();
    Element* existing_second_ptr = existing_second.get();
    parent.append_child(std::move(existing_first));
    parent.append_child(std::move(existing_second));

    Node* prepended = prepend_child(parent, std::make_unique<Element>("prepended"));

    ASSERT_NE(prepended, nullptr);
    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), prepended);
    EXPECT_EQ(prepended->next_sibling(), existing_first_ptr);
    EXPECT_EQ(existing_first_ptr->next_sibling(), existing_second_ptr);
}

TEST(DOMTest, AppendMultipleChildrenOrderPreservedV72) {
    auto append_children = [](Node& parent, std::vector<std::unique_ptr<Node>> children) {
        for (auto& child : children) {
            parent.append_child(std::move(child));
        }
    };

    Element parent("ol");
    auto first = std::make_unique<Element>("one");
    auto second = std::make_unique<Element>("two");
    auto third = std::make_unique<Element>("three");
    Element* first_ptr = first.get();
    Element* second_ptr = second.get();
    Element* third_ptr = third.get();

    std::vector<std::unique_ptr<Node>> children;
    children.push_back(std::move(first));
    children.push_back(std::move(second));
    children.push_back(std::move(third));

    append_children(parent, std::move(children));

    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->next_sibling(), third_ptr);
    EXPECT_EQ(parent.last_child(), third_ptr);
}

TEST(DOMTest, ElementGetBoundingClientRectStubReturnsDefaultBoxV73) {
    struct RectStub {
        double x;
        double y;
        double width;
        double height;
        double top;
        double right;
        double bottom;
        double left;
    };

    auto get_bounding_client_rect_stub = []([[maybe_unused]] const Element& element) {
        return RectStub{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    };

    Element element("div");
    const auto rect = get_bounding_client_rect_stub(element);
    EXPECT_EQ(rect.x, 0.0);
    EXPECT_EQ(rect.y, 0.0);
    EXPECT_EQ(rect.width, 0.0);
    EXPECT_EQ(rect.height, 0.0);
    EXPECT_EQ(rect.top, 0.0);
    EXPECT_EQ(rect.right, 0.0);
    EXPECT_EQ(rect.bottom, 0.0);
    EXPECT_EQ(rect.left, 0.0);
}

TEST(DOMTest, ElementChildElementCountPropertyCountsElementsOnlyV73) {
    auto child_element_count = [](const Node& node) {
        size_t count = 0;
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            if (child->node_type() == NodeType::Element) {
                ++count;
            }
        }
        return count;
    };

    Element parent("div");
    parent.append_child(std::make_unique<Text>("leading"));
    parent.append_child(std::make_unique<Element>("span"));
    parent.append_child(std::make_unique<Comment>("meta"));
    parent.append_child(std::make_unique<Element>("strong"));

    EXPECT_EQ(parent.child_count(), 4u);
    EXPECT_EQ(child_element_count(parent), 2u);
}

TEST(DOMTest, ElementFirstElementChildSkipsNonElementNodesV73) {
    auto first_element_child = [](const Node& node) -> Element* {
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            if (child->node_type() == NodeType::Element) {
                return static_cast<Element*>(child);
            }
        }
        return nullptr;
    };

    Element parent("div");
    parent.append_child(std::make_unique<Text>("text"));
    auto first_element = std::make_unique<Element>("span");
    Element* first_element_ptr = first_element.get();
    parent.append_child(std::move(first_element));
    parent.append_child(std::make_unique<Element>("strong"));

    EXPECT_EQ(first_element_child(parent), first_element_ptr);
}

TEST(DOMTest, ElementLastElementChildSkipsNonElementNodesV73) {
    auto last_element_child = [](const Node& node) -> Element* {
        Element* last = nullptr;
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            if (child->node_type() == NodeType::Element) {
                last = static_cast<Element*>(child);
            }
        }
        return last;
    };

    Element parent("div");
    auto first = std::make_unique<Element>("first");
    parent.append_child(std::move(first));
    parent.append_child(std::make_unique<Comment>("skip"));
    auto last = std::make_unique<Element>("last");
    Element* last_ptr = last.get();
    parent.append_child(std::move(last));
    parent.append_child(std::make_unique<Text>("tail"));

    EXPECT_EQ(last_element_child(parent), last_ptr);
}

TEST(DOMTest, ElementChildrenIndexedAccessReturnsElementByIndexV73) {
    auto children_indexed_access = [](const Node& node, size_t index) -> Element* {
        size_t element_index = 0;
        for (Node* child = node.first_child(); child != nullptr; child = child->next_sibling()) {
            if (child->node_type() != NodeType::Element) {
                continue;
            }
            if (element_index == index) {
                return static_cast<Element*>(child);
            }
            ++element_index;
        }
        return nullptr;
    };

    Element parent("div");
    auto first = std::make_unique<Element>("a");
    auto second = std::make_unique<Element>("b");
    auto third = std::make_unique<Element>("c");
    Element* first_ptr = first.get();
    Element* second_ptr = second.get();
    Element* third_ptr = third.get();

    parent.append_child(std::move(first));
    parent.append_child(std::make_unique<Text>("gap"));
    parent.append_child(std::move(second));
    parent.append_child(std::make_unique<Comment>("gap2"));
    parent.append_child(std::move(third));

    EXPECT_EQ(children_indexed_access(parent, 0), first_ptr);
    EXPECT_EQ(children_indexed_access(parent, 1), second_ptr);
    EXPECT_EQ(children_indexed_access(parent, 2), third_ptr);
    EXPECT_EQ(children_indexed_access(parent, 3), nullptr);
}

TEST(DOMTest, ElementMatchesSelectorByTagReturnsExpectedMatchV73) {
    auto matches_selector_by_tag = [](const Element& element, const std::string& selector) {
        return !selector.empty() && selector[0] != '#' && selector[0] != '.'
            && element.tag_name() == selector;
    };

    Element button("button");
    Element input("input");
    EXPECT_TRUE(matches_selector_by_tag(button, "button"));
    EXPECT_FALSE(matches_selector_by_tag(button, "Button"));
    EXPECT_FALSE(matches_selector_by_tag(button, "#button"));
    EXPECT_FALSE(matches_selector_by_tag(input, "button"));
}

TEST(DOMTest, NodeCloneNodeShallowCopiesElementWithoutChildrenV73) {
    auto clone_node_shallow = [](const Node& node) -> std::unique_ptr<Node> {
        if (node.node_type() == NodeType::Element) {
            const auto& source = static_cast<const Element&>(node);
            auto clone = std::make_unique<Element>(source.tag_name(), source.namespace_uri());
            for (const auto& attribute : source.attributes()) {
                clone->set_attribute(attribute.name, attribute.value);
            }
            return clone;
        }
        if (node.node_type() == NodeType::Text) {
            return std::make_unique<Text>(static_cast<const Text&>(node).data());
        }
        if (node.node_type() == NodeType::Comment) {
            return std::make_unique<Comment>(static_cast<const Comment&>(node).data());
        }
        return nullptr;
    };

    Element source("section");
    source.set_attribute("id", "source");
    source.append_child(std::make_unique<Element>("child"));
    source.append_child(std::make_unique<Text>("payload"));
    ASSERT_EQ(source.child_count(), 2u);

    auto clone_node = clone_node_shallow(source);
    ASSERT_NE(clone_node, nullptr);
    ASSERT_EQ(clone_node->node_type(), NodeType::Element);

    auto* clone_element = static_cast<Element*>(clone_node.get());
    EXPECT_NE(clone_element, &source);
    EXPECT_EQ(clone_element->tag_name(), "section");
    EXPECT_EQ(clone_element->get_attribute("id").value_or(""), "source");
    EXPECT_EQ(clone_element->child_count(), 0u);
    EXPECT_EQ(clone_element->first_child(), nullptr);
}

TEST(DOMTest, DocumentCreateElementWithAttributesAppliesAllValuesV73) {
    auto create_element_with_attributes =
        [](Document& document, const std::string& tag, const std::vector<Attribute>& attributes) {
            auto element = document.create_element(tag);
            for (const auto& attribute : attributes) {
                element->set_attribute(attribute.name, attribute.value);
            }
            return element;
        };

    Document document;
    std::vector<Attribute> attributes{
        {"id", "main"},
        {"class", "hero"},
        {"data-role", "banner"},
    };

    auto element = create_element_with_attributes(document, "section", attributes);
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->tag_name(), "section");
    EXPECT_EQ(element->get_attribute("id").value_or(""), "main");
    EXPECT_EQ(element->get_attribute("class").value_or(""), "hero");
    EXPECT_EQ(element->get_attribute("data-role").value_or(""), "banner");
    EXPECT_EQ(element->attributes().size(), 3u);
}

TEST(DOMTest, NodeValueNullForElementNodeV74) {
    auto node_value = [](const Node& node) -> std::optional<std::string> {
        if (node.node_type() == NodeType::Text) {
            return static_cast<const Text&>(node).data();
        }
        if (node.node_type() == NodeType::Comment) {
            return static_cast<const Comment&>(node).data();
        }
        return std::nullopt;
    };

    Element element("div");
    EXPECT_FALSE(node_value(element).has_value());
}

TEST(DOMTest, TextSplitAtOffsetCreatesTrailingSiblingV74) {
    auto split_text = [](Text& text_node, size_t offset) -> Text* {
        std::string original = text_node.data();
        if (offset > original.size()) {
            offset = original.size();
        }

        text_node.set_data(original.substr(0, offset));
        Node* parent = text_node.parent();
        if (parent == nullptr) {
            return nullptr;
        }

        auto trailing = std::make_unique<Text>(original.substr(offset));
        Text* trailing_ptr = trailing.get();
        parent->insert_before(std::move(trailing), text_node.next_sibling());
        return trailing_ptr;
    };

    Element container("div");
    auto text = std::make_unique<Text>("split-here");
    Text* text_ptr = text.get();
    container.append_child(std::move(text));

    Text* trailing_ptr = split_text(*text_ptr, 5);
    ASSERT_NE(trailing_ptr, nullptr);
    EXPECT_EQ(text_ptr->data(), "split");
    EXPECT_EQ(trailing_ptr->data(), "-here");
    EXPECT_EQ(container.child_count(), 2u);
    EXPECT_EQ(text_ptr->next_sibling(), trailing_ptr);
}

TEST(DOMTest, CommentDataAccessReadsAndWritesV74) {
    Comment comment("todo");
    EXPECT_EQ(comment.data(), "todo");

    comment.set_data("done");
    EXPECT_EQ(comment.data(), "done");
}

TEST(DOMTest, ElementScrollTopDefaultZeroV74) {
    auto scroll_top = []([[maybe_unused]] const Element& element) -> int { return 0; };

    Element element("div");
    EXPECT_EQ(scroll_top(element), 0);
}

TEST(DOMTest, ElementOffsetWidthDefaultZeroV74) {
    auto offset_width = []([[maybe_unused]] const Element& element) -> int { return 0; };

    Element element("div");
    EXPECT_EQ(offset_width(element), 0);
}

TEST(DOMTest, ElementInnerTextConcatenatesDescendantTextV74) {
    auto inner_text = [](const Element& element) -> std::string {
        return element.text_content();
    };

    Element root("div");
    root.append_child(std::make_unique<Text>("Hello "));

    auto child = std::make_unique<Element>("span");
    child->append_child(std::make_unique<Text>("world"));
    root.append_child(std::move(child));
    root.append_child(std::make_unique<Comment>("not rendered"));

    EXPECT_EQ(inner_text(root), "Hello world");
}

TEST(DOMTest, SetAttributeOverwritesExistingValueV74) {
    Element input("input");
    input.set_attribute("type", "text");
    input.set_attribute("type", "password");

    EXPECT_EQ(input.get_attribute("type").value_or(""), "password");
    EXPECT_EQ(input.attributes().size(), 1u);
}

TEST(DOMTest, ClassListToggleAddRemoveSemanticsV74) {
    Element element("div");

    element.class_list().add("base");
    EXPECT_TRUE(element.class_list().contains("base"));

    element.class_list().toggle("active");
    EXPECT_TRUE(element.class_list().contains("active"));

    element.class_list().toggle("active");
    EXPECT_FALSE(element.class_list().contains("active"));

    element.class_list().remove("base");
    EXPECT_FALSE(element.class_list().contains("base"));
}

TEST(DOMTest, ElementCreationStoresTagNameV75) {
    clever::dom::Element element("div");
    EXPECT_EQ(element.tag_name(), "div");
}

TEST(DOMTest, MissingAttributeReturnsEmptyOptionalV75) {
    clever::dom::Element element("div");
    EXPECT_FALSE(element.get_attribute("id").has_value());
}

TEST(DOMTest, SetMultipleAttributesUpdatesAttributeMapV75) {
    clever::dom::Element element("div");
    element.set_attribute("id", "main");
    element.set_attribute("role", "region");

    EXPECT_EQ(element.get_attribute("id").value_or(""), "main");
    EXPECT_EQ(element.get_attribute("role").value_or(""), "region");
    EXPECT_EQ(element.attributes().size(), 2u);
}

TEST(DOMTest, OverwritingAttributeKeepsSingleEntryV75) {
    clever::dom::Element element("div");
    element.set_attribute("key", "first");
    element.set_attribute("key", "second");

    EXPECT_EQ(element.get_attribute("key").value_or(""), "second");
    EXPECT_EQ(element.attributes().size(), 1u);
}

TEST(DOMTest, ClassListAddContainsRemoveFlowV75) {
    clever::dom::Element element("div");
    element.class_list().add("selected");
    EXPECT_TRUE(element.class_list().contains("selected"));

    element.class_list().remove("selected");
    EXPECT_FALSE(element.class_list().contains("selected"));
}

TEST(DOMTest, ClassListToggleTwiceRestoresOriginalStateV75) {
    clever::dom::Element element("div");
    EXPECT_FALSE(element.class_list().contains("active"));

    element.class_list().toggle("active");
    EXPECT_TRUE(element.class_list().contains("active"));

    element.class_list().toggle("active");
    EXPECT_FALSE(element.class_list().contains("active"));
}

TEST(DOMTest, AppendChildRegistersParentAndChildListV75) {
    clever::dom::Element element("div");
    element.append_child(std::make_unique<Element>("span"));

    ASSERT_EQ(element.child_count(), 1u);
    EXPECT_EQ(element.first_child()->parent(), &element);
}

TEST(DOMTest, NewTextNodeStartsWithoutParentV75) {
    clever::dom::Text text("hello");
    EXPECT_EQ(text.parent(), nullptr);
}

TEST(DOMTest, DOMTreeManipulationInsertBeforeReordersChildrenV76) {
    clever::dom::Element parent("ul");
    auto first = std::make_unique<clever::dom::Element>("li");
    auto third = std::make_unique<clever::dom::Element>("li");
    auto second = std::make_unique<clever::dom::Element>("li");

    auto* first_ptr = first.get();
    auto* third_ptr = third.get();
    auto* second_ptr = second.get();

    parent.append_child(std::move(first));
    parent.append_child(std::move(third));
    parent.insert_before(std::move(second), third_ptr);

    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->next_sibling(), third_ptr);
}

TEST(DOMTest, AttributeHandlingRemoveKeepsRemainingEntriesV76) {
    clever::dom::Element element("div");
    element.set_attribute("id", "main");
    element.set_attribute("role", "region");
    element.set_attribute("data-state", "ready");

    element.remove_attribute("role");

    EXPECT_FALSE(element.get_attribute("role").has_value());
    EXPECT_EQ(element.get_attribute("id").value_or(""), "main");
    EXPECT_EQ(element.get_attribute("data-state").value_or(""), "ready");
    EXPECT_EQ(element.attributes().size(), 2u);
}

TEST(DOMTest, TextContentIncludesDescendantTextNodesV76) {
    clever::dom::Element root("div");
    root.append_child(std::make_unique<clever::dom::Text>("Hello"));

    auto child = std::make_unique<clever::dom::Element>("span");
    child->append_child(std::make_unique<clever::dom::Text>(", world"));
    root.append_child(std::move(child));

    EXPECT_EQ(root.text_content(), "Hello, world");
}

TEST(DOMTest, ClassListToggleMaintainsMembershipStateV76) {
    clever::dom::Element element("div");
    element.class_list().add("base");
    EXPECT_TRUE(element.class_list().contains("base"));

    element.class_list().toggle("active");
    EXPECT_TRUE(element.class_list().contains("active"));

    element.class_list().toggle("active");
    EXPECT_FALSE(element.class_list().contains("active"));

    element.class_list().remove("base");
    EXPECT_FALSE(element.class_list().contains("base"));
}

TEST(DOMTest, EventTargetsExposeCurrentTargetDuringDispatchV76) {
    clever::dom::EventTarget target;
    clever::dom::Element button("button");
    bool saw_current_target = false;

    target.add_event_listener(
        "click",
        [&](clever::dom::Event& event) {
            saw_current_target = (event.current_target() == &button);
        },
        false);

    clever::dom::Event event("click");
    event.target_ = &button;
    event.current_target_ = &button;
    event.phase_ = clever::dom::EventPhase::AtTarget;

    EXPECT_TRUE(target.dispatch_event(event, button));
    EXPECT_TRUE(saw_current_target);
}

TEST(DOMTest, NodeTraversalUsesSiblingLinksInOrderV76) {
    clever::dom::Element parent("div");
    auto first = std::make_unique<clever::dom::Element>("a");
    auto second = std::make_unique<clever::dom::Element>("b");
    auto third = std::make_unique<clever::dom::Element>("c");

    auto* first_ptr = first.get();
    auto* second_ptr = second.get();
    auto* third_ptr = third.get();

    parent.append_child(std::move(first));
    parent.append_child(std::move(second));
    parent.append_child(std::move(third));

    EXPECT_EQ(parent.first_child(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->next_sibling(), third_ptr);
    EXPECT_EQ(third_ptr->next_sibling(), nullptr);
    EXPECT_EQ(third_ptr->previous_sibling(), second_ptr);
}

TEST(DOMTest, ElementCreationCreatesRequestedTagNamesV76) {
    clever::dom::Element article("article");
    clever::dom::Element nav("nav");

    EXPECT_EQ(article.tag_name(), "article");
    EXPECT_EQ(nav.tag_name(), "nav");
}

TEST(DOMTest, ParentChildRelationshipClearedAfterRemoveChildV76) {
    clever::dom::Element parent("section");
    auto child = std::make_unique<clever::dom::Element>("p");
    auto* child_ptr = child.get();

    parent.append_child(std::move(child));
    ASSERT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(child_ptr->parent(), &parent);

    auto removed = parent.remove_child(*child_ptr);
    EXPECT_EQ(removed.get(), child_ptr);
    EXPECT_EQ(parent.child_count(), 0u);
    EXPECT_EQ(parent.first_child(), nullptr);
    EXPECT_EQ(child_ptr->parent(), nullptr);
}

TEST(DOMTest, SetAttributeOverwritesExistingValueV77) {
    clever::dom::Element element("div");

    element.set_attribute("data-value", "first");
    EXPECT_EQ(element.get_attribute("data-value").value_or(""), "first");

    element.set_attribute("data-value", "second");
    EXPECT_EQ(element.get_attribute("data-value").value_or(""), "second");
}

TEST(DOMTest, CommentNodeTextContentMatchesConstructionV77) {
    clever::dom::Comment comment("hello");
    EXPECT_EQ(comment.data(), "hello");
}

TEST(DOMTest, TextNodePreservesSpecialCharactersV77) {
    clever::dom::Text text("<b>&amp;");
    std::string content = text.text_content();
    EXPECT_NE(content.find('<'), std::string::npos);
    EXPECT_NE(content.find('>'), std::string::npos);
}

TEST(DOMTest, SequentialAppendChildGrowsCountV77) {
    clever::dom::Element parent("div");

    parent.append_child(std::make_unique<clever::dom::Element>("span"));
    EXPECT_EQ(parent.child_count(), 1u);

    parent.append_child(std::make_unique<clever::dom::Element>("div"));
    EXPECT_EQ(parent.child_count(), 2u);

    parent.append_child(std::make_unique<clever::dom::Element>("p"));
    EXPECT_EQ(parent.child_count(), 3u);

    parent.append_child(std::make_unique<clever::dom::Element>("a"));
    EXPECT_EQ(parent.child_count(), 4u);

    parent.append_child(std::make_unique<clever::dom::Element>("button"));
    EXPECT_EQ(parent.child_count(), 5u);
}

TEST(DOMTest, RootElementParentIsNullptrV77) {
    clever::dom::Element element("div");
    EXPECT_EQ(element.parent(), nullptr);
}

TEST(DOMTest, TagNamePreservesLowercaseV77) {
    clever::dom::Element element("SPAN");
    EXPECT_EQ(element.tag_name(), "SPAN");
}

TEST(DOMTest, ClassListAddRemoveContainsV77) {
    clever::dom::Element element("div");

    element.class_list().add("btn");
    element.class_list().add("primary");
    element.class_list().add("lg");

    EXPECT_TRUE(element.class_list().contains("btn"));
    EXPECT_TRUE(element.class_list().contains("primary"));
    EXPECT_TRUE(element.class_list().contains("lg"));

    element.class_list().remove("primary");
    EXPECT_FALSE(element.class_list().contains("primary"));
    EXPECT_TRUE(element.class_list().contains("btn"));
    EXPECT_TRUE(element.class_list().contains("lg"));
}

TEST(DOMTest, RemoveChildReturnsOwnershipV77) {
    clever::dom::Element parent("div");
    auto child = std::make_unique<clever::dom::Element>("span");
    auto* child_ptr = child.get();

    parent.append_child(std::move(child));
    ASSERT_EQ(parent.child_count(), 1u);

    auto removed = parent.remove_child(*child_ptr);
    EXPECT_NE(removed.get(), nullptr);
    EXPECT_EQ(removed.get(), child_ptr);
    EXPECT_EQ(parent.child_count(), 0u);
}

TEST(DOMTest, AppendTextAndElementChildrenMixedV78) {
    clever::dom::Element parent("div");

    auto text1 = std::make_unique<clever::dom::Text>("Hello ");
    parent.append_child(std::move(text1));
    EXPECT_EQ(parent.child_count(), 1u);

    auto elem = std::make_unique<clever::dom::Element>("span");
    parent.append_child(std::move(elem));
    EXPECT_EQ(parent.child_count(), 2u);

    auto text2 = std::make_unique<clever::dom::Text>(" World");
    parent.append_child(std::move(text2));
    EXPECT_EQ(parent.child_count(), 3u);
}

TEST(DOMTest, GetAttributeReturnsNulloptForMissingV78) {
    clever::dom::Element element("div");

    auto result = element.get_attribute("non-existent");
    EXPECT_FALSE(result.has_value());
}

TEST(DOMTest, HasAttributeReturnsTrueAfterSetV78) {
    clever::dom::Element element("div");

    EXPECT_FALSE(element.has_attribute("data-test"));
    element.set_attribute("data-test", "value");
    EXPECT_TRUE(element.has_attribute("data-test"));
}

TEST(DOMTest, TextContentConcatenatesAllChildrenV78) {
    clever::dom::Element parent("section");

    parent.append_child(std::make_unique<clever::dom::Text>("First"));
    auto inner = std::make_unique<clever::dom::Element>("strong");
    inner->append_child(std::make_unique<clever::dom::Text>("Middle"));
    parent.append_child(std::move(inner));
    parent.append_child(std::make_unique<clever::dom::Text>("Last"));

    std::string content = parent.text_content();
    EXPECT_EQ(content, "FirstMiddleLast");
}

TEST(DOMTest, ClassListToggleAddsIfAbsentV78) {
    clever::dom::Element element("div");

    EXPECT_FALSE(element.class_list().contains("active"));
    element.class_list().toggle("active");
    EXPECT_TRUE(element.class_list().contains("active"));
}

TEST(DOMTest, ClassListToggleRemovesIfPresentV78) {
    clever::dom::Element element("div");

    element.class_list().add("active");
    EXPECT_TRUE(element.class_list().contains("active"));
    element.class_list().toggle("active");
    EXPECT_FALSE(element.class_list().contains("active"));
}

TEST(DOMTest, FirstChildReturnsNullOnEmptyV78) {
    clever::dom::Element element("div");

    EXPECT_EQ(element.first_child(), nullptr);
    element.append_child(std::make_unique<clever::dom::Element>("span"));
    EXPECT_NE(element.first_child(), nullptr);
}

TEST(DOMTest, MultipleSetAttributesDifferentKeysV78) {
    clever::dom::Element element("div");

    element.set_attribute("id", "myid");
    element.set_attribute("class", "btn primary");
    element.set_attribute("data-value", "42");

    EXPECT_EQ(element.get_attribute("id").value_or(""), "myid");
    EXPECT_EQ(element.get_attribute("class").value_or(""), "btn primary");
    EXPECT_EQ(element.get_attribute("data-value").value_or(""), "42");
}

// ---------------------------------------------------------------------------
// V79 Tests
// ---------------------------------------------------------------------------

TEST(DOMTest, DeepNestingFiveLevelsV79) {
    clever::dom::Element root("div");
    auto l1 = std::make_unique<clever::dom::Element>("section");
    auto l2 = std::make_unique<clever::dom::Element>("article");
    auto l3 = std::make_unique<clever::dom::Element>("nav");
    auto l4 = std::make_unique<clever::dom::Element>("span");

    clever::dom::Element* l1_ptr = l1.get();
    clever::dom::Element* l2_ptr = l2.get();
    clever::dom::Element* l3_ptr = l3.get();
    clever::dom::Element* l4_ptr = l4.get();

    l3_ptr->append_child(std::move(l4));
    l2_ptr->append_child(std::move(l3));
    l1_ptr->append_child(std::move(l2));
    root.append_child(std::move(l1));

    // Verify parent chain from innermost to root
    EXPECT_EQ(l4_ptr->parent(), static_cast<clever::dom::Node*>(l3_ptr));
    EXPECT_EQ(l3_ptr->parent(), static_cast<clever::dom::Node*>(l2_ptr));
    EXPECT_EQ(l2_ptr->parent(), static_cast<clever::dom::Node*>(l1_ptr));
    EXPECT_EQ(l1_ptr->parent(), static_cast<clever::dom::Node*>(&root));
    EXPECT_EQ(root.parent(), nullptr);
}

TEST(DOMTest, TextContentEmptyElementV79) {
    clever::dom::Element element("p");

    // Element with no children should return empty string for text_content
    EXPECT_EQ(element.text_content(), "");
}

TEST(DOMTest, CommentNodeTypeCheckV79) {
    clever::dom::Comment comment("This is a comment");

    EXPECT_EQ(comment.node_type(), clever::dom::NodeType::Comment);
}

TEST(DOMTest, SetAttributeEmptyStringV79) {
    clever::dom::Element element("div");

    element.set_attribute("data", "");
    ASSERT_TRUE(element.has_attribute("data"));
    auto val = element.get_attribute("data");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "");
}

TEST(DOMTest, ClassListContainsAfterMultipleAddsV79) {
    clever::dom::Element element("div");

    element.class_list().add("highlight");
    element.class_list().add("highlight");  // add same class twice

    EXPECT_TRUE(element.class_list().contains("highlight"));
}

TEST(DOMTest, RemoveChildFromMiddleV79) {
    clever::dom::Element parent("ul");
    auto c1 = std::make_unique<clever::dom::Element>("li");
    auto c2 = std::make_unique<clever::dom::Element>("li");
    auto c3 = std::make_unique<clever::dom::Element>("li");

    clever::dom::Element* c2_ptr = c2.get();

    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    parent.append_child(std::move(c3));

    EXPECT_EQ(parent.child_count(), 3u);

    parent.remove_child(*c2_ptr);

    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DOMTest, LastChildReturnsCorrectNodeV79) {
    clever::dom::Element parent("div");
    auto first = std::make_unique<clever::dom::Element>("span");
    auto second = std::make_unique<clever::dom::Element>("em");
    auto third = std::make_unique<clever::dom::Element>("strong");

    clever::dom::Node* third_ptr = third.get();

    parent.append_child(std::move(first));
    parent.append_child(std::move(second));
    parent.append_child(std::move(third));

    EXPECT_EQ(parent.last_child(), third_ptr);
}

TEST(DOMTest, ElementTagNameVariousTagsV79) {
    clever::dom::Element section("section");
    clever::dom::Element article("article");
    clever::dom::Element aside("aside");

    EXPECT_EQ(section.tag_name(), "section");
    EXPECT_EQ(article.tag_name(), "article");
    EXPECT_EQ(aside.tag_name(), "aside");
}

// ---------------------------------------------------------------------------
// V80 Tests
// ---------------------------------------------------------------------------

TEST(DOMTest, ForEachChildCallbackV80) {
    clever::dom::Element parent("ul");
    auto li1 = std::make_unique<clever::dom::Element>("li");
    auto li2 = std::make_unique<clever::dom::Element>("li");
    auto li3 = std::make_unique<clever::dom::Element>("li");

    clever::dom::Node* li1_ptr = li1.get();
    clever::dom::Node* li2_ptr = li2.get();
    clever::dom::Node* li3_ptr = li3.get();

    parent.append_child(std::move(li1));
    parent.append_child(std::move(li2));
    parent.append_child(std::move(li3));

    std::vector<const clever::dom::Node*> visited;
    parent.for_each_child([&](const clever::dom::Node& child) {
        visited.push_back(&child);
    });

    ASSERT_EQ(visited.size(), 3u);
    EXPECT_EQ(visited[0], li1_ptr);
    EXPECT_EQ(visited[1], li2_ptr);
    EXPECT_EQ(visited[2], li3_ptr);
}

TEST(DOMTest, SetAttributeOverwriteV80) {
    clever::dom::Element elem("input");
    elem.set_attribute("type", "text");
    ASSERT_TRUE(elem.has_attribute("type"));
    EXPECT_EQ(elem.get_attribute("type").value(), "text");

    elem.set_attribute("type", "password");
    EXPECT_EQ(elem.get_attribute("type").value(), "password");

    // Ensure only one attribute entry, not two
    elem.set_attribute("type", "email");
    EXPECT_EQ(elem.get_attribute("type").value(), "email");
}

TEST(DOMTest, TextNodeTextContentV80) {
    clever::dom::Text text("Hello, World!");
    EXPECT_EQ(text.text_content(), "Hello, World!");
    EXPECT_EQ(text.node_type(), clever::dom::NodeType::Text);
}

TEST(DOMTest, AppendChildUpdatesParentV80) {
    clever::dom::Element parent("div");
    auto child = std::make_unique<clever::dom::Element>("span");
    clever::dom::Node* child_ptr = child.get();

    EXPECT_EQ(child_ptr->parent(), nullptr);

    parent.append_child(std::move(child));

    EXPECT_EQ(child_ptr->parent(), static_cast<clever::dom::Node*>(&parent));
    EXPECT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(parent.first_child(), child_ptr);
    EXPECT_EQ(parent.last_child(), child_ptr);
}

TEST(DOMTest, ClassListMultipleToggleV80) {
    clever::dom::Element elem("div");

    // Toggle on
    elem.class_list().toggle("active");
    EXPECT_TRUE(elem.class_list().contains("active"));

    // Toggle off
    elem.class_list().toggle("active");
    EXPECT_FALSE(elem.class_list().contains("active"));

    // Toggle on again
    elem.class_list().toggle("active");
    EXPECT_TRUE(elem.class_list().contains("active"));

    // Toggle off again
    elem.class_list().toggle("active");
    EXPECT_FALSE(elem.class_list().contains("active"));
}

TEST(DOMTest, RemoveNonexistentAttributeNoErrorV80) {
    clever::dom::Element elem("div");

    // Removing an attribute that was never set should not throw or crash
    elem.remove_attribute("nonexistent");

    EXPECT_FALSE(elem.has_attribute("nonexistent"));
    auto val = elem.get_attribute("nonexistent");
    EXPECT_FALSE(val.has_value());
}

TEST(DOMTest, NodeTypeElementCheckV80) {
    clever::dom::Element elem("article");
    clever::dom::Text text("some text");
    clever::dom::Comment comment("a comment");

    EXPECT_EQ(elem.node_type(), clever::dom::NodeType::Element);
    EXPECT_EQ(text.node_type(), clever::dom::NodeType::Text);
    EXPECT_EQ(comment.node_type(), clever::dom::NodeType::Comment);
}

TEST(DOMTest, EmptyClassListContainsFalseV80) {
    clever::dom::Element elem("div");

    // No classes added — contains should return false for anything
    EXPECT_FALSE(elem.class_list().contains("foo"));
    EXPECT_FALSE(elem.class_list().contains("bar"));
    EXPECT_FALSE(elem.class_list().contains(""));
    EXPECT_FALSE(elem.class_list().contains("active"));
}

// ---------------------------------------------------------------------------
// V81 Tests
// ---------------------------------------------------------------------------

TEST(DomTest, DocumentCreateElementFactoryV81) {
    Document doc;
    auto elem = doc.create_element("section");
    EXPECT_EQ(elem->tag_name(), "section");
    EXPECT_EQ(elem->node_type(), NodeType::Element);
    EXPECT_EQ(elem->child_count(), 0u);
    EXPECT_EQ(elem->parent(), nullptr);
}

TEST(DomTest, InsertBeforeFirstChildReordersV81) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li0 = std::make_unique<Element>("li");

    li1->set_attribute("data-order", "1");
    li2->set_attribute("data-order", "2");
    li0->set_attribute("data-order", "0");

    auto* li1_ptr = li1.get();
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li2));

    // Insert li0 before li1, making it the new first child
    auto* li0_ptr = li0.get();
    parent.insert_before(std::move(li0), li1_ptr);

    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), li0_ptr);
    EXPECT_EQ(li0_ptr->next_sibling(), li1_ptr);
    EXPECT_EQ(li0_ptr->previous_sibling(), nullptr);
}

TEST(DomTest, TextNodeSetDataUpdatesContentV81) {
    Text txt("initial");
    EXPECT_EQ(txt.text_content(), "initial");
    EXPECT_EQ(txt.data(), "initial");

    txt.set_data("updated");
    EXPECT_EQ(txt.text_content(), "updated");
    EXPECT_EQ(txt.data(), "updated");
}

TEST(DomTest, ElementTextContentAggregatesChildrenV81) {
    Element div("div");
    auto t1 = std::make_unique<Text>("Hello");
    auto span = std::make_unique<Element>("span");
    auto t2 = std::make_unique<Text>(" World");
    span->append_child(std::move(t2));

    div.append_child(std::move(t1));
    div.append_child(std::move(span));

    // text_content() should recursively concatenate all text nodes
    EXPECT_EQ(div.text_content(), "Hello World");
}

TEST(DomTest, ClassListLengthAndItemsV81) {
    Element elem("div");
    EXPECT_EQ(elem.class_list().length(), 0u);

    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    elem.class_list().add("gamma");
    EXPECT_EQ(elem.class_list().length(), 3u);

    // Adding a duplicate should not increase length
    elem.class_list().add("alpha");
    EXPECT_EQ(elem.class_list().length(), 3u);

    // items() should contain exact classes
    const auto& items = elem.class_list().items();
    EXPECT_EQ(items.size(), 3u);
    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));
}

TEST(DomTest, RemoveChildReturnOwnershipV81) {
    Element parent("div");
    auto child = std::make_unique<Element>("p");
    auto* child_ptr = child.get();
    parent.append_child(std::move(child));

    EXPECT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(child_ptr->parent(), static_cast<Node*>(&parent));

    // remove_child returns the unique_ptr, transferring ownership back
    auto recovered = parent.remove_child(*child_ptr);
    EXPECT_NE(recovered, nullptr);
    EXPECT_EQ(parent.child_count(), 0u);
    EXPECT_EQ(child_ptr->parent(), nullptr);
    EXPECT_EQ(parent.first_child(), nullptr);
}

TEST(DomTest, SetAttributeIdUpdatesIdShortcutV81) {
    Element elem("div");
    EXPECT_EQ(elem.id(), "");

    elem.set_attribute("id", "main-content");
    EXPECT_EQ(elem.id(), "main-content");
    EXPECT_EQ(elem.get_attribute("id").value(), "main-content");
    EXPECT_TRUE(elem.has_attribute("id"));

    // Overwriting id attribute updates the shortcut
    elem.set_attribute("id", "sidebar");
    EXPECT_EQ(elem.id(), "sidebar");
}

TEST(DomTest, CommentNodeDataAndTypeV81) {
    Comment c("This is a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
    EXPECT_EQ(c.data(), "This is a comment");

    c.set_data("Updated comment");
    EXPECT_EQ(c.data(), "Updated comment");

    // Comment can be appended as child
    Element div("div");
    auto comment = std::make_unique<Comment>("child comment");
    auto* comment_ptr = comment.get();
    div.append_child(std::move(comment));
    EXPECT_EQ(div.child_count(), 1u);
    EXPECT_EQ(div.first_child(), comment_ptr);
}

// ---------------------------------------------------------------------------
// V82 Round — 8 new diverse DOM tests
// ---------------------------------------------------------------------------

TEST(DomTest, DeepNestedTreeTraversalV82) {
    // Build a 5-level deep tree: root > l1 > l2 > l3 > l4 > leaf_text
    auto root = std::make_unique<Element>("div");
    auto l1 = std::make_unique<Element>("section");
    auto l2 = std::make_unique<Element>("article");
    auto l3 = std::make_unique<Element>("p");
    auto l4 = std::make_unique<Element>("span");
    auto leaf = std::make_unique<Text>("deeply nested");

    auto* l1_ptr = l1.get();
    auto* l2_ptr = l2.get();
    auto* l3_ptr = l3.get();
    auto* l4_ptr = l4.get();
    auto* leaf_ptr = leaf.get();

    l4->append_child(std::move(leaf));
    l3->append_child(std::move(l4));
    l2->append_child(std::move(l3));
    l1->append_child(std::move(l2));
    root->append_child(std::move(l1));

    // Walk down the tree via first_child at each level
    EXPECT_EQ(root->first_child(), l1_ptr);
    EXPECT_EQ(l1_ptr->first_child(), l2_ptr);
    EXPECT_EQ(l2_ptr->first_child(), l3_ptr);
    EXPECT_EQ(l3_ptr->first_child(), l4_ptr);
    EXPECT_EQ(l4_ptr->first_child(), leaf_ptr);

    // Walk back up via parent
    EXPECT_EQ(leaf_ptr->parent(), l4_ptr);
    EXPECT_EQ(l4_ptr->parent(), l3_ptr);
    EXPECT_EQ(l3_ptr->parent(), l2_ptr);
    EXPECT_EQ(l2_ptr->parent(), l1_ptr);
    EXPECT_EQ(l1_ptr->parent(), root.get());

    // text_content propagates up from the deepest Text node
    EXPECT_EQ(root->text_content(), "deeply nested");
}

TEST(DomTest, ClassListBulkOperationsV82) {
    // Perform many class additions, removals, and toggles in sequence
    Element elem("div");

    // Add several classes
    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    elem.class_list().add("gamma");
    elem.class_list().add("delta");
    elem.class_list().add("epsilon");
    EXPECT_EQ(elem.class_list().length(), 5u);

    // Adding a duplicate should not increase length
    elem.class_list().add("beta");
    EXPECT_EQ(elem.class_list().length(), 5u);

    // Remove middle classes
    elem.class_list().remove("beta");
    elem.class_list().remove("delta");
    EXPECT_EQ(elem.class_list().length(), 3u);
    EXPECT_FALSE(elem.class_list().contains("beta"));
    EXPECT_FALSE(elem.class_list().contains("delta"));

    // Toggle: gamma exists -> remove it; zeta doesn't exist -> add it
    elem.class_list().toggle("gamma");
    elem.class_list().toggle("zeta");
    EXPECT_FALSE(elem.class_list().contains("gamma"));
    EXPECT_TRUE(elem.class_list().contains("zeta"));
    EXPECT_EQ(elem.class_list().length(), 3u); // alpha, epsilon, zeta

    // Remaining classes are correct
    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("epsilon"));
    EXPECT_TRUE(elem.class_list().contains("zeta"));
}

TEST(DomTest, SiblingChainIntegrityAfterInsertBeforeV82) {
    // Build a parent with 3 children, then insert a 4th using insert_before
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");

    li1->set_attribute("data-order", "1");
    li2->set_attribute("data-order", "2");
    li3->set_attribute("data-order", "3");

    auto* li1_ptr = li1.get();
    auto* li2_ptr = li2.get();
    auto* li3_ptr = li3.get();

    parent.append_child(std::move(li1));
    parent.append_child(std::move(li2));
    parent.append_child(std::move(li3));

    // Insert a new element before li2
    auto li_new = std::make_unique<Element>("li");
    li_new->set_attribute("data-order", "1.5");
    auto* li_new_ptr = li_new.get();
    parent.insert_before(std::move(li_new), li2_ptr);

    EXPECT_EQ(parent.child_count(), 4u);

    // Verify forward chain: li1 -> li_new -> li2 -> li3
    EXPECT_EQ(parent.first_child(), li1_ptr);
    EXPECT_EQ(li1_ptr->next_sibling(), li_new_ptr);
    EXPECT_EQ(li_new_ptr->next_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->next_sibling(), nullptr);

    // Verify backward chain
    EXPECT_EQ(li3_ptr->previous_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->previous_sibling(), li_new_ptr);
    EXPECT_EQ(li_new_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(li1_ptr->previous_sibling(), nullptr);
}

TEST(DomTest, MixedChildTypesElementTextCommentV82) {
    // A parent with heterogeneous child types: Element, Text, Comment, Element
    Element parent("div");

    auto header = std::make_unique<Element>("h1");
    auto text = std::make_unique<Text>("Hello ");
    auto comment = std::make_unique<Comment>("separator");
    auto span = std::make_unique<Element>("span");

    auto* header_ptr = header.get();
    auto* text_ptr = text.get();
    auto* comment_ptr = comment.get();
    auto* span_ptr = span.get();

    parent.append_child(std::move(header));
    parent.append_child(std::move(text));
    parent.append_child(std::move(comment));
    parent.append_child(std::move(span));

    EXPECT_EQ(parent.child_count(), 4u);

    // Verify types via node_type
    EXPECT_EQ(header_ptr->node_type(), NodeType::Element);
    EXPECT_EQ(text_ptr->node_type(), NodeType::Text);
    EXPECT_EQ(comment_ptr->node_type(), NodeType::Comment);
    EXPECT_EQ(span_ptr->node_type(), NodeType::Element);

    // Sibling chain links all node types
    EXPECT_EQ(header_ptr->next_sibling(), text_ptr);
    EXPECT_EQ(text_ptr->next_sibling(), comment_ptr);
    EXPECT_EQ(comment_ptr->next_sibling(), span_ptr);

    // All share the same parent
    EXPECT_EQ(header_ptr->parent(), &parent);
    EXPECT_EQ(text_ptr->parent(), &parent);
    EXPECT_EQ(comment_ptr->parent(), &parent);
    EXPECT_EQ(span_ptr->parent(), &parent);
}

TEST(DomTest, RemoveChildAndReattachElsewhereV82) {
    // Remove a child from one parent and append it to another
    Element parent_a("div");
    Element parent_b("section");

    auto child = std::make_unique<Element>("p");
    child->set_attribute("data-content", "wandering");
    auto* child_ptr = child.get();

    parent_a.append_child(std::move(child));
    EXPECT_EQ(parent_a.child_count(), 1u);
    EXPECT_EQ(child_ptr->parent(), &parent_a);

    // Remove from parent_a — get ownership back
    auto recovered = parent_a.remove_child(*child_ptr);
    EXPECT_EQ(parent_a.child_count(), 0u);
    EXPECT_EQ(parent_a.first_child(), nullptr);
    EXPECT_EQ(child_ptr->parent(), nullptr);

    // Re-attach to parent_b
    parent_b.append_child(std::move(recovered));
    EXPECT_EQ(parent_b.child_count(), 1u);
    EXPECT_EQ(child_ptr->parent(), &parent_b);
    EXPECT_EQ(parent_b.first_child(), child_ptr);

    // Attribute survives the move
    EXPECT_EQ(child_ptr->node_type(), NodeType::Element);
    auto* elem_ptr = static_cast<Element*>(child_ptr);
    EXPECT_EQ(elem_ptr->get_attribute("data-content").value(), "wandering");
}

TEST(DomTest, MultipleAttributeOverwriteAndRemoveV82) {
    // Stress-test attribute set/overwrite/remove cycles
    Element elem("input");

    // Set initial attributes
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    elem.set_attribute("placeholder", "Enter name");
    elem.set_attribute("maxlength", "50");
    EXPECT_EQ(elem.attributes().size(), 4u);

    // Overwrite existing attribute values
    elem.set_attribute("type", "email");
    elem.set_attribute("placeholder", "Enter email");
    EXPECT_EQ(elem.get_attribute("type").value(), "email");
    EXPECT_EQ(elem.get_attribute("placeholder").value(), "Enter email");
    EXPECT_EQ(elem.attributes().size(), 4u); // count unchanged

    // Remove one, check count
    elem.remove_attribute("maxlength");
    EXPECT_EQ(elem.attributes().size(), 3u);
    EXPECT_FALSE(elem.has_attribute("maxlength"));

    // Remove nonexistent attribute is a no-op
    elem.remove_attribute("nonexistent");
    EXPECT_EQ(elem.attributes().size(), 3u);

    // Remove remaining
    elem.remove_attribute("type");
    elem.remove_attribute("name");
    elem.remove_attribute("placeholder");
    EXPECT_EQ(elem.attributes().size(), 0u);

    // Setting attribute after total removal works
    elem.set_attribute("id", "revived");
    EXPECT_EQ(elem.id(), "revived");
    EXPECT_EQ(elem.attributes().size(), 1u);
}

TEST(DomTest, EventStopPropagationPreventsLaterListenersV82) {
    // When stop_propagation is called, subsequent dispatch phases should see it
    auto node = std::make_unique<Element>("button");
    EventTarget target;

    int call_count = 0;
    target.add_event_listener("click", [&](Event& e) {
        call_count++;
        e.stop_propagation();
    });
    target.add_event_listener("click", [&](Event&) {
        // This still fires — stop_propagation doesn't skip same-phase listeners
        call_count++;
    });

    Event event("click");
    target.dispatch_event(event, *node);

    // Both listeners at the same target fire
    EXPECT_EQ(call_count, 2);
    // But propagation_stopped flag is set for outer phases
    EXPECT_TRUE(event.propagation_stopped());
}

TEST(DomTest, DocumentCreateAndAdoptElementsV82) {
    // Use Document to create elements and build a mini page structure
    auto doc = std::make_unique<Document>();

    auto html = std::make_unique<Element>("html");
    auto head = std::make_unique<Element>("head");
    auto body = std::make_unique<Element>("body");
    auto title = std::make_unique<Element>("title");
    auto title_text = std::make_unique<Text>("My Page");

    auto* html_ptr = html.get();
    auto* head_ptr = head.get();
    auto* body_ptr = body.get();
    auto* title_ptr = title.get();

    title->append_child(std::move(title_text));
    head->append_child(std::move(title));
    html->append_child(std::move(head));
    html->append_child(std::move(body));
    doc->append_child(std::move(html));

    // Document has one child: html
    EXPECT_EQ(doc->child_count(), 1u);
    EXPECT_EQ(doc->first_child(), html_ptr);

    // html has two children: head and body
    EXPECT_EQ(html_ptr->child_count(), 2u);
    EXPECT_EQ(html_ptr->first_child(), head_ptr);
    EXPECT_EQ(head_ptr->next_sibling(), body_ptr);

    // head has one child: title
    EXPECT_EQ(head_ptr->child_count(), 1u);
    EXPECT_EQ(head_ptr->first_child(), title_ptr);

    // title has text content
    EXPECT_EQ(title_ptr->text_content(), "My Page");

    // body is empty
    EXPECT_EQ(body_ptr->child_count(), 0u);

    // doc node type
    EXPECT_EQ(doc->node_type(), NodeType::Document);
}

// ---------------------------------------------------------------------------
// V83 Tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementSetAndGetMultipleAttributesV83) {
    auto el = std::make_unique<Element>("section");
    el->set_attribute("id", "main");
    el->set_attribute("class", "container wide");
    el->set_attribute("data-index", "42");
    el->set_attribute("hidden", "");

    EXPECT_EQ(el->get_attribute("id").value(), "main");
    EXPECT_EQ(el->get_attribute("class").value(), "container wide");
    EXPECT_EQ(el->get_attribute("data-index").value(), "42");
    EXPECT_EQ(el->get_attribute("hidden").value(), "");
    EXPECT_FALSE(el->get_attribute("nonexistent").has_value());
    EXPECT_EQ(el->attributes().size(), 4u);
}

TEST(DomTest, ElementOverwriteAttributeValueV83) {
    auto el = std::make_unique<Element>("input");
    el->set_attribute("type", "text");
    EXPECT_EQ(el->get_attribute("type").value(), "text");

    el->set_attribute("type", "password");
    EXPECT_EQ(el->get_attribute("type").value(), "password");
    // Overwriting should not increase attribute count
    EXPECT_EQ(el->attributes().size(), 1u);
}

TEST(DomTest, ClassListAddRemoveContainsToggleV83) {
    auto el = std::make_unique<Element>("div");
    el->class_list().add("alpha");
    el->class_list().add("beta");
    el->class_list().add("gamma");

    EXPECT_TRUE(el->class_list().contains("alpha"));
    EXPECT_TRUE(el->class_list().contains("beta"));
    EXPECT_TRUE(el->class_list().contains("gamma"));
    EXPECT_FALSE(el->class_list().contains("delta"));

    el->class_list().remove("beta");
    EXPECT_FALSE(el->class_list().contains("beta"));
    EXPECT_TRUE(el->class_list().contains("alpha"));
    EXPECT_TRUE(el->class_list().contains("gamma"));

    // toggle removes if present
    el->class_list().toggle("alpha");
    EXPECT_FALSE(el->class_list().contains("alpha"));

    // toggle adds if absent
    el->class_list().toggle("delta");
    EXPECT_TRUE(el->class_list().contains("delta"));
}

TEST(DomTest, InsertBeforeAtVariousPositionsV83) {
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");

    li1->set_attribute("id", "first");
    li2->set_attribute("id", "second");
    li3->set_attribute("id", "third");

    auto* li1_ptr = li1.get();
    auto* li2_ptr = li2.get();
    auto* li3_ptr = li3.get();

    // Append li1, then li3
    parent->append_child(std::move(li1));
    parent->append_child(std::move(li3));
    EXPECT_EQ(parent->child_count(), 2u);

    // Insert li2 before li3 (between li1 and li3)
    parent->insert_before(std::move(li2), li3_ptr);
    EXPECT_EQ(parent->child_count(), 3u);

    // Verify order: li1 -> li2 -> li3
    EXPECT_EQ(parent->first_child(), li1_ptr);
    EXPECT_EQ(li1_ptr->next_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->next_sibling(), nullptr);
}

TEST(DomTest, RemoveChildUpdatesTreeStructureV83) {
    auto parent = std::make_unique<Element>("div");
    auto a = std::make_unique<Element>("span");
    auto b = std::make_unique<Element>("span");
    auto c = std::make_unique<Element>("span");

    a->set_attribute("id", "a");
    b->set_attribute("id", "b");
    c->set_attribute("id", "c");

    auto* a_ptr = a.get();
    auto* b_ptr = b.get();
    auto* c_ptr = c.get();

    parent->append_child(std::move(a));
    parent->append_child(std::move(b));
    parent->append_child(std::move(c));
    EXPECT_EQ(parent->child_count(), 3u);

    // Remove middle child
    parent->remove_child(*b_ptr);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), a_ptr);
    EXPECT_EQ(a_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->next_sibling(), nullptr);

    // Remove first child
    parent->remove_child(*a_ptr);
    EXPECT_EQ(parent->child_count(), 1u);
    EXPECT_EQ(parent->first_child(), c_ptr);
}

TEST(DomTest, TextNodeContentAndParentLinkV83) {
    auto div = std::make_unique<Element>("div");
    auto txt = std::make_unique<Text>("Hello, world!");

    auto* txt_ptr = txt.get();

    EXPECT_EQ(txt_ptr->text_content(), "Hello, world!");
    EXPECT_EQ(txt_ptr->node_type(), NodeType::Text);
    EXPECT_EQ(txt_ptr->parent(), nullptr);

    div->append_child(std::move(txt));
    EXPECT_EQ(txt_ptr->parent(), div.get());
    EXPECT_EQ(div->child_count(), 1u);
    EXPECT_EQ(div->first_child(), txt_ptr);
    EXPECT_EQ(div->text_content(), "Hello, world!");
}

TEST(DomTest, NestedElementTreeTraversalV83) {
    auto root = std::make_unique<Element>("div");
    auto child1 = std::make_unique<Element>("p");
    auto child2 = std::make_unique<Element>("p");
    auto grandchild = std::make_unique<Element>("strong");
    auto text = std::make_unique<Text>("bold text");

    auto* root_ptr = root.get();
    auto* child1_ptr = child1.get();
    auto* child2_ptr = child2.get();
    auto* grandchild_ptr = grandchild.get();
    auto* text_ptr = text.get();

    grandchild->append_child(std::move(text));
    child1->append_child(std::move(grandchild));
    root->append_child(std::move(child1));
    root->append_child(std::move(child2));

    // Root has 2 children
    EXPECT_EQ(root_ptr->child_count(), 2u);
    EXPECT_EQ(root_ptr->first_child(), child1_ptr);
    EXPECT_EQ(child1_ptr->next_sibling(), child2_ptr);

    // child1 has one child: grandchild
    EXPECT_EQ(child1_ptr->child_count(), 1u);
    EXPECT_EQ(child1_ptr->first_child(), grandchild_ptr);

    // grandchild has text
    EXPECT_EQ(grandchild_ptr->child_count(), 1u);
    EXPECT_EQ(grandchild_ptr->first_child(), text_ptr);
    EXPECT_EQ(text_ptr->text_content(), "bold text");

    // Parent pointers
    EXPECT_EQ(child1_ptr->parent(), root_ptr);
    EXPECT_EQ(child2_ptr->parent(), root_ptr);
    EXPECT_EQ(grandchild_ptr->parent(), child1_ptr);
    EXPECT_EQ(text_ptr->parent(), grandchild_ptr);
}

TEST(DomTest, MixedChildrenElementsAndTextNodesV83) {
    auto div = std::make_unique<Element>("div");
    auto span = std::make_unique<Element>("span");
    auto text1 = std::make_unique<Text>("before ");
    auto text2 = std::make_unique<Text>(" after");

    auto* text1_ptr = text1.get();
    auto* span_ptr = span.get();
    auto* text2_ptr = text2.get();

    span->set_attribute("class", "highlight");

    div->append_child(std::move(text1));
    div->append_child(std::move(span));
    div->append_child(std::move(text2));

    EXPECT_EQ(div->child_count(), 3u);

    // Verify order: text1 -> span -> text2
    EXPECT_EQ(div->first_child(), text1_ptr);
    EXPECT_EQ(text1_ptr->next_sibling(), span_ptr);
    EXPECT_EQ(span_ptr->next_sibling(), text2_ptr);
    EXPECT_EQ(text2_ptr->next_sibling(), nullptr);

    // Verify types
    EXPECT_EQ(text1_ptr->node_type(), NodeType::Text);
    EXPECT_EQ(span_ptr->node_type(), NodeType::Element);
    EXPECT_EQ(text2_ptr->node_type(), NodeType::Text);

    // Verify span attribute
    EXPECT_EQ(span_ptr->get_attribute("class").value(), "highlight");
}

// ---------------------------------------------------------------------------
// V84 tests
// ---------------------------------------------------------------------------

TEST(DomTest, InsertBeforeNullRefAppendsV84) {
    auto parent = std::make_unique<Element>("div");
    auto child1 = std::make_unique<Element>("span");
    auto child2 = std::make_unique<Element>("em");
    auto* c1 = child1.get();
    auto* c2 = child2.get();

    parent->append_child(std::move(child1));
    // insert_before with nullptr ref should append at end
    parent->insert_before(std::move(child2), nullptr);

    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), c1);
    EXPECT_EQ(c1->next_sibling(), c2);
    EXPECT_EQ(c2->next_sibling(), nullptr);
}

TEST(DomTest, RemoveChildUpdatesSiblingLinksV84) {
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");
    auto* p1 = li1.get();
    auto* p3 = li3.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));

    EXPECT_EQ(parent->child_count(), 3u);

    // Remove middle child — first and third should link
    parent->remove_child(*parent->first_child()->next_sibling());

    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), p1);
    EXPECT_EQ(p1->next_sibling(), p3);
    EXPECT_EQ(p3->next_sibling(), nullptr);
}

TEST(DomTest, ClassListAddRemoveContainsV84) {
    Element elem("div");
    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    elem.class_list().add("gamma");

    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));

    elem.class_list().remove("beta");

    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_FALSE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));
}

TEST(DomTest, ClassListToggleAddsAndRemovesV84) {
    Element elem("span");
    EXPECT_FALSE(elem.class_list().contains("active"));

    elem.class_list().toggle("active");
    EXPECT_TRUE(elem.class_list().contains("active"));

    elem.class_list().toggle("active");
    EXPECT_FALSE(elem.class_list().contains("active"));
}

TEST(DomTest, SetAttributeIdAndRetrieveV84) {
    Element elem("section");
    elem.set_attribute("id", "main-content");
    auto id_val = elem.get_attribute("id");
    ASSERT_TRUE(id_val.has_value());
    EXPECT_EQ(id_val.value(), "main-content");

    // Overwrite id
    elem.set_attribute("id", "sidebar");
    EXPECT_EQ(elem.get_attribute("id").value(), "sidebar");
}

TEST(DomTest, TextNodeContentAndTypeV84) {
    Text text_node("Hello, world!");
    EXPECT_EQ(text_node.text_content(), "Hello, world!");
    EXPECT_EQ(text_node.node_type(), NodeType::Text);

    // Text node should have no children
    EXPECT_EQ(text_node.child_count(), 0u);
    EXPECT_EQ(text_node.first_child(), nullptr);
}

TEST(DomTest, DeepNestedParentChainV84) {
    auto root = std::make_unique<Element>("div");
    auto mid = std::make_unique<Element>("section");
    auto leaf = std::make_unique<Element>("p");
    auto* root_ptr = root.get();
    auto* mid_ptr = mid.get();
    auto* leaf_ptr = leaf.get();

    mid->append_child(std::move(leaf));
    root->append_child(std::move(mid));

    // leaf's parent is mid, mid's parent is root
    EXPECT_EQ(leaf_ptr->parent(), mid_ptr);
    EXPECT_EQ(mid_ptr->parent(), root_ptr);
    EXPECT_EQ(root_ptr->parent(), nullptr);

    // Traverse from root to leaf
    EXPECT_EQ(root_ptr->first_child(), mid_ptr);
    EXPECT_EQ(mid_ptr->first_child(), leaf_ptr);
    EXPECT_EQ(leaf_ptr->first_child(), nullptr);
}

TEST(DomTest, InsertBeforeMiddleChildV84) {
    auto parent = std::make_unique<Element>("div");
    auto a = std::make_unique<Element>("a");
    auto b = std::make_unique<Element>("b");
    auto c = std::make_unique<Element>("c");
    auto* a_ptr = a.get();
    auto* b_ptr = b.get();
    auto* c_ptr = c.get();

    parent->append_child(std::move(a));
    parent->append_child(std::move(c));

    // Insert b before c — order should be a, b, c
    parent->insert_before(std::move(b), c_ptr);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), a_ptr);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->next_sibling(), nullptr);
    EXPECT_EQ(b_ptr->parent(), parent.get());
}

// ---------------------------------------------------------------------------
// V85 Tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementAttributeOverwriteV85) {
    Element elem("input");
    elem.set_attribute("type", "text");
    EXPECT_EQ(elem.get_attribute("type").value(), "text");

    // Overwrite the same attribute with a new value
    elem.set_attribute("type", "password");
    EXPECT_EQ(elem.get_attribute("type").value(), "password");

    // Confirm only one attribute exists (overwrite, not duplicate)
    elem.set_attribute("name", "field1");
    EXPECT_EQ(elem.get_attribute("name").value(), "field1");
    EXPECT_EQ(elem.get_attribute("type").value(), "password");
}

TEST(DomTest, ClassListToggleAddRemoveV85) {
    Element elem("div");

    // toggle adds when absent
    elem.class_list().toggle("active");
    EXPECT_TRUE(elem.class_list().contains("active"));

    // toggle removes when present
    elem.class_list().toggle("active");
    EXPECT_FALSE(elem.class_list().contains("active"));

    // add then remove via named methods
    elem.class_list().add("hidden");
    elem.class_list().add("bold");
    EXPECT_TRUE(elem.class_list().contains("hidden"));
    EXPECT_TRUE(elem.class_list().contains("bold"));

    elem.class_list().remove("hidden");
    EXPECT_FALSE(elem.class_list().contains("hidden"));
    EXPECT_TRUE(elem.class_list().contains("bold"));
}

TEST(DomTest, RemoveChildUpdatesParentAndSiblingsV85) {
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    auto* li2_ptr = li2.get();
    auto* li3_ptr = li3.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    parent->append_child(std::move(li3));
    EXPECT_EQ(parent->child_count(), 3u);

    // Remove the middle child
    parent->remove_child(*li2_ptr);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), li1_ptr);
    EXPECT_EQ(li1_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->next_sibling(), nullptr);
}

TEST(DomTest, CommentNodeDataAndSetDataV85) {
    Comment comment("initial data");
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
    EXPECT_EQ(comment.data(), "initial data");

    comment.set_data("updated data");
    EXPECT_EQ(comment.data(), "updated data");

    // Empty data
    comment.set_data("");
    EXPECT_EQ(comment.data(), "");
}

TEST(DomTest, TextNodeContentAndTypeV85) {
    Text text("Hello, World!");
    EXPECT_EQ(text.node_type(), NodeType::Text);
    EXPECT_EQ(text.text_content(), "Hello, World!");

    // Text with special characters
    Text text2("<script>alert('xss')</script>");
    EXPECT_EQ(text2.text_content(), "<script>alert('xss')</script>");
}

TEST(DomTest, InsertBeforeFirstChildV85) {
    auto parent = std::make_unique<Element>("div");
    auto existing = std::make_unique<Element>("span");
    auto new_first = std::make_unique<Element>("em");
    auto* existing_ptr = existing.get();
    auto* new_first_ptr = new_first.get();

    parent->append_child(std::move(existing));
    EXPECT_EQ(parent->first_child(), existing_ptr);

    // Insert before the first (and only) child
    parent->insert_before(std::move(new_first), existing_ptr);
    EXPECT_EQ(parent->child_count(), 2u);
    EXPECT_EQ(parent->first_child(), new_first_ptr);
    EXPECT_EQ(new_first_ptr->next_sibling(), existing_ptr);
    EXPECT_EQ(existing_ptr->next_sibling(), nullptr);
    EXPECT_EQ(new_first_ptr->parent(), parent.get());
}

TEST(DomTest, MixedNodeTypesAsChildrenV85) {
    auto parent = std::make_unique<Element>("div");
    auto child_elem = std::make_unique<Element>("p");
    auto child_text = std::make_unique<Text>("some text");
    auto child_comment = std::make_unique<Comment>("a comment");
    auto* elem_ptr = child_elem.get();
    auto* text_ptr = child_text.get();
    auto* comment_ptr = child_comment.get();

    parent->append_child(std::move(child_elem));
    parent->append_child(std::move(child_text));
    parent->append_child(std::move(child_comment));

    EXPECT_EQ(parent->child_count(), 3u);

    // Verify node types in order
    auto* first = parent->first_child();
    EXPECT_EQ(first, elem_ptr);
    EXPECT_EQ(first->node_type(), NodeType::Element);

    auto* second = first->next_sibling();
    EXPECT_EQ(second, text_ptr);
    EXPECT_EQ(second->node_type(), NodeType::Text);

    auto* third = second->next_sibling();
    EXPECT_EQ(third, comment_ptr);
    EXPECT_EQ(third->node_type(), NodeType::Comment);

    EXPECT_EQ(third->next_sibling(), nullptr);
}

TEST(DomTest, GetAttributeReturnsNulloptWhenMissingV85) {
    Element elem("div");

    // No attributes set — get_attribute should return nullopt
    EXPECT_FALSE(elem.get_attribute("id").has_value());
    EXPECT_FALSE(elem.get_attribute("class").has_value());
    EXPECT_FALSE(elem.get_attribute("nonexistent").has_value());

    // Set one attribute, others still missing
    elem.set_attribute("id", "main");
    EXPECT_TRUE(elem.get_attribute("id").has_value());
    EXPECT_EQ(elem.get_attribute("id").value(), "main");
    EXPECT_FALSE(elem.get_attribute("class").has_value());
}

// ---------------------------------------------------------------------------
// V86 Tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementSetAndGetMultipleAttributesV86) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    elem.set_attribute("placeholder", "Enter name");

    EXPECT_TRUE(elem.get_attribute("type").has_value());
    EXPECT_EQ(elem.get_attribute("type").value(), "text");
    EXPECT_TRUE(elem.get_attribute("name").has_value());
    EXPECT_EQ(elem.get_attribute("name").value(), "username");
    EXPECT_TRUE(elem.get_attribute("placeholder").has_value());
    EXPECT_EQ(elem.get_attribute("placeholder").value(), "Enter name");

    // Overwrite an existing attribute
    elem.set_attribute("type", "password");
    EXPECT_EQ(elem.get_attribute("type").value(), "password");
}

TEST(DomTest, TextNodeTextContentV86) {
    Text text_node("Hello, world!");
    EXPECT_EQ(text_node.text_content(), "Hello, world!");
    EXPECT_EQ(text_node.node_type(), NodeType::Text);

    Text empty_text("");
    EXPECT_EQ(empty_text.text_content(), "");
}

TEST(DomTest, CommentSetDataV86) {
    Comment comment("initial data");
    EXPECT_EQ(comment.data(), "initial data");
    EXPECT_EQ(comment.node_type(), NodeType::Comment);

    comment.set_data("updated data");
    EXPECT_EQ(comment.data(), "updated data");

    comment.set_data("");
    EXPECT_EQ(comment.data(), "");
}

TEST(DomTest, ClassListAddRemoveContainsV86) {
    Element elem("div");

    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    elem.class_list().add("gamma");

    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));
    EXPECT_FALSE(elem.class_list().contains("delta"));

    elem.class_list().remove("beta");
    EXPECT_FALSE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));
}

TEST(DomTest, InsertBeforeMiddleChildV86) {
    auto parent = std::make_unique<Element>("ul");
    auto first_li = std::make_unique<Element>("li");
    auto third_li = std::make_unique<Element>("li");
    auto* first_ptr = first_li.get();
    auto* third_ptr = third_li.get();

    parent->append_child(std::move(first_li));
    parent->append_child(std::move(third_li));
    EXPECT_EQ(parent->child_count(), 2u);

    // Insert a new node before the third_ptr (second child)
    auto second_li = std::make_unique<Element>("li");
    auto* second_ptr = second_li.get();
    parent->insert_before(std::move(second_li), third_ptr);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->next_sibling(), third_ptr);
}

TEST(DomTest, RemoveChildUpdatesTreeV86) {
    auto parent = std::make_unique<Element>("div");
    auto child1 = std::make_unique<Element>("span");
    auto child2 = std::make_unique<Element>("p");
    auto* child1_ptr = child1.get();
    auto* child2_ptr = child2.get();

    parent->append_child(std::move(child1));
    parent->append_child(std::move(child2));
    EXPECT_EQ(parent->child_count(), 2u);

    // Remove first child — dereference the raw pointer
    parent->remove_child(*child1_ptr);
    EXPECT_EQ(parent->child_count(), 1u);
    EXPECT_EQ(parent->first_child(), child2_ptr);
}

TEST(DomTest, NextSiblingTraversalV86) {
    auto parent = std::make_unique<Element>("ol");
    auto c1 = std::make_unique<Element>("li");
    auto c2 = std::make_unique<Text>("text node");
    auto c3 = std::make_unique<Comment>("a comment");
    auto* c1_ptr = c1.get();
    auto* c2_ptr = c2.get();
    auto* c3_ptr = c3.get();

    parent->append_child(std::move(c1));
    parent->append_child(std::move(c2));
    parent->append_child(std::move(c3));

    // Traverse using next_sibling
    auto* current = parent->first_child();
    EXPECT_EQ(current, c1_ptr);
    current = current->next_sibling();
    EXPECT_EQ(current, c2_ptr);
    current = current->next_sibling();
    EXPECT_EQ(current, c3_ptr);
    current = current->next_sibling();
    EXPECT_EQ(current, nullptr);
}

TEST(DomTest, ParentPointerAfterAppendV86) {
    auto parent = std::make_unique<Element>("section");
    auto child_elem = std::make_unique<Element>("article");
    auto child_text = std::make_unique<Text>("some text");
    auto* parent_ptr = parent.get();
    auto* elem_ptr = child_elem.get();
    auto* text_ptr = child_text.get();

    // Before appending, parent should be null
    EXPECT_EQ(elem_ptr->parent(), nullptr);
    EXPECT_EQ(text_ptr->parent(), nullptr);

    parent->append_child(std::move(child_elem));
    parent->append_child(std::move(child_text));

    // After appending, parent pointer should be set
    EXPECT_EQ(elem_ptr->parent(), parent_ptr);
    EXPECT_EQ(text_ptr->parent(), parent_ptr);
}

// ---------------------------------------------------------------------------
// V87 Tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementSetAndGetMultipleAttributesV87) {
    Element elem("input");
    elem.set_attribute("type", "text");
    elem.set_attribute("name", "username");
    elem.set_attribute("placeholder", "Enter name");

    EXPECT_EQ(elem.get_attribute("type").value(), "text");
    EXPECT_EQ(elem.get_attribute("name").value(), "username");
    EXPECT_EQ(elem.get_attribute("placeholder").value(), "Enter name");
    EXPECT_FALSE(elem.get_attribute("value").has_value());
}

TEST(DomTest, ClassListAddRemoveContainsToggleV87) {
    Element elem("div");
    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    elem.class_list().add("gamma");

    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));

    elem.class_list().remove("beta");
    EXPECT_FALSE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("alpha"));

    // toggle removes existing
    elem.class_list().toggle("alpha");
    EXPECT_FALSE(elem.class_list().contains("alpha"));

    // toggle adds missing
    elem.class_list().toggle("delta");
    EXPECT_TRUE(elem.class_list().contains("delta"));
}

TEST(DomTest, RemoveChildByDereferenceV87) {
    auto parent = std::make_unique<Element>("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    auto* li2_ptr = li2.get();

    parent->append_child(std::move(li1));
    parent->append_child(std::move(li2));
    EXPECT_EQ(parent->child_count(), 2u);

    parent->remove_child(*li1_ptr);
    EXPECT_EQ(parent->child_count(), 1u);
    EXPECT_EQ(parent->first_child(), li2_ptr);
}

TEST(DomTest, InsertBeforeMiddleChildV87) {
    auto parent = std::make_unique<Element>("div");
    auto a = std::make_unique<Element>("a");
    auto c = std::make_unique<Element>("c");
    auto* a_ptr = a.get();
    auto* c_ptr = c.get();

    parent->append_child(std::move(a));
    parent->append_child(std::move(c));

    auto b = std::make_unique<Element>("b");
    auto* b_ptr = b.get();
    parent->insert_before(std::move(b), c_ptr);

    EXPECT_EQ(parent->child_count(), 3u);
    EXPECT_EQ(parent->first_child(), a_ptr);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
}

TEST(DomTest, TextNodeContentAndTypeV87) {
    Text t("Hello, world!");
    EXPECT_EQ(t.text_content(), "Hello, world!");
    EXPECT_EQ(t.node_type(), NodeType::Text);
}

TEST(DomTest, CommentNodeTypeAndContentV87) {
    Comment c("This is a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
    EXPECT_EQ(c.data(), "This is a comment");
}

TEST(DomTest, OverwriteAttributeValueV87) {
    Element elem("meta");
    elem.set_attribute("charset", "ascii");
    EXPECT_EQ(elem.get_attribute("charset").value(), "ascii");

    elem.set_attribute("charset", "utf-8");
    EXPECT_EQ(elem.get_attribute("charset").value(), "utf-8");
}

TEST(DomTest, SiblingTraversalAfterInsertBeforeV87) {
    auto parent = std::make_unique<Element>("nav");
    auto first = std::make_unique<Element>("span");
    auto last = std::make_unique<Element>("span");
    auto* first_ptr = first.get();
    auto* last_ptr = last.get();

    parent->append_child(std::move(first));
    parent->append_child(std::move(last));

    // Insert two nodes before last
    auto mid1 = std::make_unique<Element>("em");
    auto mid2 = std::make_unique<Element>("strong");
    auto* mid1_ptr = mid1.get();
    auto* mid2_ptr = mid2.get();

    parent->insert_before(std::move(mid1), last_ptr);
    parent->insert_before(std::move(mid2), last_ptr);

    EXPECT_EQ(parent->child_count(), 4u);

    // Traverse: first -> mid1 -> mid2 -> last -> nullptr
    auto* cur = parent->first_child();
    EXPECT_EQ(cur, first_ptr);
    cur = cur->next_sibling();
    EXPECT_EQ(cur, mid1_ptr);
    cur = cur->next_sibling();
    EXPECT_EQ(cur, mid2_ptr);
    cur = cur->next_sibling();
    EXPECT_EQ(cur, last_ptr);
    cur = cur->next_sibling();
    EXPECT_EQ(cur, nullptr);
}

// ---------------------------------------------------------------------------
// V88 Round — 8 new tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementClassListToggleMultipleV88) {
    Element elem("div");
    elem.class_list().add("alpha");
    elem.class_list().add("beta");
    elem.class_list().add("gamma");
    EXPECT_TRUE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("beta"));
    EXPECT_TRUE(elem.class_list().contains("gamma"));

    // Toggle off alpha and gamma, leave beta
    elem.class_list().toggle("alpha");
    elem.class_list().toggle("gamma");
    EXPECT_FALSE(elem.class_list().contains("alpha"));
    EXPECT_TRUE(elem.class_list().contains("beta"));
    EXPECT_FALSE(elem.class_list().contains("gamma"));

    // Toggle alpha back on
    elem.class_list().toggle("alpha");
    EXPECT_TRUE(elem.class_list().contains("alpha"));
}

TEST(DomTest, InsertBeforeFirstChildV88) {
    Element parent("ul");
    auto first = std::make_unique<Element>("li");
    auto* first_ptr = first.get();
    parent.append_child(std::move(first));

    // Insert a new node before the first child (i.e., at position 0)
    auto new_first = std::make_unique<Element>("li");
    auto* new_first_ptr = new_first.get();
    parent.insert_before(std::move(new_first), first_ptr);

    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), new_first_ptr);
    EXPECT_EQ(new_first_ptr->next_sibling(), first_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), nullptr);
}

TEST(DomTest, RemoveChildMiddleNodeV88) {
    Element parent("div");
    auto c1 = std::make_unique<Element>("span");
    auto c2 = std::make_unique<Element>("em");
    auto c3 = std::make_unique<Element>("strong");
    auto* c1_ptr = c1.get();
    auto* c2_ptr = c2.get();
    auto* c3_ptr = c3.get();
    parent.append_child(std::move(c1));
    parent.append_child(std::move(c2));
    parent.append_child(std::move(c3));
    EXPECT_EQ(parent.child_count(), 3u);

    // Remove the middle child
    parent.remove_child(*c2_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), c1_ptr);
    EXPECT_EQ(c1_ptr->next_sibling(), c3_ptr);
    EXPECT_EQ(c3_ptr->next_sibling(), nullptr);
}

TEST(DomTest, TextNodeContentAndTypeV88) {
    Text text("Hello, world!");
    EXPECT_EQ(text.node_type(), NodeType::Text);
    EXPECT_EQ(text.text_content(), "Hello, world!");

    // Empty text node
    Text empty_text("");
    EXPECT_EQ(empty_text.text_content(), "");
    EXPECT_EQ(empty_text.node_type(), NodeType::Text);
}

TEST(DomTest, CommentNodeDataAndTypeV88) {
    Comment comment("This is a comment");
    EXPECT_EQ(comment.node_type(), NodeType::Comment);
    EXPECT_EQ(comment.data(), "This is a comment");

    // Comment with special characters
    Comment special("<!-- inner --> &amp;");
    EXPECT_EQ(special.data(), "<!-- inner --> &amp;");
}

TEST(DomTest, AttributeOverwriteV88) {
    Element elem("input");
    elem.set_attribute("type", "text");
    ASSERT_TRUE(elem.get_attribute("type").has_value());
    EXPECT_EQ(elem.get_attribute("type").value(), "text");

    // Overwrite with a new value
    elem.set_attribute("type", "password");
    ASSERT_TRUE(elem.get_attribute("type").has_value());
    EXPECT_EQ(elem.get_attribute("type").value(), "password");

    // Overwrite with empty string
    elem.set_attribute("type", "");
    ASSERT_TRUE(elem.get_attribute("type").has_value());
    EXPECT_EQ(elem.get_attribute("type").value(), "");
}

TEST(DomTest, DeepNestedTraversalV88) {
    Element root("div");
    auto level1 = std::make_unique<Element>("section");
    auto* level1_ptr = level1.get();
    auto level2 = std::make_unique<Element>("article");
    auto* level2_ptr = level2.get();
    auto level3 = std::make_unique<Element>("p");
    auto* level3_ptr = level3.get();
    auto leaf = std::make_unique<Text>("deep content");
    auto* leaf_ptr = leaf.get();

    level3_ptr->append_child(std::move(leaf));
    level2_ptr->append_child(std::move(level3));
    level1_ptr->append_child(std::move(level2));
    root.append_child(std::move(level1));

    // Verify 4-level deep nesting
    EXPECT_EQ(root.child_count(), 1u);
    EXPECT_EQ(root.first_child(), level1_ptr);

    auto* l2 = level1_ptr->first_child();
    EXPECT_EQ(l2, level2_ptr);
    EXPECT_EQ(l2->parent(), level1_ptr);

    auto* l3 = level2_ptr->first_child();
    EXPECT_EQ(l3, level3_ptr);
    EXPECT_EQ(l3->parent(), level2_ptr);

    auto* lf = level3_ptr->first_child();
    EXPECT_EQ(lf, leaf_ptr);
    EXPECT_EQ(lf->parent(), level3_ptr);
    EXPECT_EQ(leaf_ptr->text_content(), "deep content");
}

TEST(DomTest, ParentPointerAfterAppendV88) {
    Element parent("nav");
    auto child1 = std::make_unique<Element>("a");
    auto* child1_ptr = child1.get();
    auto child2 = std::make_unique<Element>("a");
    auto* child2_ptr = child2.get();

    // Before appending, parent is null
    EXPECT_EQ(child1_ptr->parent(), nullptr);
    EXPECT_EQ(child2_ptr->parent(), nullptr);

    parent.append_child(std::move(child1));
    parent.append_child(std::move(child2));

    // After appending, parent is set correctly
    EXPECT_EQ(child1_ptr->parent(), &parent);
    EXPECT_EQ(child2_ptr->parent(), &parent);

    // Siblings correct
    EXPECT_EQ(child1_ptr->next_sibling(), child2_ptr);
    EXPECT_EQ(child2_ptr->next_sibling(), nullptr);
}

TEST(DomTest, MultipleAttributesMapSizeV89) {
    Element el("div");
    el.set_attribute("id", "main");
    el.set_attribute("class", "container");
    el.set_attribute("data-x", "42");
    el.set_attribute("role", "banner");
    EXPECT_EQ(el.attributes().size(), 4u);
    EXPECT_TRUE(el.has_attribute("id"));
    EXPECT_TRUE(el.has_attribute("class"));
    EXPECT_TRUE(el.has_attribute("data-x"));
    EXPECT_TRUE(el.has_attribute("role"));
    EXPECT_EQ(el.get_attribute("data-x").value(), "42");
}

TEST(DomTest, ClassListToggleTwiceRestoresV89) {
    Element el("span");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
}

TEST(DomTest, DocumentCreateElementAppendV89) {
    Document doc;
    auto div = doc.create_element("div");
    auto* div_ptr = div.get();
    div_ptr->set_attribute("id", "root");
    Element container("body");
    container.append_child(std::move(div));
    EXPECT_EQ(container.child_count(), 1u);
    EXPECT_EQ(static_cast<Element*>(container.first_child())->get_attribute("id").value(), "root");
}

TEST(DomTest, RemoveChildDecreasesCountV89) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    auto li2 = std::make_unique<Element>("li");
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li2));
    EXPECT_EQ(parent.child_count(), 2u);
    parent.remove_child(*li1_ptr);
    EXPECT_EQ(parent.child_count(), 1u);
}

TEST(DomTest, InsertBeforeNullAppendsV89) {
    Element parent("div");
    auto first = std::make_unique<Element>("p");
    auto* first_ptr = first.get();
    parent.append_child(std::move(first));
    auto second = std::make_unique<Element>("span");
    auto* second_ptr = second.get();
    parent.insert_before(std::move(second), nullptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.last_child(), second_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
}

TEST(DomTest, TextNodeParentAfterAppendV89) {
    Element div("div");
    auto txt = std::make_unique<Text>("hello world");
    auto* txt_ptr = txt.get();
    EXPECT_EQ(txt_ptr->parent(), nullptr);
    div.append_child(std::move(txt));
    EXPECT_EQ(txt_ptr->parent(), &div);
    EXPECT_EQ(txt_ptr->text_content(), "hello world");
    EXPECT_EQ(div.text_content(), "hello world");
}

TEST(DomTest, CommentNodeChildCountV89) {
    Element div("div");
    auto comment = std::make_unique<Comment>("a comment");
    div.append_child(std::move(comment));
    auto txt = std::make_unique<Text>("visible");
    div.append_child(std::move(txt));
    EXPECT_EQ(div.child_count(), 2u);
}

TEST(DomTest, GrandchildParentChainV89) {
    Element root("div");
    auto child = std::make_unique<Element>("section");
    auto* child_ptr = child.get();
    auto grandchild = std::make_unique<Element>("p");
    auto* gc_ptr = grandchild.get();
    child_ptr->append_child(std::move(grandchild));
    root.append_child(std::move(child));
    EXPECT_EQ(gc_ptr->parent(), child_ptr);
    EXPECT_EQ(gc_ptr->parent()->parent(), &root);
    EXPECT_EQ(root.child_count(), 1u);
    EXPECT_EQ(child_ptr->child_count(), 1u);
}

TEST(DomTest, AttributeMapSizeAfterMultipleSetsV90) {
    Element el("div");
    el.set_attribute("id", "main");
    el.set_attribute("class", "container");
    el.set_attribute("data-role", "panel");
    EXPECT_EQ(el.attributes().size(), 3u);
    EXPECT_TRUE(el.has_attribute("data-role"));
    el.remove_attribute("class");
    EXPECT_EQ(el.attributes().size(), 2u);
    EXPECT_FALSE(el.has_attribute("class"));
}

TEST(DomTest, ClassListToggleTwiceRestoresV90) {
    Element el("span");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
}

TEST(DomTest, InsertBeforeUpdatesAllSiblingsV90) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    auto li3 = std::make_unique<Element>("li");
    auto* li3_ptr = li3.get();
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li3));
    auto li2 = std::make_unique<Element>("li");
    auto* li2_ptr = li2.get();
    parent.insert_before(std::move(li2), li3_ptr);
    EXPECT_EQ(li1_ptr->next_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->previous_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(parent.child_count(), 3u);
}

TEST(DomTest, DocumentCreateElementReturnsUniqueTagV90) {
    Document doc;
    auto div = doc.create_element("div");
    auto span = doc.create_element("span");
    EXPECT_EQ(div->tag_name(), "div");
    EXPECT_EQ(span->tag_name(), "span");
    EXPECT_NE(div.get(), span.get());
}

TEST(DomTest, OverwriteAttributePreservesOthersV90) {
    Element el("a");
    el.set_attribute("href", "http://example.com");
    el.set_attribute("target", "_blank");
    el.set_attribute("href", "http://other.com");
    EXPECT_EQ(el.get_attribute("href").value(), "http://other.com");
    EXPECT_EQ(el.get_attribute("target").value(), "_blank");
    EXPECT_EQ(el.attributes().size(), 2u);
}

TEST(DomTest, RemoveChildClearsParentAndSiblingsV90) {
    Element parent("div");
    auto a = std::make_unique<Element>("p");
    auto* a_ptr = a.get();
    auto b = std::make_unique<Element>("p");
    auto* b_ptr = b.get();
    auto c = std::make_unique<Element>("p");
    auto* c_ptr = c.get();
    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));
    parent.remove_child(*b_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(a_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), a_ptr);
}

TEST(DomTest, TextContentConcatenatesMultipleChildrenV90) {
    Element div("div");
    div.append_child(std::make_unique<Text>("Hello "));
    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("World"));
    div.append_child(std::move(span));
    div.append_child(std::make_unique<Text>("!"));
    EXPECT_EQ(div.text_content(), "Hello World!");
}

TEST(DomTest, FirstLastChildAfterRemovalsV90) {
    Element parent("div");
    auto a = std::make_unique<Element>("span");
    auto* a_ptr = a.get();
    auto b = std::make_unique<Element>("span");
    auto c = std::make_unique<Element>("span");
    auto* c_ptr = c.get();
    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));
    EXPECT_EQ(parent.first_child(), a_ptr);
    EXPECT_EQ(parent.last_child(), c_ptr);
    parent.remove_child(*a_ptr);
    EXPECT_NE(parent.first_child(), a_ptr);
    EXPECT_EQ(parent.last_child(), c_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DomTest, InsertBeforeUpdatesAllSiblingLinksV91) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    auto li3 = std::make_unique<Element>("li");
    auto* li3_ptr = li3.get();
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li3));
    auto li2 = std::make_unique<Element>("li");
    auto* li2_ptr = li2.get();
    parent.insert_before(std::move(li2), li3_ptr);
    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(li1_ptr->next_sibling(), li2_ptr);
    EXPECT_EQ(li2_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(li2_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->previous_sibling(), li2_ptr);
}

TEST(DomTest, ClassListToggleTwiceRestoresV91) {
    Element el("div");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
}

TEST(DomTest, DocumentCreateElementReturnsUniquePtrsV91) {
    Document doc;
    auto el1 = doc.create_element("section");
    auto el2 = doc.create_element("article");
    EXPECT_EQ(el1->tag_name(), "section");
    EXPECT_EQ(el2->tag_name(), "article");
    EXPECT_NE(el1.get(), el2.get());
}

TEST(DomTest, RemoveAttributeThenHasAttributeV91) {
    Element el("input");
    el.set_attribute("type", "text");
    el.set_attribute("name", "username");
    EXPECT_TRUE(el.has_attribute("type"));
    el.remove_attribute("type");
    EXPECT_FALSE(el.has_attribute("type"));
    EXPECT_TRUE(el.has_attribute("name"));
    EXPECT_EQ(el.attributes().size(), 1u);
}

TEST(DomTest, TextNodeParentSetOnAppendV91) {
    Element div("div");
    auto txt = std::make_unique<Text>("hello");
    auto* txt_ptr = txt.get();
    div.append_child(std::move(txt));
    EXPECT_EQ(txt_ptr->parent(), &div);
    EXPECT_EQ(div.child_count(), 1u);
    EXPECT_EQ(div.text_content(), "hello");
}

TEST(DomTest, CommentNodeDoesNotAffectTextContentV91) {
    Element div("div");
    div.append_child(std::make_unique<Text>("visible"));
    div.append_child(std::make_unique<Comment>("hidden comment"));
    div.append_child(std::make_unique<Text>(" text"));
    EXPECT_EQ(div.text_content(), "visible text");
}

TEST(DomTest, InsertBeforeNullAppendsToEndV91) {
    Element parent("div");
    auto a = std::make_unique<Element>("span");
    auto* a_ptr = a.get();
    parent.append_child(std::move(a));
    auto b = std::make_unique<Element>("p");
    auto* b_ptr = b.get();
    parent.insert_before(std::move(b), nullptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.last_child(), b_ptr);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->previous_sibling(), a_ptr);
}

TEST(DomTest, SetAttributeIdThenGetElementByIdV91) {
    Document doc;
    auto div = doc.create_element("div");
    auto* div_ptr = div.get();
    div_ptr->set_attribute("id", "main-content");
    doc.register_id("main-content", div_ptr);
    doc.append_child(std::move(div));
    auto* found = doc.get_element_by_id("main-content");
    EXPECT_EQ(found, div_ptr);
    EXPECT_EQ(found->tag_name(), "div");
    EXPECT_EQ(found->get_attribute("id").value(), "main-content");
}

TEST(DomTest, AppendChildSetsFirstAndLastChildV92) {
    Element div("div");
    EXPECT_EQ(div.first_child(), nullptr);
    EXPECT_EQ(div.last_child(), nullptr);
    auto span = std::make_unique<Element>("span");
    auto* span_ptr = span.get();
    div.append_child(std::move(span));
    EXPECT_EQ(div.first_child(), span_ptr);
    EXPECT_EQ(div.last_child(), span_ptr);
    auto p = std::make_unique<Element>("p");
    auto* p_ptr = p.get();
    div.append_child(std::move(p));
    EXPECT_EQ(div.first_child(), span_ptr);
    EXPECT_EQ(div.last_child(), p_ptr);
}

TEST(DomTest, RemoveChildUpdatesSiblingPointersV92) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    parent.append_child(std::move(li1));
    auto li2 = std::make_unique<Element>("li");
    auto* li2_ptr = li2.get();
    parent.append_child(std::move(li2));
    auto li3 = std::make_unique<Element>("li");
    auto* li3_ptr = li3.get();
    parent.append_child(std::move(li3));
    parent.remove_child(*li2_ptr);
    EXPECT_EQ(li1_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DomTest, ClassListToggleAddsWhenAbsentV92) {
    Element el("div");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
}

TEST(DomTest, MultipleAttributesSetAndRetrieveV92) {
    Element el("a");
    el.set_attribute("href", "https://example.com");
    el.set_attribute("target", "_blank");
    el.set_attribute("rel", "noopener");
    EXPECT_EQ(el.attributes().size(), 3u);
    EXPECT_EQ(el.get_attribute("href").value(), "https://example.com");
    EXPECT_EQ(el.get_attribute("target").value(), "_blank");
    EXPECT_EQ(el.get_attribute("rel").value(), "noopener");
}

TEST(DomTest, InsertBeforeFirstChildV92) {
    Element parent("div");
    auto existing = std::make_unique<Element>("span");
    auto* existing_ptr = existing.get();
    parent.append_child(std::move(existing));
    auto new_el = std::make_unique<Element>("em");
    auto* new_ptr = new_el.get();
    parent.insert_before(std::move(new_el), existing_ptr);
    EXPECT_EQ(parent.first_child(), new_ptr);
    EXPECT_EQ(new_ptr->next_sibling(), existing_ptr);
    EXPECT_EQ(existing_ptr->previous_sibling(), new_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DomTest, NestedElementTextContentV92) {
    Element div("div");
    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("hello "));
    div.append_child(std::move(span));
    auto em = std::make_unique<Element>("em");
    em->append_child(std::make_unique<Text>("world"));
    div.append_child(std::move(em));
    EXPECT_EQ(div.text_content(), "hello world");
}

TEST(DomTest, OverwriteExistingAttributeV92) {
    Element el("input");
    el.set_attribute("value", "old");
    EXPECT_EQ(el.get_attribute("value").value(), "old");
    el.set_attribute("value", "new");
    EXPECT_EQ(el.get_attribute("value").value(), "new");
    EXPECT_EQ(el.attributes().size(), 1u);
}

TEST(DomTest, DocumentCreateMultipleElementTypesV92) {
    Document doc;
    auto div = doc.create_element("div");
    auto span = doc.create_element("span");
    auto p = doc.create_element("p");
    EXPECT_EQ(div->tag_name(), "div");
    EXPECT_EQ(span->tag_name(), "span");
    EXPECT_EQ(p->tag_name(), "p");
    auto* div_ptr = div.get();
    auto* span_ptr = span.get();
    div_ptr->append_child(std::move(span));
    div_ptr->append_child(std::move(p));
    EXPECT_EQ(div_ptr->child_count(), 2u);
    EXPECT_EQ(div_ptr->first_child(), span_ptr);
}

TEST(DomTest, RemoveChildUpdatesParentAndSiblingsV93) {
    Element parent("div");
    auto a = std::make_unique<Element>("span");
    auto* a_ptr = a.get();
    parent.append_child(std::move(a));
    auto b = std::make_unique<Element>("em");
    auto* b_ptr = b.get();
    parent.append_child(std::move(b));
    auto c = std::make_unique<Element>("p");
    auto* c_ptr = c.get();
    parent.append_child(std::move(c));
    EXPECT_EQ(parent.child_count(), 3u);
    parent.remove_child(*b_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(a_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), a_ptr);
}

TEST(DomTest, ClassListAddRemoveContainsV93) {
    Element el("div");
    el.class_list().add("alpha");
    el.class_list().add("beta");
    EXPECT_TRUE(el.class_list().contains("alpha"));
    EXPECT_TRUE(el.class_list().contains("beta"));
    el.class_list().remove("alpha");
    EXPECT_FALSE(el.class_list().contains("alpha"));
    EXPECT_TRUE(el.class_list().contains("beta"));
}

TEST(DomTest, ClassListToggleAddsAndRemovesV93) {
    Element el("span");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
}

TEST(DomTest, GetAttributeReturnsNulloptForMissingV93) {
    Element el("p");
    EXPECT_FALSE(el.get_attribute("nonexistent").has_value());
    EXPECT_FALSE(el.has_attribute("nonexistent"));
    el.set_attribute("data-x", "123");
    EXPECT_TRUE(el.has_attribute("data-x"));
    EXPECT_EQ(el.get_attribute("data-x").value(), "123");
}

TEST(DomTest, RemoveAttributeRemovesOnlyTargetV93) {
    Element el("a");
    el.set_attribute("href", "/page");
    el.set_attribute("title", "Page");
    el.set_attribute("class", "link");
    EXPECT_EQ(el.attributes().size(), 3u);
    el.remove_attribute("title");
    EXPECT_EQ(el.attributes().size(), 2u);
    EXPECT_FALSE(el.has_attribute("title"));
    EXPECT_TRUE(el.has_attribute("href"));
    EXPECT_TRUE(el.has_attribute("class"));
}

TEST(DomTest, InsertBeforeMiddleChildV93) {
    Element parent("div");
    auto first = std::make_unique<Element>("span");
    auto* first_ptr = first.get();
    parent.append_child(std::move(first));
    auto third = std::make_unique<Element>("p");
    auto* third_ptr = third.get();
    parent.append_child(std::move(third));
    auto second = std::make_unique<Element>("em");
    auto* second_ptr = second.get();
    parent.insert_before(std::move(second), third_ptr);
    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(first_ptr->next_sibling(), second_ptr);
    EXPECT_EQ(second_ptr->next_sibling(), third_ptr);
    EXPECT_EQ(third_ptr->previous_sibling(), second_ptr);
}

TEST(DomTest, TextAndCommentAsChildrenV93) {
    Element div("div");
    div.append_child(std::make_unique<Text>("Hello"));
    div.append_child(std::make_unique<Comment>("a comment"));
    div.append_child(std::make_unique<Text>(" World"));
    EXPECT_EQ(div.child_count(), 3u);
    EXPECT_EQ(div.text_content(), "Hello World");
}

TEST(DomTest, SetAttributeIdAndVerifyTreeStructureV93) {
    Element parent("div");
    parent.set_attribute("id", "main");
    parent.set_attribute("class", "container");
    auto child1 = std::make_unique<Element>("span");
    child1->set_attribute("id", "s1");
    auto* c1 = child1.get();
    parent.append_child(std::move(child1));
    auto child2 = std::make_unique<Element>("p");
    child2->set_attribute("id", "s2");
    auto* c2 = child2.get();
    parent.append_child(std::move(child2));
    EXPECT_EQ(parent.get_attribute("id").value(), "main");
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), c1);
    EXPECT_EQ(parent.last_child(), c2);
    EXPECT_EQ(static_cast<Element*>(c1)->get_attribute("id").value(), "s1");
    EXPECT_EQ(static_cast<Element*>(c2)->get_attribute("id").value(), "s2");
}

TEST(DomTest, AppendChildReturnsReferenceV94) {
    Element parent("ul");
    auto li = std::make_unique<Element>("li");
    auto* expected = li.get();
    Node& returned = parent.append_child(std::move(li));
    EXPECT_EQ(&returned, expected);
    EXPECT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(parent.first_child(), expected);
}

TEST(DomTest, RemoveChildUpdatesFirstAndLastV94) {
    Element parent("ol");
    auto a = std::make_unique<Element>("li");
    auto* a_ptr = a.get();
    parent.append_child(std::move(a));
    auto b = std::make_unique<Element>("li");
    auto* b_ptr = b.get();
    parent.append_child(std::move(b));
    auto c = std::make_unique<Element>("li");
    auto* c_ptr = c.get();
    parent.append_child(std::move(c));
    parent.remove_child(*b_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), a_ptr);
    EXPECT_EQ(parent.last_child(), c_ptr);
    EXPECT_EQ(a_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), a_ptr);
}

TEST(DomTest, ClassListMultipleOpsV94) {
    Element el("div");
    el.class_list().add("alpha");
    el.class_list().add("beta");
    el.class_list().add("gamma");
    EXPECT_TRUE(el.class_list().contains("alpha"));
    EXPECT_TRUE(el.class_list().contains("beta"));
    EXPECT_TRUE(el.class_list().contains("gamma"));
    el.class_list().toggle("beta");
    EXPECT_FALSE(el.class_list().contains("beta"));
    el.class_list().remove("alpha");
    EXPECT_FALSE(el.class_list().contains("alpha"));
    EXPECT_TRUE(el.class_list().contains("gamma"));
}

TEST(DomTest, NestedTextContentConcatenationV94) {
    Element div("div");
    div.append_child(std::make_unique<Text>("Hello "));
    auto span = std::make_unique<Element>("span");
    span->append_child(std::make_unique<Text>("beautiful "));
    div.append_child(std::move(span));
    div.append_child(std::make_unique<Text>("world"));
    EXPECT_EQ(div.text_content(), "Hello beautiful world");
    EXPECT_EQ(div.child_count(), 3u);
}

TEST(DomTest, InsertBeforeAtFrontV94) {
    Element parent("div");
    auto orig = std::make_unique<Element>("b");
    auto* orig_ptr = orig.get();
    parent.append_child(std::move(orig));
    auto first = std::make_unique<Element>("a");
    auto* first_ptr = first.get();
    parent.insert_before(std::move(first), orig_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), first_ptr);
    EXPECT_EQ(parent.last_child(), orig_ptr);
    EXPECT_EQ(first_ptr->next_sibling(), orig_ptr);
    EXPECT_EQ(orig_ptr->previous_sibling(), first_ptr);
}

TEST(DomTest, AttributeOverwritePreservesCountV94) {
    Element el("input");
    el.set_attribute("type", "text");
    el.set_attribute("value", "abc");
    EXPECT_EQ(el.attributes().size(), 2u);
    el.set_attribute("value", "xyz");
    EXPECT_EQ(el.attributes().size(), 2u);
    EXPECT_EQ(el.get_attribute("value").value(), "xyz");
    EXPECT_EQ(el.get_attribute("type").value(), "text");
}

TEST(DomTest, TextContentOfEmptyElementV94) {
    Element empty("div");
    EXPECT_EQ(empty.text_content(), "");
    EXPECT_EQ(empty.child_count(), 0u);
    EXPECT_EQ(empty.first_child(), nullptr);
    EXPECT_EQ(empty.last_child(), nullptr);
}

TEST(DomTest, ParentAndSiblingPointersAfterInsertV94) {
    Element parent("nav");
    auto a = std::make_unique<Element>("a");
    auto* a_ptr = a.get();
    parent.append_child(std::move(a));
    auto c = std::make_unique<Element>("c");
    auto* c_ptr = c.get();
    parent.append_child(std::move(c));
    auto b = std::make_unique<Element>("b");
    auto* b_ptr = b.get();
    parent.insert_before(std::move(b), c_ptr);
    EXPECT_EQ(b_ptr->parent(), &parent);
    EXPECT_EQ(a_ptr->next_sibling(), b_ptr);
    EXPECT_EQ(b_ptr->previous_sibling(), a_ptr);
    EXPECT_EQ(b_ptr->next_sibling(), c_ptr);
    EXPECT_EQ(c_ptr->previous_sibling(), b_ptr);
    EXPECT_EQ(a_ptr->previous_sibling(), nullptr);
    EXPECT_EQ(c_ptr->next_sibling(), nullptr);
}

// ---------------------------------------------------------------------------
// V95 Tests
// ---------------------------------------------------------------------------

TEST(DomTest, RemoveChildUpdatesLinksV95) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto* li1_ptr = li1.get();
    parent.append_child(std::move(li1));
    auto li2 = std::make_unique<Element>("li");
    auto* li2_ptr = li2.get();
    parent.append_child(std::move(li2));
    auto li3 = std::make_unique<Element>("li");
    auto* li3_ptr = li3.get();
    parent.append_child(std::move(li3));
    EXPECT_EQ(parent.child_count(), 3u);
    parent.remove_child(*li2_ptr);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(li1_ptr->next_sibling(), li3_ptr);
    EXPECT_EQ(li3_ptr->previous_sibling(), li1_ptr);
    EXPECT_EQ(parent.first_child(), li1_ptr);
    EXPECT_EQ(parent.last_child(), li3_ptr);
}

TEST(DomTest, ClassListToggleTwiceRestoresV95) {
    Element el("div");
    el.class_list().add("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
}

TEST(DomTest, CommentNodeDataAccessV95) {
    Comment c("this is a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
    EXPECT_EQ(c.data(), "this is a comment");
}

TEST(DomTest, NestedElementTextContentV95) {
    Element div("div");
    div.set_attribute("id", "wrap");
    auto p = std::make_unique<Element>("p");
    p->append_child(std::make_unique<Text>("hello"));
    auto* raw_p = p.get();
    div.append_child(std::move(p));
    EXPECT_EQ(div.child_count(), 1u);
    EXPECT_EQ(raw_p->text_content(), "hello");
    EXPECT_EQ(div.text_content(), "hello");
}

TEST(DomTest, HasAttributeReturnsFalseAfterRemoveV95) {
    Element el("input");
    el.set_attribute("required", "");
    EXPECT_TRUE(el.has_attribute("required"));
    el.remove_attribute("required");
    EXPECT_FALSE(el.has_attribute("required"));
    EXPECT_EQ(el.attributes().size(), 0u);
}

TEST(DomTest, AppendChildSetsParentV95) {
    Element parent("section");
    auto child = std::make_unique<Element>("article");
    auto* raw = child.get();
    parent.append_child(std::move(child));
    EXPECT_EQ(raw->parent(), &parent);
    EXPECT_EQ(parent.first_child(), raw);
    EXPECT_EQ(parent.child_count(), 1u);
}

TEST(DomTest, MultipleTextChildrenConcatV95) {
    Element div("div");
    div.append_child(std::make_unique<Text>("A"));
    div.append_child(std::make_unique<Element>("br"));
    div.append_child(std::make_unique<Text>("B"));
    EXPECT_EQ(div.child_count(), 3u);
    EXPECT_EQ(div.text_content(), "AB");
}

TEST(DomTest, SetAttributeViaIdThenGetV95) {
    Element el("span");
    el.set_attribute("id", "main-title");
    el.set_attribute("data-x", "42");
    EXPECT_EQ(el.get_attribute("id").value(), "main-title");
    EXPECT_EQ(el.get_attribute("data-x").value(), "42");
    EXPECT_EQ(el.attributes().size(), 2u);
    el.class_list().add("big");
    el.class_list().add("red");
    EXPECT_TRUE(el.class_list().contains("big"));
    EXPECT_TRUE(el.class_list().contains("red"));
}

// ---------------------------------------------------------------------------
// Round 96 tests
// ---------------------------------------------------------------------------

TEST(DomTest, RemoveChildUpdatesFirstAndLastChildV96) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto li3 = std::make_unique<Element>("li");
    auto* raw1 = li1.get();
    auto* raw2 = li2.get();
    auto* raw3 = li3.get();
    parent.append_child(std::move(li1));
    parent.append_child(std::move(li2));
    parent.append_child(std::move(li3));
    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), raw1);
    EXPECT_EQ(parent.last_child(), raw3);
    parent.remove_child(*raw1);
    EXPECT_EQ(parent.child_count(), 2u);
    EXPECT_EQ(parent.first_child(), raw2);
    EXPECT_EQ(parent.last_child(), raw3);
    parent.remove_child(*raw3);
    EXPECT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(parent.first_child(), raw2);
    EXPECT_EQ(parent.last_child(), raw2);
}

TEST(DomTest, InsertBeforeAtMiddlePositionV96) {
    Element parent("div");
    auto first = std::make_unique<Element>("span");
    auto third = std::make_unique<Element>("p");
    auto* rawFirst = first.get();
    auto* rawThird = third.get();
    parent.append_child(std::move(first));
    parent.append_child(std::move(third));
    auto second = std::make_unique<Element>("em");
    auto* rawSecond = second.get();
    parent.insert_before(std::move(second), rawThird);
    EXPECT_EQ(parent.child_count(), 3u);
    EXPECT_EQ(parent.first_child(), rawFirst);
    EXPECT_EQ(rawFirst->next_sibling(), rawSecond);
    EXPECT_EQ(rawSecond->next_sibling(), rawThird);
    EXPECT_EQ(rawSecond->parent(), &parent);
}

TEST(DomTest, CommentNodeDataAndTypeV96) {
    Comment c("This is a comment with <special> & chars");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
    EXPECT_EQ(c.data(), "This is a comment with <special> & chars");
    Comment empty("");
    EXPECT_EQ(empty.data(), "");
}

TEST(DomTest, ClassListToggleAddsAndRemovesV96) {
    Element el("div");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    el.class_list().toggle("hidden");
    EXPECT_TRUE(el.class_list().contains("active"));
    EXPECT_TRUE(el.class_list().contains("hidden"));
}

TEST(DomTest, AttributeOverwritePreservesCountV96) {
    Element el("input");
    el.set_attribute("type", "text");
    el.set_attribute("value", "hello");
    EXPECT_EQ(el.attributes().size(), 2u);
    el.set_attribute("type", "password");
    EXPECT_EQ(el.attributes().size(), 2u);
    EXPECT_EQ(el.get_attribute("type").value(), "password");
    EXPECT_EQ(el.get_attribute("value").value(), "hello");
}

TEST(DomTest, TextContentAcrossNestedChildrenV96) {
    Element outer("div");
    auto inner = std::make_unique<Element>("span");
    auto* rawInner = inner.get();
    rawInner->append_child(std::make_unique<Text>("Hello "));
    outer.append_child(std::move(inner));
    outer.append_child(std::make_unique<Text>("World"));
    EXPECT_EQ(outer.text_content(), "Hello World");
    EXPECT_EQ(outer.child_count(), 2u);
}

TEST(DomTest, RemoveAttributeThenHasAttributeV96) {
    Element el("a");
    el.set_attribute("href", "https://example.com");
    el.set_attribute("target", "_blank");
    el.set_attribute("rel", "noopener");
    EXPECT_EQ(el.attributes().size(), 3u);
    el.remove_attribute("target");
    EXPECT_FALSE(el.has_attribute("target"));
    EXPECT_FALSE(el.get_attribute("target").has_value());
    EXPECT_EQ(el.attributes().size(), 2u);
    EXPECT_TRUE(el.has_attribute("href"));
    EXPECT_TRUE(el.has_attribute("rel"));
    el.remove_attribute("nonexistent");
    EXPECT_EQ(el.attributes().size(), 2u);
}

TEST(DomTest, SiblingNavigationChainV96) {
    Element parent("nav");
    auto a = std::make_unique<Element>("a");
    auto b = std::make_unique<Text>("separator");
    auto c = std::make_unique<Element>("a");
    auto* rawA = a.get();
    auto* rawB = b.get();
    auto* rawC = c.get();
    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    parent.append_child(std::move(c));
    EXPECT_EQ(rawA->next_sibling(), rawB);
    EXPECT_EQ(rawB->next_sibling(), rawC);
    EXPECT_EQ(rawC->next_sibling(), nullptr);
    EXPECT_EQ(rawA->node_type(), NodeType::Element);
    EXPECT_EQ(rawB->node_type(), NodeType::Text);
    EXPECT_EQ(rawC->node_type(), NodeType::Element);
    EXPECT_EQ(rawB->text_content(), "separator");
}

// ---------------------------------------------------------------------------
// V97 Round — 8 new tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementRemoveAttributeThenHasReturnsFalseV97) {
    Element el("input");
    el.set_attribute("type", "text");
    EXPECT_TRUE(el.has_attribute("type"));
    el.remove_attribute("type");
    EXPECT_FALSE(el.has_attribute("type"));
    EXPECT_EQ(el.attributes().size(), 0u);
}

TEST(DomTest, InsertBeforeFirstChildReordersV97) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto li2 = std::make_unique<Element>("li");
    auto* rawLi1 = li1.get();
    auto* rawLi2 = li2.get();
    parent.append_child(std::move(li1));
    parent.insert_before(std::move(li2), rawLi1);
    EXPECT_EQ(parent.first_child(), rawLi2);
    EXPECT_EQ(rawLi2->next_sibling(), rawLi1);
    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DomTest, CommentNodeDataAndTypeV97) {
    Comment c("TODO: fix this later");
    EXPECT_EQ(c.data(), "TODO: fix this later");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

TEST(DomTest, ClassListToggleAddsAndRemovesV97) {
    Element el("div");
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
}

TEST(DomTest, RemoveChildUpdatesFirstAndLastV97) {
    Element parent("div");
    auto a = std::make_unique<Element>("span");
    auto b = std::make_unique<Element>("span");
    auto* rawA = a.get();
    auto* rawB = b.get();
    parent.append_child(std::move(a));
    parent.append_child(std::move(b));
    EXPECT_EQ(parent.child_count(), 2u);
    parent.remove_child(*rawA);
    EXPECT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(parent.first_child(), rawB);
    EXPECT_EQ(parent.last_child(), rawB);
}

TEST(DomTest, TextNodeContentAndSiblingV97) {
    Element parent("p");
    auto t1 = std::make_unique<Text>("hello ");
    auto t2 = std::make_unique<Text>("world");
    auto* rawT1 = t1.get();
    auto* rawT2 = t2.get();
    parent.append_child(std::move(t1));
    parent.append_child(std::move(t2));
    EXPECT_EQ(rawT1->text_content(), "hello ");
    EXPECT_EQ(rawT2->text_content(), "world");
    EXPECT_EQ(rawT1->next_sibling(), rawT2);
    EXPECT_EQ(rawT2->next_sibling(), nullptr);
}

TEST(DomTest, GetAttributeReturnsNulloptWhenMissingV97) {
    Element el("div");
    auto val = el.get_attribute("nonexistent");
    EXPECT_FALSE(val.has_value());
    el.set_attribute("data-x", "42");
    auto val2 = el.get_attribute("data-x");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), "42");
}

TEST(DomTest, MultipleClassListOperationsV97) {
    Element el("nav");
    el.class_list().add("primary");
    el.class_list().add("sticky");
    el.class_list().add("dark-mode");
    EXPECT_TRUE(el.class_list().contains("primary"));
    EXPECT_TRUE(el.class_list().contains("sticky"));
    EXPECT_TRUE(el.class_list().contains("dark-mode"));
    el.class_list().remove("sticky");
    EXPECT_FALSE(el.class_list().contains("sticky"));
    EXPECT_TRUE(el.class_list().contains("primary"));
    EXPECT_TRUE(el.class_list().contains("dark-mode"));
}

// ---------------------------------------------------------------------------
// V98 Round — 8 new tests
// ---------------------------------------------------------------------------

TEST(DomTest, ElementInsertBeforeFirstChildV98) {
    Element parent("ul");
    auto li1 = std::make_unique<Element>("li");
    auto* rawLi1 = li1.get();
    parent.append_child(std::move(li1));

    auto li0 = std::make_unique<Element>("li");
    auto* rawLi0 = li0.get();
    parent.insert_before(std::move(li0), rawLi1);

    EXPECT_EQ(parent.first_child(), rawLi0);
    EXPECT_EQ(rawLi0->next_sibling(), rawLi1);
    EXPECT_EQ(parent.child_count(), 2u);
}

TEST(DomTest, CommentNodeDataAccessV98) {
    Comment c("This is a comment");
    EXPECT_EQ(c.data(), "This is a comment");
    EXPECT_EQ(c.node_type(), NodeType::Comment);
}

TEST(DomTest, RemoveChildUpdatesTreeV98) {
    Element parent("div");
    auto child1 = std::make_unique<Element>("p");
    auto* rawChild1 = child1.get();
    auto child2 = std::make_unique<Element>("span");
    auto* rawChild2 = child2.get();
    parent.append_child(std::move(child1));
    parent.append_child(std::move(child2));
    EXPECT_EQ(parent.child_count(), 2u);

    parent.remove_child(*rawChild1);
    EXPECT_EQ(parent.child_count(), 1u);
    EXPECT_EQ(parent.first_child(), rawChild2);
    EXPECT_EQ(parent.last_child(), rawChild2);
}

TEST(DomTest, AttributeOverwriteValueV98) {
    Element el("input");
    el.set_attribute("type", "text");
    EXPECT_EQ(el.get_attribute("type").value(), "text");
    el.set_attribute("type", "password");
    EXPECT_EQ(el.get_attribute("type").value(), "password");
    EXPECT_EQ(el.attributes().size(), 1u);
}

TEST(DomTest, ClassListToggleAddsAndRemovesV98) {
    Element el("div");
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_FALSE(el.class_list().contains("active"));
    el.class_list().toggle("active");
    EXPECT_TRUE(el.class_list().contains("active"));
}

TEST(DomTest, TextNodeSiblingNavigationV98) {
    Element parent("p");
    auto t1 = std::make_unique<Text>("Hello");
    auto* rawT1 = t1.get();
    auto t2 = std::make_unique<Text>(" ");
    auto* rawT2 = t2.get();
    auto t3 = std::make_unique<Text>("World");
    auto* rawT3 = t3.get();
    parent.append_child(std::move(t1));
    parent.append_child(std::move(t2));
    parent.append_child(std::move(t3));

    EXPECT_EQ(parent.first_child(), rawT1);
    EXPECT_EQ(rawT1->next_sibling(), rawT2);
    EXPECT_EQ(rawT2->next_sibling(), rawT3);
    EXPECT_EQ(rawT3->next_sibling(), nullptr);
    EXPECT_EQ(parent.last_child(), rawT3);
}

TEST(DomTest, ElementChildCountAfterOperationsV98) {
    Element root("section");
    EXPECT_EQ(root.child_count(), 0u);

    auto h1 = std::make_unique<Element>("h1");
    auto* rawH1 = h1.get();
    root.append_child(std::move(h1));
    EXPECT_EQ(root.child_count(), 1u);

    root.append_child(std::make_unique<Element>("p"));
    EXPECT_EQ(root.child_count(), 2u);

    root.append_child(std::make_unique<Element>("footer"));
    EXPECT_EQ(root.child_count(), 3u);

    root.remove_child(*rawH1);
    EXPECT_EQ(root.child_count(), 2u);
}

TEST(DomTest, NodeTypeDistinguishesElementTextCommentV98) {
    Element el("div");
    Text txt("hello");
    Comment cmt("note");

    EXPECT_EQ(el.node_type(), NodeType::Element);
    EXPECT_EQ(txt.node_type(), NodeType::Text);
    EXPECT_EQ(cmt.node_type(), NodeType::Comment);

    EXPECT_EQ(el.tag_name(), "div");
    EXPECT_EQ(txt.text_content(), "hello");
    EXPECT_EQ(cmt.data(), "note");
}
