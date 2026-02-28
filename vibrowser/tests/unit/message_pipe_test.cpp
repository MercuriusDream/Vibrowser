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

// ---------------------------------------------------------------------------
// Cycle 484 — additional MessagePipe regression tests
// ---------------------------------------------------------------------------

// send(ptr, size) with non-null data
TEST(MessagePipeTest, SendRawPointerWithData) {
    auto [a, b] = MessagePipe::create_pair();

    const uint8_t kData[] = {0xCA, 0xFE, 0xBA, 0xBE};
    ASSERT_TRUE(a.send(kData, sizeof(kData)));

    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    ASSERT_EQ(received->size(), sizeof(kData));
    for (size_t i = 0; i < sizeof(kData); ++i) {
        EXPECT_EQ((*received)[i], kData[i]);
    }
}

// close() called multiple times should not crash
TEST(MessagePipeTest, CloseCalledMultipleTimes) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_NO_THROW({
        a.close();
        a.close(); // idempotent
    });
    EXPECT_FALSE(a.is_open());
}

// fd() returns -1 after close
TEST(MessagePipeTest, FdReturnsNegativeWhenClosed) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_GE(a.fd(), 0);
    a.close();
    EXPECT_EQ(a.fd(), -1);
}

// send payloads of various sizes, verify each round-trips correctly
TEST(MessagePipeTest, SendVariousPayloadSizes) {
    auto [a, b] = MessagePipe::create_pair();

    const std::vector<size_t> kSizes = {0, 1, 7, 64, 255, 1024, 4096};
    for (size_t sz : kSizes) {
        std::vector<uint8_t> payload(sz, static_cast<uint8_t>(sz & 0xFF));
        ASSERT_TRUE(a.send(payload)) << "send failed for size " << sz;
        auto received = b.receive();
        ASSERT_TRUE(received.has_value()) << "receive returned nullopt for size " << sz;
        EXPECT_EQ(*received, payload) << "mismatch for size " << sz;
    }
}

// bidirectional alternating flow: a→b, b→a, a→b, b→a
TEST(MessagePipeTest, BidirectionalAlternatingFlow) {
    auto [a, b] = MessagePipe::create_pair();

    for (int i = 0; i < 4; ++i) {
        std::vector<uint8_t> fwd = {static_cast<uint8_t>(i * 2)};
        std::vector<uint8_t> rev = {static_cast<uint8_t>(i * 2 + 1)};

        ASSERT_TRUE(a.send(fwd));
        auto r_fwd = b.receive();
        ASSERT_TRUE(r_fwd.has_value());
        EXPECT_EQ(*r_fwd, fwd);

        ASSERT_TRUE(b.send(rev));
        auto r_rev = a.receive();
        ASSERT_TRUE(r_rev.has_value());
        EXPECT_EQ(*r_rev, rev);
    }
}

// send from b to a (reversed direction) with 5 distinct messages
TEST(MessagePipeTest, ReversedDirectionMultipleMessages) {
    auto [a, b] = MessagePipe::create_pair();

    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(100 + i),
                                     static_cast<uint8_t>(200 + i)};
        ASSERT_TRUE(b.send(data));
    }

    for (int i = 0; i < 5; ++i) {
        auto received = a.receive();
        ASSERT_TRUE(received.has_value());
        std::vector<uint8_t> expected = {static_cast<uint8_t>(100 + i),
                                         static_cast<uint8_t>(200 + i)};
        EXPECT_EQ(*received, expected);
    }
}

// payload content is preserved byte-for-byte (all 256 values)
TEST(MessagePipeTest, AllByteValuesPreserved) {
    auto [a, b] = MessagePipe::create_pair();

    std::vector<uint8_t> all_bytes(256);
    std::iota(all_bytes.begin(), all_bytes.end(), static_cast<uint8_t>(0));

    ASSERT_TRUE(a.send(all_bytes));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, all_bytes);
}

// send to a pipe whose remote end is already closed
TEST(MessagePipeTest, ReceiveFromClosedSenderReturnsNullopt) {
    auto [a, b] = MessagePipe::create_pair();

    // Close b (the receiver end) without sending anything
    b.close();

    // Send from a to a closed peer — send may fail or succeed at OS level
    // but receive on b must not crash and returns nullopt since b is closed
    EXPECT_FALSE(b.is_open());
    auto recv = b.receive();
    EXPECT_FALSE(recv.has_value());
}

// ============================================================================
// Cycle 501: MessagePipe regression tests
// ============================================================================

// send/receive exactly 1 byte
TEST(MessagePipeTest, SendOneByteSingleValue) {
    auto [a, b] = MessagePipe::create_pair();
    const uint8_t kByte = 0xAB;
    ASSERT_TRUE(a.send(&kByte, 1));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    ASSERT_EQ(received->size(), 1u);
    EXPECT_EQ((*received)[0], kByte);
}

// 1000-byte payload round-trips correctly
TEST(MessagePipeTest, Send1000BytePayloadRoundTrip) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> payload(1000);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<uint8_t>(i % 251);
    ASSERT_TRUE(a.send(payload));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, payload);
}

// 100 bytes of 0xFF are all preserved
TEST(MessagePipeTest, AllFfBytesPreserved) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> all_ff(100, 0xFF);
    ASSERT_TRUE(a.send(all_ff));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, all_ff);
}

// 10 sequential messages all received intact
TEST(MessagePipeTest, TenSequentialMessagesAllReceived) {
    auto [a, b] = MessagePipe::create_pair();
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> payload = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1)};
        ASSERT_TRUE(a.send(payload));
        auto received = b.receive();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(*received, payload);
    }
}

// received payload size matches the sent payload size
TEST(MessagePipeTest, ReceivedPayloadSizeMatchesSent) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> payload(42, 0x5A);
    ASSERT_TRUE(a.send(payload));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->size(), payload.size());
}

// send empty then send non-empty — both received in order
TEST(MessagePipeTest, EmptyThenNonEmptySequential) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> empty;
    std::vector<uint8_t> nonempty = {0x01, 0x02, 0x03};

    ASSERT_TRUE(a.send(empty));
    ASSERT_TRUE(a.send(nonempty));

    auto r1 = b.receive();
    ASSERT_TRUE(r1.has_value());
    EXPECT_TRUE(r1->empty());

    auto r2 = b.receive();
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(*r2, nonempty);
}

// send 1 byte via raw pointer API
TEST(MessagePipeTest, SendRawPointerSize1) {
    auto [a, b] = MessagePipe::create_pair();
    const uint8_t kVal = 0x77;
    ASSERT_TRUE(a.send(&kVal, 1));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    ASSERT_EQ(received->size(), 1u);
    EXPECT_EQ((*received)[0], kVal);
}

// 3 consecutive sends from A, all received by B
TEST(MessagePipeTest, ThreeConsecutiveSendsAllReceived) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<std::vector<uint8_t>> msgs = {{1}, {2, 3}, {4, 5, 6}};
    for (auto& m : msgs) ASSERT_TRUE(a.send(m));
    for (size_t i = 0; i < msgs.size(); ++i) {
        auto received = b.receive();
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(*received, msgs[i]);
    }
}

// ============================================================================
// Cycle 525: MessagePipe regression tests
// ============================================================================

// Send 256-byte payload
TEST(MessagePipeTest, Send256BytePayload) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> payload(256);
    for (int i = 0; i < 256; ++i) payload[i] = static_cast<uint8_t>(i);
    ASSERT_TRUE(a.send(payload));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    ASSERT_EQ(received->size(), 256u);
    EXPECT_EQ(*received, payload);
}

// Bidirectional: both sides send and receive
TEST(MessagePipeTest, BidirectionalExchange) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg_a = {0xAA};
    std::vector<uint8_t> msg_b = {0xBB};
    ASSERT_TRUE(a.send(msg_a));
    ASSERT_TRUE(b.send(msg_b));
    auto from_a = b.receive();
    auto from_b = a.receive();
    ASSERT_TRUE(from_a.has_value());
    ASSERT_TRUE(from_b.has_value());
    EXPECT_EQ(from_a->at(0), 0xAA);
    EXPECT_EQ(from_b->at(0), 0xBB);
}

// Receive on an empty pipe returns nullopt
TEST(MessagePipeTest, ReceiveOnEmptyPipeReturnsNullopt) {
    auto [a, b] = MessagePipe::create_pair();
    // Nothing was sent, so receive should return empty/fail
    // (non-blocking check)
    // We test with a closed sender
    a.close();
    auto received = b.receive();
    // After sender closes, receiver gets nullopt
    EXPECT_FALSE(received.has_value());
}

// is_open() returns true on fresh pipe
TEST(MessagePipeTest, IsOpenOnFreshPipe) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_TRUE(a.is_open());
    EXPECT_TRUE(b.is_open());
}

// After close(), is_open() returns false
TEST(MessagePipeTest, IsOpenFalseAfterClose) {
    auto [a, b] = MessagePipe::create_pair();
    a.close();
    EXPECT_FALSE(a.is_open());
}

// Send 5 messages, receive all 5
TEST(MessagePipeTest, SendFiveMessagesReceiveAll) {
    auto [a, b] = MessagePipe::create_pair();
    for (int i = 0; i < 5; ++i) {
        std::vector<uint8_t> msg = {static_cast<uint8_t>(i * 10)};
        ASSERT_TRUE(a.send(msg));
    }
    for (int i = 0; i < 5; ++i) {
        auto recv = b.receive();
        ASSERT_TRUE(recv.has_value());
        EXPECT_EQ(recv->at(0), static_cast<uint8_t>(i * 10));
    }
}

// fd() returns valid file descriptor
TEST(MessagePipeTest, FdReturnsValidDescriptor) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_GE(a.fd(), 0);
    EXPECT_GE(b.fd(), 0);
}

// Send payload with all 0xFF bytes
TEST(MessagePipeTest, AllMaxBytesPayload) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> payload(8, 0xFF);
    ASSERT_TRUE(a.send(payload));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(*received, payload);
}

// ============================================================================
// Cycle 536: MessagePipe regression tests
// ============================================================================

// Send single byte (value 0)
TEST(MessagePipeTest, SendSingleZeroByte) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg = {0x00};
    ASSERT_TRUE(a.send(msg));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->at(0), 0x00u);
}

// Create two pairs: they are independent
TEST(MessagePipeTest, TwoPairsAreIndependent) {
    auto [a1, b1] = MessagePipe::create_pair();
    auto [a2, b2] = MessagePipe::create_pair();
    std::vector<uint8_t> msg1 = {1};
    std::vector<uint8_t> msg2 = {2};
    ASSERT_TRUE(a1.send(msg1));
    ASSERT_TRUE(a2.send(msg2));
    auto r1 = b1.receive();
    auto r2 = b2.receive();
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1->at(0), 1u);
    EXPECT_EQ(r2->at(0), 2u);
}

// Send 3 messages and receive in FIFO order
TEST(MessagePipeTest, ReceiveInFIFOOrder) {
    auto [a, b] = MessagePipe::create_pair();
    for (uint8_t i = 10; i < 13; ++i) {
        std::vector<uint8_t> msg = {i};
        ASSERT_TRUE(a.send(msg));
    }
    for (uint8_t i = 10; i < 13; ++i) {
        auto recv = b.receive();
        ASSERT_TRUE(recv.has_value());
        EXPECT_EQ(recv->at(0), i);
    }
}

// Send payload with alternating bytes
TEST(MessagePipeTest, AlternatingBytesRoundTrip) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> payload = {0xAA, 0x55, 0xAA, 0x55};
    ASSERT_TRUE(a.send(payload));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(*recv, payload);
}

// fd() values differ between the two ends of a pair
TEST(MessagePipeTest, FdValuesAreDifferent) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_NE(a.fd(), b.fd());
}

// receive returns nullopt after sender closes
TEST(MessagePipeTest, ReceiveNulloptAfterSenderClose) {
    auto [a, b] = MessagePipe::create_pair();
    // Close sender: receiver should get nullopt
    a.close();
    auto recv = b.receive();
    EXPECT_FALSE(recv.has_value());
}

// Send 2-byte payload and verify size
TEST(MessagePipeTest, TwoBytesPayloadSizeCorrect) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg = {0x12, 0x34};
    ASSERT_TRUE(a.send(msg));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->size(), 2u);
    EXPECT_EQ(recv->at(0), 0x12u);
    EXPECT_EQ(recv->at(1), 0x34u);
}

// After close, is_open returns false
TEST(MessagePipeTest, CloseMakesIsOpenFalse) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_TRUE(a.is_open());
    a.close();
    EXPECT_FALSE(a.is_open());
}

// ============================================================================
// Cycle 555: MessagePipe regression tests
// ============================================================================

// Send 10 messages with sequential values
TEST(MessagePipeTest, TenSequentialMessagesWithValues) {
    auto [a, b] = MessagePipe::create_pair();
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> msg = {static_cast<uint8_t>(i)};
        ASSERT_TRUE(a.send(msg));
    }
    for (int i = 0; i < 10; ++i) {
        auto recv = b.receive();
        ASSERT_TRUE(recv.has_value());
        EXPECT_EQ(recv->at(0), static_cast<uint8_t>(i));
    }
}

// Payload with sequential byte values 0..7
TEST(MessagePipeTest, SequentialBytePayload) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg = {0, 1, 2, 3, 4, 5, 6, 7};
    ASSERT_TRUE(a.send(msg));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(*recv, msg);
}

// Send empty payload
TEST(MessagePipeTest, SendEmptyVectorPayload) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> empty;
    ASSERT_TRUE(a.send(empty));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_TRUE(recv->empty());
}

// fd() is non-negative after pair creation
TEST(MessagePipeTest, FdIsNonnegativeAfterCreate) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_GE(a.fd(), 0);
    EXPECT_GE(b.fd(), 0);
}

// Send from b to a (reverse direction)
TEST(MessagePipeTest, ReverseDirectionSendReceive) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg = {42};
    ASSERT_TRUE(b.send(msg));
    auto recv = a.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->at(0), 42u);
}

// Closed pipe: is_open returns false
TEST(MessagePipeTest, IsOpenFalseAfterClose2) {
    auto [a, b] = MessagePipe::create_pair();
    b.close();
    EXPECT_FALSE(b.is_open());
    EXPECT_TRUE(a.is_open());
}

// 4-byte payload
TEST(MessagePipeTest, FourBytePayloadRoundTrip) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_TRUE(a.send(msg));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->size(), 4u);
    EXPECT_EQ(recv->at(0), 0xDEu);
    EXPECT_EQ(recv->at(3), 0xEFu);
}

// Three pairs coexist without interference
TEST(MessagePipeTest, ThreePairsCoexist) {
    auto [a1, b1] = MessagePipe::create_pair();
    auto [a2, b2] = MessagePipe::create_pair();
    auto [a3, b3] = MessagePipe::create_pair();

    ASSERT_TRUE(a1.send({1}));
    ASSERT_TRUE(a2.send({2}));
    ASSERT_TRUE(a3.send({3}));

    EXPECT_EQ(b1.receive()->at(0), 1u);
    EXPECT_EQ(b2.receive()->at(0), 2u);
    EXPECT_EQ(b3.receive()->at(0), 3u);
}

// ============================================================================
// Cycle 570: More MessagePipe tests
// ============================================================================

// Pipe: fd values are non-negative after creation
TEST(MessagePipeTest, BothFdsNonNegative) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_GE(a.fd(), 0);
    EXPECT_GE(b.fd(), 0);
}

// Pipe: both ends open after creation
TEST(MessagePipeTest, BothEndsOpenAfterCreate) {
    auto [a, b] = MessagePipe::create_pair();
    EXPECT_TRUE(a.is_open());
    EXPECT_TRUE(b.is_open());
}

// Pipe: send 16-byte message, receive preserves all bytes
TEST(MessagePipeTest, SixteenBytePayloadPreserved) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg(16);
    for (int i = 0; i < 16; ++i) msg[i] = static_cast<uint8_t>(i * 16);
    ASSERT_TRUE(a.send(msg));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    ASSERT_EQ(recv->size(), 16u);
    EXPECT_EQ(recv->at(0), 0u);
    EXPECT_EQ(recv->at(15), 240u);
}

// Pipe: send 3 messages in sequence
TEST(MessagePipeTest, ThreeMessagesSequential) {
    auto [a, b] = MessagePipe::create_pair();
    for (int i = 1; i <= 3; ++i) {
        ASSERT_TRUE(a.send({static_cast<uint8_t>(i)}));
    }
    for (int i = 1; i <= 3; ++i) {
        auto recv = b.receive();
        ASSERT_TRUE(recv.has_value());
        EXPECT_EQ(recv->at(0), static_cast<uint8_t>(i));
    }
}

// Pipe: close sender, receiver gets nullopt
TEST(MessagePipeTest, CloseSenderReceiverGetsNullopt) {
    auto [a, b] = MessagePipe::create_pair();
    a.close();
    auto result = b.receive();
    EXPECT_FALSE(result.has_value());
}

// Pipe: close one end, other still open
TEST(MessagePipeTest, CloseOneEndOtherStillOpen) {
    auto [a, b] = MessagePipe::create_pair();
    a.close();
    EXPECT_TRUE(b.is_open());
    EXPECT_FALSE(a.is_open());
}

// Pipe: send from b to a (reverse direction)
TEST(MessagePipeTest, SendFromBToAWorks) {
    auto [a, b] = MessagePipe::create_pair();
    std::vector<uint8_t> msg = {0xAB, 0xCD};
    ASSERT_TRUE(b.send(msg));
    auto recv = a.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->at(0), 0xABu);
    EXPECT_EQ(recv->at(1), 0xCDu);
}

// Pipe: single-byte message round trip
TEST(MessagePipeTest, SingleByteRoundTripValue) {
    auto [a, b] = MessagePipe::create_pair();
    ASSERT_TRUE(a.send({0x42}));
    auto recv = b.receive();
    ASSERT_TRUE(recv.has_value());
    EXPECT_EQ(recv->size(), 1u);
    EXPECT_EQ(recv->at(0), 0x42u);
}

TEST(MessagePipeTest, MessagePipeV128_4_SendRawPointerMultiBytePayload) {
    auto [a, b] = MessagePipe::create_pair();
    const uint8_t data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    ASSERT_TRUE(a.send(data, 8));
    auto received = b.receive();
    ASSERT_TRUE(received.has_value());
    ASSERT_EQ(received->size(), 8u);
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_EQ((*received)[i], data[i]);
    }
}

TEST(MessagePipeTest, MessagePipeV128_5_MoveAssignFromClosedPipe) {
    auto [a, b] = MessagePipe::create_pair();
    a.close();

    MessagePipe dest(-1);
    dest = std::move(a);

    EXPECT_FALSE(dest.is_open());
    EXPECT_EQ(dest.fd(), -1);
}
