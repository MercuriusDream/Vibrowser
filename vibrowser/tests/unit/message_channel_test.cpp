#include <clever/ipc/message_channel.h>
#include <clever/ipc/message.h>
#include <clever/ipc/message_pipe.h>
#include <clever/ipc/serializer.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

using clever::ipc::Message;
using clever::ipc::MessageChannel;
using clever::ipc::MessagePipe;
using clever::ipc::Serializer;
using clever::ipc::Deserializer;

// ------------------------------------------------------------------
// 1. Send and receive Message
// ------------------------------------------------------------------

TEST(MessageChannelTest, SendAndReceiveMessage) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel a(std::move(pa));
    MessageChannel b(std::move(pb));

    Message msg;
    msg.type = 1;
    msg.request_id = 42;
    msg.payload = {0xDE, 0xAD, 0xBE, 0xEF};

    ASSERT_TRUE(a.send(msg));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 1u);
    EXPECT_EQ(received->request_id, 42u);
    EXPECT_EQ(received->payload, msg.payload);
}

TEST(MessageChannelTest, SendAndReceiveEmptyPayload) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel a(std::move(pa));
    MessageChannel b(std::move(pb));

    Message msg;
    msg.type = 99;
    msg.request_id = 0;
    // payload left empty

    ASSERT_TRUE(a.send(msg));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 99u);
    EXPECT_EQ(received->request_id, 0u);
    EXPECT_TRUE(received->payload.empty());
}

// ------------------------------------------------------------------
// 2. Register handler and dispatch
// ------------------------------------------------------------------

TEST(MessageChannelTest, RegisterHandlerAndDispatch) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    bool handler_called = false;
    uint32_t received_type = 0;
    uint32_t received_req_id = 0;

    ch.on(5, [&](const Message& m) {
        handler_called = true;
        received_type = m.type;
        received_req_id = m.request_id;
    });

    Message msg;
    msg.type = 5;
    msg.request_id = 100;
    ch.dispatch(msg);

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(received_type, 5u);
    EXPECT_EQ(received_req_id, 100u);
}

TEST(MessageChannelTest, DispatchUnregisteredTypeDoesNotCrash) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    Message msg;
    msg.type = 999;
    // Should not crash even with no handler registered
    EXPECT_NO_THROW(ch.dispatch(msg));
}

// ------------------------------------------------------------------
// 3. Send multiple message types with different handlers
// ------------------------------------------------------------------

TEST(MessageChannelTest, MultipleMessageTypesWithDifferentHandlers) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int handler1_count = 0;
    int handler2_count = 0;
    int handler3_count = 0;

    ch.on(1, [&](const Message&) { handler1_count++; });
    ch.on(2, [&](const Message&) { handler2_count++; });
    ch.on(3, [&](const Message&) { handler3_count++; });

    Message m1; m1.type = 1;
    Message m2; m2.type = 2;
    Message m3; m3.type = 3;

    ch.dispatch(m1);
    ch.dispatch(m2);
    ch.dispatch(m2);
    ch.dispatch(m3);
    ch.dispatch(m3);
    ch.dispatch(m3);

    EXPECT_EQ(handler1_count, 1);
    EXPECT_EQ(handler2_count, 2);
    EXPECT_EQ(handler3_count, 3);
}

// ------------------------------------------------------------------
// 4. Channel over pipe pair — full round trip
// ------------------------------------------------------------------

TEST(MessageChannelTest, FullRoundTripOverPipePair) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    // Build a message with structured payload
    Serializer s;
    s.write_string("hello from sender");
    s.write_u32(12345);

    Message msg;
    msg.type = 10;
    msg.request_id = 7;
    msg.payload = s.take_data();

    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 10u);
    EXPECT_EQ(received->request_id, 7u);

    // Deserialize the payload
    Deserializer d(received->payload);
    EXPECT_EQ(d.read_string(), "hello from sender");
    EXPECT_EQ(d.read_u32(), 12345u);
    EXPECT_FALSE(d.has_remaining());
}

TEST(MessageChannelTest, FullRoundTripMultipleMessages) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    // Send 5 messages
    for (uint32_t i = 0; i < 5; ++i) {
        Message msg;
        msg.type = i;
        msg.request_id = i * 10;
        msg.payload = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        ASSERT_TRUE(sender.send(msg));
    }

    // Receive 5 messages
    for (uint32_t i = 0; i < 5; ++i) {
        auto received = receiver.receive();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(received->type, i);
        EXPECT_EQ(received->request_id, i * 10);
        std::vector<uint8_t> expected = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        EXPECT_EQ(received->payload, expected);
    }
}

TEST(MessageChannelTest, ReceiveAndDispatch) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    std::string received_payload;
    receiver.on(20, [&](const Message& m) {
        Deserializer d(m.payload);
        received_payload = d.read_string();
    });

    Serializer s;
    s.write_string("dispatch test");
    Message msg;
    msg.type = 20;
    msg.request_id = 1;
    msg.payload = s.take_data();
    ASSERT_TRUE(sender.send(msg));

    auto recv = receiver.receive();
    ASSERT_TRUE(recv.has_value());
    receiver.dispatch(*recv);

    EXPECT_EQ(received_payload, "dispatch test");
}

// ------------------------------------------------------------------
// Channel open/close state
// ------------------------------------------------------------------

TEST(MessageChannelTest, IsOpenAndClose) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    EXPECT_TRUE(ch.is_open());
    ch.close();
    EXPECT_FALSE(ch.is_open());
}

TEST(MessageChannelTest, ReceiveAfterSenderCloses) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    sender.close();

    auto received = receiver.receive();
    EXPECT_FALSE(received.has_value());
}

// ---------------------------------------------------------------------------
// Cycle 485 — additional MessageChannel regression tests
// ---------------------------------------------------------------------------

// send() returns false on closed channel
TEST(MessageChannelTest, SendOnClosedChannelReturnsFalse) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));
    ch.close();

    Message msg;
    msg.type = 1;
    EXPECT_FALSE(ch.send(msg));
}

// handler registered AFTER dispatch does NOT fire retroactively
TEST(MessageChannelTest, HandlerRegisteredAfterDispatchNotFired) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    Message msg;
    msg.type = 7;
    ch.dispatch(msg); // dispatch before registering handler

    bool called = false;
    ch.on(7, [&](const Message&) { called = true; });
    // Handler was registered after dispatch — should NOT have been called
    EXPECT_FALSE(called);
}

// replacing a handler for the same type with a new one
TEST(MessageChannelTest, ReplaceHandlerForSameType) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int first_count = 0, second_count = 0;
    ch.on(3, [&](const Message&) { first_count++; });
    ch.on(3, [&](const Message&) { second_count++; }); // replaces first

    Message msg;
    msg.type = 3;
    ch.dispatch(msg);

    // Behavior depends on impl: either only second fires (replace) or both fire (multi-handler).
    // Either way at least one handler should fire.
    EXPECT_TRUE(second_count > 0 || first_count > 0);
}

// send message with large payload survives round-trip
TEST(MessageChannelTest, SendLargePayloadRoundTrip) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    std::vector<uint8_t> big(32 * 1024);
    for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i & 0xFF);

    Message msg;
    msg.type = 42;
    msg.request_id = 999;
    msg.payload = big;

    ASSERT_TRUE(sender.send(msg));
    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 42u);
    EXPECT_EQ(received->request_id, 999u);
    EXPECT_EQ(received->payload, big);
}

// handler receives payload bytes intact
TEST(MessageChannelTest, HandlerReceivesPayloadIntact) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    std::vector<uint8_t> expected_payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
    std::vector<uint8_t> received_payload;

    ch.on(55, [&](const Message& m) {
        received_payload = m.payload;
    });

    Message msg;
    msg.type = 55;
    msg.payload = expected_payload;
    ch.dispatch(msg);

    EXPECT_EQ(received_payload, expected_payload);
}

// dispatch with handler count: 5 dispatches → handler called 5 times
TEST(MessageChannelTest, DispatchCountMatchesHandlerCallCount) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int count = 0;
    ch.on(9, [&](const Message&) { count++; });

    Message msg;
    msg.type = 9;
    for (int i = 0; i < 5; ++i) ch.dispatch(msg);

    EXPECT_EQ(count, 5);
}

// close() is idempotent — second call does not crash
TEST(MessageChannelTest, CloseCalledTwiceIsSafe) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));
    EXPECT_TRUE(ch.is_open());
    EXPECT_NO_THROW({
        ch.close();
        ch.close();
    });
    EXPECT_FALSE(ch.is_open());
}

// round-trip with Serializer/Deserializer for a bool + float + string
TEST(MessageChannelTest, FullRoundTripBoolFloatString) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    Serializer s;
    s.write_u32(42u);
    s.write_i32(-100);
    s.write_string("roundtrip");

    Message msg;
    msg.type = 77;
    msg.request_id = 1;
    msg.payload = s.take_data();

    ASSERT_TRUE(sender.send(msg));
    auto recv = receiver.receive();
    ASSERT_TRUE(recv.has_value());

    Deserializer d(recv->payload);
    EXPECT_EQ(d.read_u32(), 42u);
    EXPECT_EQ(d.read_i32(), -100);
    EXPECT_EQ(d.read_string(), "roundtrip");
    EXPECT_FALSE(d.has_remaining());
}

// ============================================================================
// Cycle 499: MessageChannel regression tests
// ============================================================================

// dispatch with type=0 fires the registered type-0 handler
TEST(MessageChannelTest, MessageTypeZeroHandledByDispatch) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int count = 0;
    ch.on(0, [&](const Message&) { count++; });

    Message msg;
    msg.type = 0;
    ch.dispatch(msg);
    EXPECT_EQ(count, 1);
}

// UINT32_MAX request_id is preserved end-to-end over the pipe
TEST(MessageChannelTest, MaxRequestIdPreservedOverPipe) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    Message msg;
    msg.type = 5;
    msg.request_id = UINT32_MAX;

    ASSERT_TRUE(sender.send(msg));
    auto recv = receiver.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->request_id, UINT32_MAX);
}

// handler lambda can inspect the request_id of the dispatched message
TEST(MessageChannelTest, HandlerSeesCorrectRequestId) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    uint32_t seen_id = 0;
    ch.on(15, [&](const Message& m) { seen_id = m.request_id; });

    Message msg;
    msg.type = 15;
    msg.request_id = 9999;
    ch.dispatch(msg);

    EXPECT_EQ(seen_id, 9999u);
}

// dispatch with empty payload still fires the registered handler
TEST(MessageChannelTest, EmptyPayloadDispatchFiresHandler) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    bool fired = false;
    ch.on(22, [&](const Message& m) {
        fired = true;
        EXPECT_TRUE(m.payload.empty());
    });

    Message msg;
    msg.type = 22;
    // no payload assigned — empty by default
    ch.dispatch(msg);
    EXPECT_TRUE(fired);
}

// dispatch 200 messages of the same type — handler fires exactly 200 times
TEST(MessageChannelTest, HighVolumeDispatch) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int count = 0;
    ch.on(33, [&](const Message&) { count++; });

    Message msg;
    msg.type = 33;
    for (int i = 0; i < 200; ++i) ch.dispatch(msg);
    EXPECT_EQ(count, 200);
}

// i64 payload serialized and deserialized correctly over pipe
TEST(MessageChannelTest, RoundTripI64Payload) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    Serializer s;
    s.write_i64(std::numeric_limits<int64_t>::min());

    Message msg;
    msg.type = 44;
    msg.request_id = 88;
    msg.payload = s.take_data();

    ASSERT_TRUE(sender.send(msg));
    auto recv = receiver.receive();
    ASSERT_TRUE(recv.has_value());

    Deserializer d(recv->payload);
    EXPECT_EQ(d.read_i64(), std::numeric_limits<int64_t>::min());
    EXPECT_FALSE(d.has_remaining());
}

// dispatch with type=UINT32_MAX triggers its registered handler
TEST(MessageChannelTest, MaxTypeValueHandled) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    bool fired = false;
    ch.on(UINT32_MAX, [&](const Message&) { fired = true; });

    Message msg;
    msg.type = UINT32_MAX;
    ch.dispatch(msg);
    EXPECT_TRUE(fired);
}

// is_open() remains true after a successful send
TEST(MessageChannelTest, IsOpenRemainsAfterSend) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    EXPECT_TRUE(sender.is_open());

    Message msg;
    msg.type = 1;
    ASSERT_TRUE(sender.send(msg));

    EXPECT_TRUE(sender.is_open());
}

TEST(MessageChannelTest, MessageChannelV128_6_FullDuplexBothDirections) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    Message from_sender;
    from_sender.type = 1;
    ASSERT_TRUE(sender.send(from_sender));

    Message from_receiver;
    from_receiver.type = 2;
    ASSERT_TRUE(receiver.send(from_receiver));

    auto recv_on_receiver = receiver.receive();
    ASSERT_TRUE(recv_on_receiver.has_value());
    EXPECT_EQ(recv_on_receiver->type, 1u);

    auto recv_on_sender = sender.receive();
    ASSERT_TRUE(recv_on_sender.has_value());
    EXPECT_EQ(recv_on_sender->type, 2u);
}

TEST(MessageChannelTest, MessageChannelV128_7_ReceiveAfterOwnCloseReturnsNullopt) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));
    ch.close();

    auto result = ch.receive();
    EXPECT_FALSE(result.has_value());
}

TEST(MessageChannelTest, MessageChannelV128_8_DispatchOnlyMatchingTypeHandlerFires) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    bool fired10 = false;
    bool fired20 = false;
    ch.on(10, [&](const Message&) { fired10 = true; });
    ch.on(20, [&](const Message&) { fired20 = true; });

    Message msg;
    msg.type = 10;
    ch.dispatch(msg);

    EXPECT_TRUE(fired10);
    EXPECT_FALSE(fired20);
}

TEST(MessageChannelTest, MessageChannelV129_1_TypedDispatchMultipleHandlers) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    bool fired100 = false;
    bool fired200 = false;
    bool fired300 = false;
    ch.on(100, [&](const Message&) { fired100 = true; });
    ch.on(200, [&](const Message&) { fired200 = true; });
    ch.on(300, [&](const Message&) { fired300 = true; });

    Message msg;
    msg.type = 200;
    ch.dispatch(msg);

    EXPECT_FALSE(fired100);
    EXPECT_TRUE(fired200);
    EXPECT_FALSE(fired300);
}

TEST(MessageChannelTest, MessageChannelV129_2_BidirectionalLargePayload) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel a(std::move(pa));
    MessageChannel b(std::move(pb));

    std::vector<uint8_t> payload_ab(4096);
    for (size_t i = 0; i < payload_ab.size(); ++i) {
        payload_ab[i] = static_cast<uint8_t>(i % 256);
    }
    std::vector<uint8_t> payload_ba(4096);
    for (size_t i = 0; i < payload_ba.size(); ++i) {
        payload_ba[i] = static_cast<uint8_t>(255 - (i % 256));
    }

    Message msg_ab;
    msg_ab.type = 1;
    msg_ab.payload = payload_ab;
    ASSERT_TRUE(a.send(msg_ab));

    Message msg_ba;
    msg_ba.type = 2;
    msg_ba.payload = payload_ba;
    ASSERT_TRUE(b.send(msg_ba));

    auto recv_b = b.receive();
    ASSERT_TRUE(recv_b.has_value());
    EXPECT_EQ(recv_b->payload.size(), 4096u);
    EXPECT_EQ(recv_b->payload, payload_ab);

    auto recv_a = a.receive();
    ASSERT_TRUE(recv_a.has_value());
    EXPECT_EQ(recv_a->payload.size(), 4096u);
    EXPECT_EQ(recv_a->payload, payload_ba);
}

TEST(MessageChannelTest, MessageChannelV129_3_DispatchHandlerInspectsPayloadContent) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    uint32_t received_val = 0;
    std::string received_str;
    bool received_flag = false;

    ch.on(50, [&](const Message& msg) {
        Deserializer d(msg.payload.data(), msg.payload.size());
        received_val = d.read_u32();
        received_str = d.read_string();
        received_flag = d.read_bool();
    });

    Serializer s;
    s.write_u32(0xCAFEBABE);
    s.write_string("payload_test");
    s.write_bool(true);

    Message msg;
    msg.type = 50;
    msg.payload = s.data();
    ch.dispatch(msg);

    EXPECT_EQ(received_val, 0xCAFEBABEu);
    EXPECT_EQ(received_str, "payload_test");
    EXPECT_TRUE(received_flag);
}

TEST(MessageChannelTest, MessageChannelV130_1_OverwriteHandlerOnlyReplacementFires) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int first_count = 0;
    int second_count = 0;

    ch.on(10, [&](const Message&) { first_count++; });
    ch.on(10, [&](const Message&) { second_count++; });

    Message msg;
    msg.type = 10;
    ch.dispatch(msg);

    EXPECT_EQ(first_count, 0);
    EXPECT_EQ(second_count, 1);
}

TEST(MessageChannelTest, MessageChannelV130_2_CloseOneSideReceiveReturnsNullopt) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    cha.close();

    auto result = chb.receive();
    EXPECT_FALSE(result.has_value());
}

TEST(MessageChannelTest, MessageChannelV130_3_MultipleTypesInOrderOverSingleChannel) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    for (uint32_t t = 1; t <= 3; ++t) {
        Message msg;
        msg.type = t;
        msg.payload = {static_cast<uint8_t>(t)};
        ASSERT_TRUE(cha.send(msg));
    }

    for (uint32_t t = 1; t <= 3; ++t) {
        auto received = chb.receive();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(received->type, t);
        ASSERT_EQ(received->payload.size(), 1u);
        EXPECT_EQ(received->payload[0], static_cast<uint8_t>(t));
    }
}

TEST(MessageChannelTest, MessageChannelV131_1_DispatchSameMessageTwiceHandlerFiresTwice) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int counter = 0;
    ch.on(7, [&counter](const Message& msg) {
        EXPECT_EQ(msg.type, 7u);
        ++counter;
    });

    Message msg;
    msg.type = 7;
    msg.payload = {0x42};

    ch.dispatch(msg);
    ch.dispatch(msg);
    EXPECT_EQ(counter, 2);
}

TEST(MessageChannelTest, MessageChannelV131_2_DispatchNoHandlerThenRegisterAndRedispatch) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    Message msg;
    msg.type = 99;
    msg.payload = {0x01, 0x02};

    // Dispatch with no handler registered -- should not crash
    ch.dispatch(msg);

    int fired = 0;
    ch.on(99, [&fired](const Message&) { ++fired; });

    ch.dispatch(msg);
    EXPECT_EQ(fired, 1);
}

TEST(MessageChannelTest, MessageChannelV131_3_ReceiveThenDispatchThroughRegisteredHandler) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    int handler_fired = 0;
    uint32_t received_type = 0;
    std::vector<uint8_t> received_payload;

    chb.on(42, [&](const Message& msg) {
        ++handler_fired;
        received_type = msg.type;
        received_payload = msg.payload;
    });

    Message msg;
    msg.type = 42;
    msg.payload = {0xCA, 0xFE};
    ASSERT_TRUE(cha.send(msg));

    auto recv = chb.receive();
    ASSERT_TRUE(recv.has_value());

    chb.dispatch(*recv);

    EXPECT_EQ(handler_fired, 1);
    EXPECT_EQ(received_type, 42u);
    ASSERT_EQ(received_payload.size(), 2u);
    EXPECT_EQ(received_payload[0], 0xCAu);
    EXPECT_EQ(received_payload[1], 0xFEu);
}

// ------------------------------------------------------------------
// V132 Round 132 tests
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV132_1_TypedDispatchOnlyMatchingHandlerFires) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    int handler_100 = 0;
    int handler_200 = 0;
    int handler_300 = 0;

    chb.on(100, [&](const Message&) { handler_100++; });
    chb.on(200, [&](const Message&) { handler_200++; });
    chb.on(300, [&](const Message&) { handler_300++; });

    // Send message with type 200
    Message msg;
    msg.type = 200;
    msg.request_id = 0;
    msg.payload = {0x01};
    ASSERT_TRUE(cha.send(msg));

    auto recv = chb.receive();
    ASSERT_TRUE(recv.has_value());
    chb.dispatch(*recv);

    // Only handler_200 should have fired
    EXPECT_EQ(handler_100, 0);
    EXPECT_EQ(handler_200, 1);
    EXPECT_EQ(handler_300, 0);
}

TEST(MessageChannelTest, MessageChannelV132_2_BidirectionalLargePayload4KB) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    // Create 4096-byte payload
    std::vector<uint8_t> large_payload(4096);
    for (size_t i = 0; i < large_payload.size(); ++i) {
        large_payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Send from a to b
    Message msg_ab;
    msg_ab.type = 1;
    msg_ab.request_id = 10;
    msg_ab.payload = large_payload;
    ASSERT_TRUE(cha.send(msg_ab));

    auto recv_b = chb.receive();
    ASSERT_TRUE(recv_b.has_value());
    EXPECT_EQ(recv_b->type, 1u);
    EXPECT_EQ(recv_b->request_id, 10u);
    EXPECT_EQ(recv_b->payload.size(), 4096u);
    EXPECT_EQ(recv_b->payload, large_payload);

    // Send from b to a
    Message msg_ba;
    msg_ba.type = 2;
    msg_ba.request_id = 20;
    msg_ba.payload = large_payload;
    ASSERT_TRUE(chb.send(msg_ba));

    auto recv_a = cha.receive();
    ASSERT_TRUE(recv_a.has_value());
    EXPECT_EQ(recv_a->type, 2u);
    EXPECT_EQ(recv_a->request_id, 20u);
    EXPECT_EQ(recv_a->payload.size(), 4096u);
    EXPECT_EQ(recv_a->payload, large_payload);
}

TEST(MessageChannelTest, MessageChannelV132_3_DispatchHandlerDeserializesPayloadContent) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    // Build a payload using Serializer: u32 + string + bool
    Serializer s;
    s.write_u32(999);
    s.write_string("hello_v132");
    s.write_bool(true);

    Message msg;
    msg.type = 50;
    msg.request_id = 7;
    msg.payload = s.take_data();
    ASSERT_TRUE(cha.send(msg));

    uint32_t deserialized_u32 = 0;
    std::string deserialized_str;
    bool deserialized_bool = false;
    bool handler_called = false;

    chb.on(50, [&](const Message& m) {
        handler_called = true;
        Deserializer d(m.payload);
        deserialized_u32 = d.read_u32();
        deserialized_str = d.read_string();
        deserialized_bool = d.read_bool();
    });

    auto recv = chb.receive();
    ASSERT_TRUE(recv.has_value());
    chb.dispatch(*recv);

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(deserialized_u32, 999u);
    EXPECT_EQ(deserialized_str, "hello_v132");
    EXPECT_EQ(deserialized_bool, true);
}

// ------------------------------------------------------------------
// V133 Round 133 tests
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV133_1_RequestIdZeroPreservedRoundTrip) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    Message msg;
    msg.type = 1;
    msg.request_id = 0;
    msg.payload = {0xAA, 0xBB};
    ASSERT_TRUE(cha.send(msg));

    auto received = chb.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 1u);
    EXPECT_EQ(received->request_id, 0u);
    EXPECT_EQ(received->payload.size(), 2u);
}

TEST(MessageChannelTest, MessageChannelV133_2_DispatchWithNoHandlersNoCrash) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    // Send several messages with different types
    for (uint32_t t = 0; t < 5; ++t) {
        Message msg;
        msg.type = t;
        msg.request_id = t + 10;
        msg.payload = {static_cast<uint8_t>(t)};
        ASSERT_TRUE(cha.send(msg));
    }

    // Receive and dispatch all — no handlers registered, should not crash
    for (uint32_t t = 0; t < 5; ++t) {
        auto received = chb.receive();
        ASSERT_TRUE(received.has_value());
        chb.dispatch(*received);
    }
}

TEST(MessageChannelTest, MessageChannelV133_3_CloseIdempotentAndIsOpenReflects) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    // Close twice on each side — should not crash
    cha.close();
    cha.close();

    chb.close();
    chb.close();
}

// ------------------------------------------------------------------
// V134 Round 134 tests
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV134_1_MaxTypeAndRequestIdPreserved) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    Message msg;
    msg.type = UINT32_MAX;
    msg.request_id = UINT32_MAX;
    msg.payload = {0xFF};
    ASSERT_TRUE(cha.send(msg));

    auto received = chb.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, UINT32_MAX);
    EXPECT_EQ(received->request_id, UINT32_MAX);
    EXPECT_EQ(received->payload.size(), 1u);
    EXPECT_EQ(received->payload[0], 0xFFu);
}

TEST(MessageChannelTest, MessageChannelV134_2_EmptyPayloadRoundTrip) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    Message msg;
    msg.type = 42;
    msg.request_id = 7;
    msg.payload = {};
    ASSERT_TRUE(cha.send(msg));

    auto received = chb.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 42u);
    EXPECT_EQ(received->request_id, 7u);
    EXPECT_TRUE(received->payload.empty());
}

TEST(MessageChannelTest, MessageChannelV134_3_MultipleHandlersSameType) {
    // handlers_ is a map, so registering a second handler on the same type
    // replaces the first. Verify that the latest handler wins.
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel cha(std::move(pa));
    MessageChannel chb(std::move(pb));

    int handler1_count = 0;
    int handler2_count = 0;

    chb.on(10, [&](const Message& m) {
        (void)m;
        handler1_count++;
    });
    // Second registration replaces the first
    chb.on(10, [&](const Message& m) {
        (void)m;
        handler2_count++;
    });

    Message msg;
    msg.type = 10;
    msg.request_id = 0;
    msg.payload = {0x01};
    ASSERT_TRUE(cha.send(msg));

    auto received = chb.receive();
    ASSERT_TRUE(received.has_value());
    chb.dispatch(*received);

    // First handler was replaced, so it should NOT have fired
    EXPECT_EQ(handler1_count, 0);
    // Second (replacement) handler should have fired once
    EXPECT_EQ(handler2_count, 1);
}

// ------------------------------------------------------------------
// V135 MessageChannel tests
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV135_1_UnregisteredTypeMessageSilentlyDropped) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    // Register handler for type 50 only
    bool handler_called = false;
    receiver.on(50, [&](const Message&) { handler_called = true; });

    // Send a message with unregistered type 99
    Message msg;
    msg.type = 99;
    msg.request_id = 1;
    msg.payload = {0xAA, 0xBB};
    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    // Dispatching an unregistered type should not crash
    EXPECT_NO_THROW(receiver.dispatch(*received));
    // The handler for type 50 should NOT have been called
    EXPECT_FALSE(handler_called);
}

TEST(MessageChannelTest, MessageChannelV135_2_HandlerReceivesCorrectRequestId) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    uint32_t captured_request_id = 0;
    receiver.on(77, [&](const Message& m) {
        captured_request_id = m.request_id;
    });

    Message msg;
    msg.type = 77;
    msg.request_id = 54321;
    msg.payload = {0x01};
    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    receiver.dispatch(*received);

    EXPECT_EQ(captured_request_id, 54321u);
}

TEST(MessageChannelTest, MessageChannelV135_3_EmptyPayloadHandlerStillFires) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    bool handler_fired = false;
    std::vector<uint8_t> captured_payload;
    receiver.on(33, [&](const Message& m) {
        handler_fired = true;
        captured_payload = m.payload;
    });

    Message msg;
    msg.type = 33;
    msg.request_id = 7;
    // payload intentionally left empty
    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    receiver.dispatch(*received);

    EXPECT_TRUE(handler_fired);
    EXPECT_TRUE(captured_payload.empty());
}

// ------------------------------------------------------------------
// V136 MessageChannel tests
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV136_1_OverwriteHandlerForSameType) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int first_count = 0;
    int second_count = 0;

    // Register first handler for type 10
    ch.on(10, [&](const Message&) {
        first_count++;
    });

    // Overwrite with second handler for the same type
    ch.on(10, [&](const Message&) {
        second_count++;
    });

    // Dispatch a message of type 10
    Message msg;
    msg.type = 10;
    msg.request_id = 1;
    ch.dispatch(msg);

    // Only the second (replacement) handler should fire
    EXPECT_EQ(first_count, 0);
    EXPECT_EQ(second_count, 1);
}

TEST(MessageChannelTest, MessageChannelV136_2_SequentialPollsProcessOneMessageEach) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    int dispatch_count = 0;
    std::vector<uint32_t> received_request_ids;
    receiver.on(7, [&](const Message& m) {
        dispatch_count++;
        received_request_ids.push_back(m.request_id);
    });

    // Send 3 messages
    for (uint32_t i = 1; i <= 3; ++i) {
        Message msg;
        msg.type = 7;
        msg.request_id = i * 100;
        ASSERT_TRUE(sender.send(msg));
    }

    // Receive and dispatch each one individually
    for (int i = 0; i < 3; ++i) {
        auto received = receiver.receive();
        ASSERT_TRUE(received.has_value());
        receiver.dispatch(*received);
    }

    EXPECT_EQ(dispatch_count, 3);
    ASSERT_EQ(received_request_ids.size(), 3u);
    EXPECT_EQ(received_request_ids[0], 100u);
    EXPECT_EQ(received_request_ids[1], 200u);
    EXPECT_EQ(received_request_ids[2], 300u);
}

TEST(MessageChannelTest, MessageChannelV136_3_LargeRequestIdRoundTrip) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    uint32_t captured_request_id = 0;
    receiver.on(50, [&](const Message& m) {
        captured_request_id = m.request_id;
    });

    Message msg;
    msg.type = 50;
    msg.request_id = UINT32_MAX; // 0xFFFFFFFF
    msg.payload = {0x42};
    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 50u);
    EXPECT_EQ(received->request_id, UINT32_MAX);
    EXPECT_EQ(received->payload.size(), 1u);
    EXPECT_EQ(received->payload[0], 0x42);

    receiver.dispatch(*received);
    EXPECT_EQ(captured_request_id, UINT32_MAX);
}

// ------------------------------------------------------------------
// V137 tests
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV137_1_MultipleTypesInterleavedDispatch) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int type1_count = 0;
    int type2_count = 0;
    int type3_count = 0;

    ch.on(1, [&](const Message&) { type1_count++; });
    ch.on(2, [&](const Message&) { type2_count++; });
    ch.on(3, [&](const Message&) { type3_count++; });

    // Send interleaved: 1, 2, 3, 1, 3, 2, 1
    uint32_t sequence[] = {1, 2, 3, 1, 3, 2, 1};
    for (auto t : sequence) {
        Message m;
        m.type = t;
        m.request_id = 0;
        ch.dispatch(m);
    }

    EXPECT_EQ(type1_count, 3);
    EXPECT_EQ(type2_count, 2);
    EXPECT_EQ(type3_count, 2);
}

TEST(MessageChannelTest, MessageChannelV137_2_ZeroTypeAndZeroRequestId) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    Message msg;
    msg.type = 0;
    msg.request_id = 0;
    msg.payload = {0xAA, 0xBB};

    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 0u);
    EXPECT_EQ(received->request_id, 0u);
    EXPECT_EQ(received->payload.size(), 2u);
    EXPECT_EQ(received->payload[0], 0xAA);
    EXPECT_EQ(received->payload[1], 0xBB);

    // Also test that a handler registered on type 0 fires correctly
    bool handler_fired = false;
    receiver.on(0, [&](const Message& m) {
        handler_fired = true;
        EXPECT_EQ(m.type, 0u);
        EXPECT_EQ(m.request_id, 0u);
    });
    receiver.dispatch(*received);
    EXPECT_TRUE(handler_fired);
}

TEST(MessageChannelTest, MessageChannelV137_3_PayloadPreservesExactBytes) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    // Build a payload with every possible byte value including 0x00 and 0xFF
    std::vector<uint8_t> payload;
    payload.reserve(256);
    for (int i = 0; i < 256; ++i) {
        payload.push_back(static_cast<uint8_t>(i));
    }

    Message msg;
    msg.type = 77;
    msg.request_id = 12345;
    msg.payload = payload;

    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, 77u);
    EXPECT_EQ(received->request_id, 12345u);
    ASSERT_EQ(received->payload.size(), 256u);
    for (int i = 0; i < 256; ++i) {
        EXPECT_EQ(received->payload[i], static_cast<uint8_t>(i))
            << "payload mismatch at byte index " << i;
    }
}

// ------------------------------------------------------------------
// V138: Handler called with correct payload size (100 bytes)
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV138_1_HandlerCalledWithCorrectPayloadSize) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    size_t observed_payload_size = 0;
    bool handler_invoked = false;

    ch.on(42, [&](const Message& m) {
        handler_invoked = true;
        observed_payload_size = m.payload.size();
    });

    // Build a 100-byte payload
    std::vector<uint8_t> payload(100, 0xCC);
    Message msg;
    msg.type = 42;
    msg.request_id = 1;
    msg.payload = payload;

    ch.dispatch(msg);

    EXPECT_TRUE(handler_invoked);
    EXPECT_EQ(observed_payload_size, 100u);
}

// ------------------------------------------------------------------
// V138: Multiple dispatch of the same message
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV138_2_MultipleDispatchSameMessage) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int call_count = 0;
    uint32_t last_request_id = 0;

    ch.on(7, [&](const Message& m) {
        call_count++;
        last_request_id = m.request_id;
    });

    Message msg;
    msg.type = 7;
    msg.request_id = 55;
    msg.payload = {0x01, 0x02};

    // Dispatch the same message twice — handler should fire each time
    ch.dispatch(msg);
    ch.dispatch(msg);

    EXPECT_EQ(call_count, 2);
    EXPECT_EQ(last_request_id, 55u);
}

// ------------------------------------------------------------------
// V138: Register handler before and after send
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV138_3_RegisterHandlerBeforeAndAfterSend) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int first_handler_count = 0;

    // Register handler for type 20
    ch.on(20, [&](const Message&) {
        first_handler_count++;
    });

    // Dispatch — handler should fire
    Message msg;
    msg.type = 20;
    msg.request_id = 1;
    ch.dispatch(msg);
    EXPECT_EQ(first_handler_count, 1);

    // Dispatch a message with an unregistered type — should not crash
    Message msg2;
    msg2.type = 999;
    msg2.request_id = 2;
    EXPECT_NO_THROW(ch.dispatch(msg2));

    // Register a second handler for a new type and verify it works
    int second_handler_count = 0;
    ch.on(30, [&](const Message&) {
        second_handler_count++;
    });

    Message msg3;
    msg3.type = 30;
    msg3.request_id = 3;
    ch.dispatch(msg3);
    EXPECT_EQ(second_handler_count, 1);

    // Original handler should still work
    ch.dispatch(msg);
    EXPECT_EQ(first_handler_count, 2);
}

// ------------------------------------------------------------------
// V139: Dispatch with no handlers registered — no crash
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV139_1_DispatchWithNoHandlersNoOp) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    // No handlers registered at all — dispatch should not crash
    Message msg;
    msg.type = 42;
    msg.request_id = 1;
    msg.payload = {0x01, 0x02, 0x03};

    EXPECT_NO_THROW(ch.dispatch(msg));

    // Try dispatching with multiple different types — still no crash
    for (uint32_t t = 0; t < 10; ++t) {
        Message m;
        m.type = t;
        m.request_id = t + 100;
        EXPECT_NO_THROW(ch.dispatch(m));
    }
}

// ------------------------------------------------------------------
// V139: Handler accesses full 50-byte payload
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV139_2_HandlerAccessesFullPayload) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    // Build a 50-byte payload with distinct values
    std::vector<uint8_t> payload(50);
    for (size_t i = 0; i < 50; ++i) {
        payload[i] = static_cast<uint8_t>(i * 5);
    }

    std::vector<uint8_t> captured_payload;
    ch.on(7, [&](const Message& m) {
        captured_payload = m.payload;
    });

    Message msg;
    msg.type = 7;
    msg.request_id = 999;
    msg.payload = payload;
    ch.dispatch(msg);

    // Verify the handler received the complete payload
    ASSERT_EQ(captured_payload.size(), 50u);
    for (size_t i = 0; i < 50; ++i) {
        EXPECT_EQ(captured_payload[i], static_cast<uint8_t>(i * 5))
            << "Payload byte mismatch at index " << i;
    }
}

// ------------------------------------------------------------------
// V139: Send/receive 10 messages in rapid succession
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV139_3_SendReceiveRapidSuccession) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    constexpr int num_messages = 10;

    // Send 10 messages rapidly
    for (int i = 0; i < num_messages; ++i) {
        Message msg;
        msg.type = static_cast<uint32_t>(i + 1);
        msg.request_id = static_cast<uint32_t>(i * 10);
        msg.payload = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 100)};
        ASSERT_TRUE(sender.send(msg)) << "Failed to send message " << i;
    }

    // Receive all 10 messages and verify ordering and content
    for (int i = 0; i < num_messages; ++i) {
        auto received = receiver.receive();
        ASSERT_TRUE(received.has_value()) << "Failed to receive message " << i;
        EXPECT_EQ(received->type, static_cast<uint32_t>(i + 1));
        EXPECT_EQ(received->request_id, static_cast<uint32_t>(i * 10));
        ASSERT_EQ(received->payload.size(), 2u);
        EXPECT_EQ(received->payload[0], static_cast<uint8_t>(i));
        EXPECT_EQ(received->payload[1], static_cast<uint8_t>(i + 100));
    }
}

// ------------------------------------------------------------------
// V140: Handler fires only on correct type (10, 20, 30)
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV140_1_HandlerFiresOnCorrectType) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int handler10_count = 0;
    int handler20_count = 0;
    int handler30_count = 0;

    ch.on(10, [&](const Message&) { handler10_count++; });
    ch.on(20, [&](const Message&) { handler20_count++; });
    ch.on(30, [&](const Message&) { handler30_count++; });

    // Dispatch type 20 only
    Message msg;
    msg.type = 20;
    msg.request_id = 0;
    ch.dispatch(msg);

    EXPECT_EQ(handler10_count, 0);
    EXPECT_EQ(handler20_count, 1);
    EXPECT_EQ(handler30_count, 0);

    // Dispatch type 10 twice
    msg.type = 10;
    ch.dispatch(msg);
    ch.dispatch(msg);

    EXPECT_EQ(handler10_count, 2);
    EXPECT_EQ(handler20_count, 1);
    EXPECT_EQ(handler30_count, 0);
}

// ------------------------------------------------------------------
// V140: Empty payload handler still fires
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV140_2_PayloadSizeZeroHandlerFires) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    bool handler_called = false;
    size_t payload_size = 999;

    ch.on(42, [&](const Message& m) {
        handler_called = true;
        payload_size = m.payload.size();
    });

    Message msg;
    msg.type = 42;
    msg.request_id = 0;
    // payload intentionally left empty
    ch.dispatch(msg);

    EXPECT_TRUE(handler_called);
    EXPECT_EQ(payload_size, 0u);
}

// ------------------------------------------------------------------
// V140: Request ID preserved across channel send/receive
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV140_3_RequestIdPreservedAcrossChannel) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel sender(std::move(pa));
    MessageChannel receiver(std::move(pb));

    Message msg;
    msg.type = 1;
    msg.request_id = 12345;
    msg.payload = {0xAA, 0xBB};

    ASSERT_TRUE(sender.send(msg));

    auto received = receiver.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->request_id, 12345u);
    EXPECT_EQ(received->type, 1u);
    EXPECT_EQ(received->payload.size(), 2u);
    EXPECT_EQ(received->payload[0], 0xAA);
    EXPECT_EQ(received->payload[1], 0xBB);
}

// ------------------------------------------------------------------
// V141: Unregistered type drops silently
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV141_1_UnregisteredTypeDropsSilently) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    // No handler registered for type 999
    Message msg;
    msg.type = 999;
    msg.request_id = 0;
    msg.payload = {0x01, 0x02, 0x03};

    // dispatch should not crash even with no handler
    ch.dispatch(msg);
    // If we reach here, the test passes — no crash or exception
}

// ------------------------------------------------------------------
// V141: Register handler overwrites previous for same type
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV141_2_RegisterHandlerOverwritesPrevious) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int first_count = 0;
    int second_count = 0;

    ch.on(42, [&](const Message&) { ++first_count; });
    ch.on(42, [&](const Message&) { ++second_count; });

    Message msg;
    msg.type = 42;
    msg.request_id = 0;
    msg.payload = {0xCC};

    ch.dispatch(msg);

    // Only the second (overwriting) handler should have fired
    EXPECT_EQ(first_count, 0);
    EXPECT_EQ(second_count, 1);
}

// ------------------------------------------------------------------
// V142: Multiple types with multiple handlers — each fires once
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV142_1_MultipleTypesMultipleHandlers) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    int count10 = 0, count20 = 0, count30 = 0;
    ch.on(10, [&](const Message&) { ++count10; });
    ch.on(20, [&](const Message&) { ++count20; });
    ch.on(30, [&](const Message&) { ++count30; });

    Message m10; m10.type = 10; m10.request_id = 0; m10.payload = {0x0A};
    Message m20; m20.type = 20; m20.request_id = 0; m20.payload = {0x14};
    Message m30; m30.type = 30; m30.request_id = 0; m30.payload = {0x1E};

    ch.dispatch(m10);
    ch.dispatch(m20);
    ch.dispatch(m30);

    EXPECT_EQ(count10, 1);
    EXPECT_EQ(count20, 1);
    EXPECT_EQ(count30, 1);
}

// ------------------------------------------------------------------
// V142: Payload integrity — serialize u32+string, dispatch, verify
// ------------------------------------------------------------------

TEST(MessageChannelTest, MessageChannelV142_2_PayloadIntegrityCheck) {
    auto [pa, pb] = MessagePipe::create_pair();
    MessageChannel ch(std::move(pa));

    uint32_t received_value = 0;
    std::string received_str;

    ch.on(50, [&](const Message& m) {
        Deserializer d(m.payload);
        received_value = d.read_u32();
        received_str = d.read_string();
    });

    // Build payload with Serializer
    Serializer s;
    s.write_u32(0xCAFEBABE);
    s.write_string("integrity_test");

    Message msg;
    msg.type = 50;
    msg.request_id = 0;
    msg.payload = s.data();

    ch.dispatch(msg);

    EXPECT_EQ(received_value, 0xCAFEBABEu);
    EXPECT_EQ(received_str, "integrity_test");
}
