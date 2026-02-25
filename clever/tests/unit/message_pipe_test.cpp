#include <clever/ipc/message_pipe.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <numeric>
#include <vector>

using clever::ipc::MessagePipe;

// ------------------------------------------------------------------
// 1. Create pair and send/receive bytes
// ------------------------------------------------------------------

TEST(MessagePipeTest, CreatePairAndSendReceive) {
    auto [a, b] = MessagePipe::create_pair();

    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    ASSERT_TRUE(a.send(data));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, data);
}

TEST(MessagePipeTest, SendReceiveBothDirections) {
    auto [a, b] = MessagePipe::create_pair();

    std::vector<uint8_t> msg_ab = {10, 20, 30};
    std::vector<uint8_t> msg_ba = {40, 50, 60};

    ASSERT_TRUE(a.send(msg_ab));
    ASSERT_TRUE(b.send(msg_ba));

    auto recv_b = b.receive();
    ASSERT_TRUE(recv_b.has_value());
    EXPECT_EQ(*recv_b, msg_ab);

    auto recv_a = a.receive();
    ASSERT_TRUE(recv_a.has_value());
    EXPECT_EQ(*recv_a, msg_ba);
}

// ------------------------------------------------------------------
// 2. Send multiple messages
// ------------------------------------------------------------------

TEST(MessagePipeTest, SendMultipleMessages) {
    auto [a, b] = MessagePipe::create_pair();

    for (uint8_t i = 0; i < 10; ++i) {
        std::vector<uint8_t> data(i + 1, i);
        ASSERT_TRUE(a.send(data));
    }

    for (uint8_t i = 0; i < 10; ++i) {
        auto received = b.receive();
        ASSERT_TRUE(received.has_value());
        std::vector<uint8_t> expected(i + 1, i);
        EXPECT_EQ(*received, expected);
    }
}

// ------------------------------------------------------------------
// 3. Close one end — receive returns nullopt on other end
// ------------------------------------------------------------------

TEST(MessagePipeTest, CloseOneEndReceiveReturnsNullopt) {
    auto [a, b] = MessagePipe::create_pair();

    a.close();
    EXPECT_FALSE(a.is_open());

    auto received = b.receive();
    EXPECT_FALSE(received.has_value());
}

TEST(MessagePipeTest, CloseAndSendFails) {
    auto [a, b] = MessagePipe::create_pair();

    a.close();
    std::vector<uint8_t> data = {1, 2, 3};
    EXPECT_FALSE(a.send(data));
}

// ------------------------------------------------------------------
// 4. Send empty payload
// ------------------------------------------------------------------

TEST(MessagePipeTest, SendEmptyPayload) {
    auto [a, b] = MessagePipe::create_pair();

    std::vector<uint8_t> empty;
    ASSERT_TRUE(a.send(empty));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_TRUE(received->empty());
}

TEST(MessagePipeTest, SendEmptyPayloadRawPointer) {
    auto [a, b] = MessagePipe::create_pair();

    ASSERT_TRUE(a.send(nullptr, 0));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_TRUE(received->empty());
}

// ------------------------------------------------------------------
// 5. Send large payload (>64KB)
// ------------------------------------------------------------------

TEST(MessagePipeTest, SendLargePayload) {
    auto [a, b] = MessagePipe::create_pair();

    // 128KB payload
    std::vector<uint8_t> large_data(128 * 1024);
    std::iota(large_data.begin(), large_data.end(), static_cast<uint8_t>(0));

    ASSERT_TRUE(a.send(large_data));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->size(), large_data.size());
    EXPECT_EQ(*received, large_data);
}

// ------------------------------------------------------------------
// 6. Move constructor works
// ------------------------------------------------------------------

TEST(MessagePipeTest, MoveConstructor) {
    auto [a, b] = MessagePipe::create_pair();

    int original_fd = a.fd();
    MessagePipe moved_a(std::move(a));

    EXPECT_EQ(moved_a.fd(), original_fd);
    EXPECT_EQ(a.fd(), -1);  // NOLINT: testing moved-from state
    EXPECT_FALSE(a.is_open());  // NOLINT
    EXPECT_TRUE(moved_a.is_open());

    // Should still work after move
    std::vector<uint8_t> data = {7, 8, 9};
    ASSERT_TRUE(moved_a.send(data));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, data);
}

TEST(MessagePipeTest, MoveAssignment) {
    auto [a, b] = MessagePipe::create_pair();
    auto [c, d] = MessagePipe::create_pair();

    int b_fd = b.fd();
    // Move-assign b into c (should close c's old fd)
    c = std::move(b);

    EXPECT_EQ(c.fd(), b_fd);
    EXPECT_EQ(b.fd(), -1);  // NOLINT: testing moved-from state
    EXPECT_TRUE(c.is_open());

    // c should still be connected to a
    std::vector<uint8_t> data = {11, 22, 33};
    ASSERT_TRUE(a.send(data));
    auto received = c.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, data);
}

// ------------------------------------------------------------------
// is_open reflects state
// ------------------------------------------------------------------

TEST(MessagePipeTest, IsOpenReflectsState) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_TRUE(a.is_open());
    EXPECT_TRUE(b.is_open());

    a.close();
    EXPECT_FALSE(a.is_open());
    EXPECT_TRUE(b.is_open());

    b.close();
    EXPECT_FALSE(b.is_open());
}

// ------------------------------------------------------------------
// Constructor from fd
// ------------------------------------------------------------------

TEST(MessagePipeTest, ConstructFromInvalidFd) {
    MessagePipe p(-1);
    EXPECT_FALSE(p.is_open());
}
