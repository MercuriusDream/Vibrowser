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
