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
