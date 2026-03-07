#define private public
#include <algorithm>
#include <clever/net/hpack.h>
#include <clever/net/http2_connection.h>
#include <clever/net/header_map.h>
#undef private
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace clever::net;

// ===========================================================================
// HPACK Tests
// ===========================================================================

TEST(HpackTest, HuffmanEncodeDecode) {
    std::string input = "Hello";
    auto encoded = hpack_huffman_encode(input);
    EXPECT_FALSE(encoded.empty());

    auto decoded = hpack_huffman_decode(encoded.data(), encoded.size());
    EXPECT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), input);
}

TEST(HpackTest, HuffmanEncodeEmptyString) {
    std::string input = "";
    auto encoded = hpack_huffman_encode(input);
    EXPECT_TRUE(encoded.empty());
}

TEST(HpackTest, HpackEncoderConstruction) {
    HpackEncoder encoder(4096);
    EXPECT_EQ(encoder.max_dynamic_table_size(), 4096u);
    EXPECT_EQ(encoder.dynamic_table_size(), 0u);
}

TEST(HpackTest, HpackEncoderSetMaxTableSize) {
    HpackEncoder encoder(4096);
    encoder.set_max_dynamic_table_size(8192);
    EXPECT_EQ(encoder.max_dynamic_table_size(), 8192u);
}

TEST(HpackTest, HpackDecoderConstruction) {
    HpackDecoder decoder(4096);
    EXPECT_EQ(decoder.max_dynamic_table_size(), 4096u);
    EXPECT_EQ(decoder.dynamic_table_size(), 0u);
}

TEST(HpackTest, StaticTableSize) {
    EXPECT_EQ(kHpackStaticTable.size(), 61u);
}

TEST(HpackTest, StaticTableFirstEntry) {
    auto entry = kHpackStaticTable[0];
    EXPECT_EQ(entry.name, ":authority");
    EXPECT_EQ(entry.value, "");
}

TEST(HpackTest, StaticTableStatusEntry) {
    auto entry = kHpackStaticTable[7]; // index 8
    EXPECT_EQ(entry.name, ":status");
    EXPECT_EQ(entry.value, "200");
}

// ===========================================================================
// Http2Connection Frame Tests
// ===========================================================================

TEST(Http2ConnectionTest, FrameConstants) {
    EXPECT_EQ(Http2Connection::FRAME_TYPE_DATA, 0x0);
    EXPECT_EQ(Http2Connection::FRAME_TYPE_HEADERS, 0x1);
    EXPECT_EQ(Http2Connection::FRAME_TYPE_RST_STREAM, 0x3);
    EXPECT_EQ(Http2Connection::FRAME_TYPE_SETTINGS, 0x4);
    EXPECT_EQ(Http2Connection::FRAME_TYPE_GOAWAY, 0x7);
    EXPECT_EQ(Http2Connection::FRAME_TYPE_WINDOW_UPDATE, 0x8);
    EXPECT_EQ(Http2Connection::FRAME_TYPE_CONTINUATION, 0x9);
}

TEST(Http2ConnectionTest, FlagConstants) {
    EXPECT_EQ(Http2Connection::FLAG_ACK, 0x1);
    EXPECT_EQ(Http2Connection::FLAG_END_STREAM, 0x1);
    EXPECT_EQ(Http2Connection::FLAG_END_HEADERS, 0x4);
    EXPECT_EQ(Http2Connection::FLAG_PADDED, 0x8);
    EXPECT_EQ(Http2Connection::FLAG_PRIORITY, 0x20);
}

TEST(Http2ConnectionTest, SettingsConstants) {
    EXPECT_EQ(Http2Connection::SETTINGS_HEADER_TABLE_SIZE, 0x1);
    EXPECT_EQ(Http2Connection::SETTINGS_ENABLE_PUSH, 0x2);
    EXPECT_EQ(Http2Connection::SETTINGS_MAX_CONCURRENT_STREAMS, 0x3);
    EXPECT_EQ(Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE, 0x4);
}

TEST(Http2ConnectionTest, FrameStructure) {
    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_DATA;
    frame.flags = Http2Connection::FLAG_END_STREAM;
    frame.stream_id = 1;
    frame.payload = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"

    EXPECT_EQ(frame.type, 0x0);
    EXPECT_EQ(frame.flags, 0x1);
    EXPECT_EQ(frame.stream_id, 1u);
    EXPECT_EQ(frame.payload.size(), 5u);
}

TEST(Http2ConnectionTest, SettingsFrameSendsAck) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        Http2Connection::Frame frame;
        frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        frame.stream_id = 0;
        frame.payload = {
            0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
            0x00, 0x01, 0x00, 0x00
        };

        ASSERT_TRUE(connection.handle_settings(frame));

        uint8_t raw_frame[9];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));
        EXPECT_EQ(raw_frame[0], 0);
        EXPECT_EQ(raw_frame[1], 0);
        EXPECT_EQ(raw_frame[2], 0);
        EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(raw_frame[4], Http2Connection::FLAG_ACK);
        EXPECT_EQ(raw_frame[5], 0);
        EXPECT_EQ(raw_frame[6], 0);
        EXPECT_EQ(raw_frame[7], 0);
        EXPECT_EQ(raw_frame[8], 0);
        EXPECT_EQ(connection.remote_initial_window_size_, 65536u);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, WindowUpdateAdjustsSendWindow) {
    Http2Connection connection(-1);
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 1024;

    Http2Connection::Frame connection_frame;
    connection_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    connection_frame.stream_id = 0;
    connection_frame.payload = {0x00, 0x00, 0x04, 0x00};

    ASSERT_TRUE(connection.handle_window_update(connection_frame));
    EXPECT_EQ(connection.connection_send_window_, 65535 + 1024);

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x00, 0x00, 0x00, 0x80};

    ASSERT_TRUE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.streams_[1].send_window, 1024 + 128);
}

TEST(Http2ConnectionTest, WindowUpdateZeroIncrementDoesNotMutateStateV2087) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 2048;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 1024;

    Http2Connection::Frame connection_frame;
    connection_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    connection_frame.stream_id = 0;
    connection_frame.payload = {0x00, 0x00, 0x00, 0x00};

    ASSERT_FALSE(connection.handle_window_update(connection_frame));
    EXPECT_EQ(connection.connection_send_window_, 2048);
    EXPECT_EQ(connection.streams_[1].send_window, 1024);

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x00, 0x00, 0x00, 0x00};

    ASSERT_FALSE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.connection_send_window_, 2048);
    EXPECT_EQ(connection.streams_[1].send_window, 1024);
}

TEST(Http2ConnectionTest, SettingsInitialWindowDeltaUpdatesExistingStreamsV2062) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.remote_initial_window_size_ = 65535;
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].send_window = 1000;
        connection.streams_[3].state = Http2Connection::StreamState::HalfClosedRemote;
        connection.streams_[3].send_window = -64;
        connection.streams_[5].state = Http2Connection::StreamState::Closed;
        connection.streams_[5].send_window = 777;

        Http2Connection::Frame frame;
        frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        frame.stream_id = 0;
        frame.payload = {
            0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
            0x00, 0x01, 0x01, 0x00
        };

        ASSERT_TRUE(connection.handle_settings(frame));
        EXPECT_EQ(connection.remote_initial_window_size_, 65792u);
        EXPECT_EQ(connection.streams_[1].send_window, 1257);
        EXPECT_EQ(connection.streams_[3].send_window, 193);
        EXPECT_EQ(connection.streams_[5].send_window, 777);

        uint8_t raw_frame[9];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));
        EXPECT_EQ(raw_frame[0], 0);
        EXPECT_EQ(raw_frame[1], 0);
        EXPECT_EQ(raw_frame[2], 0);
        EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(raw_frame[4], Http2Connection::FLAG_ACK);
        EXPECT_EQ(raw_frame[5], 0);
        EXPECT_EQ(raw_frame[6], 0);
        EXPECT_EQ(raw_frame[7], 0);
        EXPECT_EQ(raw_frame[8], 0);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, SettingsInitialWindowRejectsOverflowV2062) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.remote_initial_window_size_ = 65535;
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].send_window =
            Http2Connection::kMaxFlowControlWindowSize - 16;

        Http2Connection::Frame overflow_frame;
        overflow_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        overflow_frame.stream_id = 0;
        overflow_frame.payload = {
            0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
            0x00, 0x01, 0xFF, 0xFF
        };

        ASSERT_FALSE(connection.handle_settings(overflow_frame));
        EXPECT_EQ(connection.remote_initial_window_size_, 65535u);
        EXPECT_EQ(connection.streams_[1].send_window,
                  Http2Connection::kMaxFlowControlWindowSize - 16);

        uint8_t raw_frame[9];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);

        Http2Connection::Frame illegal_value_frame;
        illegal_value_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        illegal_value_frame.stream_id = 0;
        illegal_value_frame.payload = {
            0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
            0x80, 0x00, 0x00, 0x00
        };

        ASSERT_FALSE(connection.handle_settings(illegal_value_frame));
        EXPECT_EQ(connection.remote_initial_window_size_, 65535u);
        EXPECT_EQ(connection.streams_[1].send_window,
                  Http2Connection::kMaxFlowControlWindowSize - 16);

        Http2Connection::Frame ack_frame;
        ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        ack_frame.flags = Http2Connection::FLAG_ACK;
        ack_frame.stream_id = 0;
        ack_frame.payload = {0x00};

        EXPECT_FALSE(connection.handle_settings(ack_frame));
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, SettingsInvalidInitialWindowDoesNotPartiallyApplyV2065) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.remote_initial_window_size_ = 65535;
        connection.encoder_.set_max_dynamic_table_size(4096);
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].send_window = 2048;

        Http2Connection::Frame frame;
        frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        frame.stream_id = 0;
        frame.payload = {
            0x00, Http2Connection::SETTINGS_HEADER_TABLE_SIZE,
            0x00, 0x00, 0x20, 0x00,
            0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
            0x80, 0x00, 0x00, 0x00
        };

        ASSERT_FALSE(connection.handle_settings(frame));
        EXPECT_EQ(connection.encoder_.max_dynamic_table_size(), 4096u);
        EXPECT_EQ(connection.remote_initial_window_size_, 65535u);
        EXPECT_EQ(connection.streams_[1].send_window, 2048);

        uint8_t raw_frame[9];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, DataFramesDecrementReceiveWindowsWithoutEarlyWindowUpdate) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.streams_[1].state = Http2Connection::StreamState::Open;

        Http2Connection::Frame frame;
        frame.type = Http2Connection::FRAME_TYPE_DATA;
        frame.stream_id = 1;
        frame.payload.assign(Http2Connection::kWindowUpdateThreshold - 1, 0x41);

        std::optional<Response> response;
        ASSERT_TRUE(connection.handle_data(frame, response, 1));
        EXPECT_FALSE(response.has_value());
        EXPECT_EQ(connection.streams_[1].recv_window,
                  static_cast<int64_t>(Http2Connection::kInitialWindowSize) -
                      static_cast<int64_t>(frame.payload.size()));
        EXPECT_EQ(connection.connection_recv_window_,
                  static_cast<int64_t>(Http2Connection::kInitialWindowSize) -
                      static_cast<int64_t>(frame.payload.size()));

        uint8_t raw_frame[13];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, WindowUpdateOverflowDoesNotMutateStateV2065) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = Http2Connection::kMaxFlowControlWindowSize - 10;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = Http2Connection::kMaxFlowControlWindowSize - 12;

    Http2Connection::Frame connection_frame;
    connection_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    connection_frame.stream_id = 0;
    connection_frame.payload = {0x00, 0x00, 0x00, 0x20};

    ASSERT_FALSE(connection.handle_window_update(connection_frame));
    EXPECT_EQ(connection.connection_send_window_,
              Http2Connection::kMaxFlowControlWindowSize - 10);
    EXPECT_EQ(connection.streams_[1].send_window,
              Http2Connection::kMaxFlowControlWindowSize - 12);

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x00, 0x00, 0x00, 0x20};

    ASSERT_FALSE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.connection_send_window_,
              Http2Connection::kMaxFlowControlWindowSize - 10);
    EXPECT_EQ(connection.streams_[1].send_window,
              Http2Connection::kMaxFlowControlWindowSize - 12);
}

TEST(Http2ConnectionTest, SettingsAckPayloadDoesNotMutateStateV2068) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.remote_initial_window_size_ = 65535;
        connection.encoder_.set_max_dynamic_table_size(4096);
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].send_window = 3210;

        Http2Connection::Frame ack_frame;
        ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        ack_frame.flags = Http2Connection::FLAG_ACK;
        ack_frame.stream_id = 0;
        ack_frame.payload = {0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
                             0x00, 0x01, 0x00, 0x00};

        ASSERT_FALSE(connection.handle_settings(ack_frame));
        EXPECT_EQ(connection.remote_initial_window_size_, 65535u);
        EXPECT_EQ(connection.encoder_.max_dynamic_table_size(), 4096u);
        EXPECT_EQ(connection.streams_[1].send_window, 3210);

        uint8_t raw_frame[9];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, SettingsAckWithUnexpectedFlagsDoesNotMutateStateV2072) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.remote_initial_window_size_ = 65535;
        connection.encoder_.set_max_dynamic_table_size(4096);
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].send_window = 3210;

        Http2Connection::Frame ack_frame;
        ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        ack_frame.flags = Http2Connection::FLAG_ACK | Http2Connection::FLAG_END_HEADERS;
        ack_frame.stream_id = 0;

        ASSERT_FALSE(connection.handle_settings(ack_frame));
        EXPECT_EQ(connection.remote_initial_window_size_, 65535u);
        EXPECT_EQ(connection.encoder_.max_dynamic_table_size(), 4096u);
        EXPECT_EQ(connection.streams_[1].send_window, 3210);

        uint8_t raw_frame[9];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, RepeatedOutboundSettingsAckInOrderRebasesExistingStreamsV2099) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.decoder_.set_max_dynamic_table_size(4096);
        connection.local_initial_window_size_ = 65535;
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].recv_window = 50000;
        connection.streams_[3].state = Http2Connection::StreamState::HalfClosedLocal;
        connection.streams_[3].recv_window = 32000;
        connection.streams_[5].state = Http2Connection::StreamState::Closed;
        connection.streams_[5].recv_window = 777;

        std::unordered_map<uint16_t, uint32_t> settings = {
            {Http2Connection::SETTINGS_HEADER_TABLE_SIZE, 2048},
            {Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE, 32768},
        };
        ASSERT_TRUE(connection.send_settings(settings));
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
        ASSERT_TRUE(connection.pending_local_settings_.front().header_table_size.has_value());
        EXPECT_EQ(*connection.pending_local_settings_.front().header_table_size, 2048u);
        ASSERT_TRUE(connection.pending_local_settings_.front().initial_window_size.has_value());
        EXPECT_EQ(*connection.pending_local_settings_.front().initial_window_size, 32768u);

        EXPECT_EQ(connection.decoder_.max_dynamic_table_size(), 4096u);
        EXPECT_EQ(connection.local_initial_window_size_, 65535u);
        EXPECT_EQ(connection.streams_[1].recv_window, 50000);
        EXPECT_EQ(connection.streams_[3].recv_window, 32000);
        EXPECT_EQ(connection.streams_[5].recv_window, 777);

        uint8_t raw_frame[21];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));
        EXPECT_EQ(raw_frame[0], 0);
        EXPECT_EQ(raw_frame[1], 0);
        EXPECT_EQ(raw_frame[2], 12);
        EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(raw_frame[4], 0);
        EXPECT_EQ(raw_frame[5], 0);
        EXPECT_EQ(raw_frame[6], 0);
        EXPECT_EQ(raw_frame[7], 0);
        EXPECT_EQ(raw_frame[8], 0);

        ASSERT_TRUE(connection.send_settings(
            {{Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE, 30000}}));
        ASSERT_EQ(connection.pending_local_settings_.size(), 2u);
        EXPECT_FALSE(connection.pending_local_settings_[1].header_table_size.has_value());
        ASSERT_TRUE(connection.pending_local_settings_[1].initial_window_size.has_value());
        EXPECT_EQ(*connection.pending_local_settings_[1].initial_window_size, 30000u);

        uint8_t second_raw_frame[15];
        ASSERT_EQ(::read(fds[1], second_raw_frame, sizeof(second_raw_frame)),
                  static_cast<ssize_t>(sizeof(second_raw_frame)));
        EXPECT_EQ(second_raw_frame[0], 0);
        EXPECT_EQ(second_raw_frame[1], 0);
        EXPECT_EQ(second_raw_frame[2], 6);
        EXPECT_EQ(second_raw_frame[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(second_raw_frame[4], 0);

        Http2Connection::Frame ack_frame;
        ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        ack_frame.flags = Http2Connection::FLAG_ACK;
        ack_frame.stream_id = 0;

        ASSERT_TRUE(connection.handle_settings(ack_frame));
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
        EXPECT_EQ(connection.decoder_.max_dynamic_table_size(), 2048u);
        EXPECT_EQ(connection.local_initial_window_size_, 32768u);
        EXPECT_EQ(connection.streams_[1].recv_window, 17233);
        EXPECT_EQ(connection.streams_[3].recv_window, -767);
        EXPECT_EQ(connection.streams_[5].recv_window, 777);

        ASSERT_TRUE(connection.handle_settings(ack_frame));
        EXPECT_TRUE(connection.pending_local_settings_.empty());
        EXPECT_EQ(connection.decoder_.max_dynamic_table_size(), 2048u);
        EXPECT_EQ(connection.local_initial_window_size_, 30000u);
        EXPECT_EQ(connection.streams_[1].recv_window, 14465);
        EXPECT_EQ(connection.streams_[3].recv_window, -3535);
        EXPECT_EQ(connection.streams_[5].recv_window, 777);

        EXPECT_FALSE(connection.handle_settings(ack_frame));
        EXPECT_EQ(connection.decoder_.max_dynamic_table_size(), 2048u);
        EXPECT_EQ(connection.local_initial_window_size_, 30000u);

        ASSERT_TRUE(connection.send_settings(
            {{Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE, 65535}}));
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
        uint8_t resend_frame[15];
        ASSERT_EQ(::read(fds[1], resend_frame, sizeof(resend_frame)),
                  static_cast<ssize_t>(sizeof(resend_frame)));
        EXPECT_EQ(resend_frame[0], 0);
        EXPECT_EQ(resend_frame[1], 0);
        EXPECT_EQ(resend_frame[2], 6);
        EXPECT_EQ(resend_frame[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(resend_frame[4], 0);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, SettingsAckWithoutPendingStateFailsClosedV2098) {
    Http2Connection connection(-1);
    connection.decoder_.set_max_dynamic_table_size(4096);
    connection.local_initial_window_size_ = 65535;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].recv_window = 1234;

    Http2Connection::Frame ack_frame;
    ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
    ack_frame.flags = Http2Connection::FLAG_ACK;
    ack_frame.stream_id = 0;

    ASSERT_FALSE(connection.handle_settings(ack_frame));
    EXPECT_TRUE(connection.pending_local_settings_.empty());
    EXPECT_EQ(connection.decoder_.max_dynamic_table_size(), 4096u);
    EXPECT_EQ(connection.local_initial_window_size_, 65535u);
    EXPECT_EQ(connection.streams_[1].recv_window, 1234);
}

TEST(Http2ConnectionTest, SettingsAckOverflowKeepsPendingEntryQueuedV2099) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.decoder_.set_max_dynamic_table_size(4096);
        connection.local_initial_window_size_ = 60000;
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].recv_window =
            Http2Connection::kMaxFlowControlWindowSize - 4;

        ASSERT_TRUE(connection.send_settings({
            {Http2Connection::SETTINGS_HEADER_TABLE_SIZE, 1024},
            {Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
             static_cast<uint32_t>(Http2Connection::kMaxFlowControlWindowSize)},
        }));
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);

        uint8_t raw_frame[21];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));

        Http2Connection::Frame ack_frame;
        ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        ack_frame.flags = Http2Connection::FLAG_ACK;
        ack_frame.stream_id = 0;

        ASSERT_FALSE(connection.handle_settings(ack_frame));
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
        EXPECT_EQ(connection.decoder_.max_dynamic_table_size(), 4096u);
        EXPECT_EQ(connection.local_initial_window_size_, 60000u);
        EXPECT_EQ(connection.streams_[1].recv_window,
                  Http2Connection::kMaxFlowControlWindowSize - 4);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, WindowUpdateOverflowDoesNotTouchClosedStreamV2068) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.streams_[1].state = Http2Connection::StreamState::Closed;
    connection.streams_[1].send_window = Http2Connection::kMaxFlowControlWindowSize - 4;
    connection.streams_[3].state = Http2Connection::StreamState::Open;
    connection.streams_[3].send_window = 2048;

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x7F, 0xFF, 0xFF, 0xFF};

    ASSERT_FALSE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
    EXPECT_EQ(connection.streams_[3].send_window, 2048);
}

TEST(Http2ConnectionTest, WindowUpdateRetiresClosedStreamFlowControlStateV2096) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.streams_[1].state = Http2Connection::StreamState::Closed;
    connection.streams_[1].send_window = 1234;
    connection.streams_[1].recv_window = 5678;

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x00, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
}

TEST(Http2ConnectionTest,
     WindowUpdateRejectedForHalfClosedLocalStreamButConnectionWindowStillAdvancesV2095) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.streams_[1].state = Http2Connection::StreamState::HalfClosedLocal;
    connection.streams_[1].send_window = 2048;

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x00, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.streams_[1].send_window, 2048);

    Http2Connection::Frame connection_frame;
    connection_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    connection_frame.stream_id = 0;
    connection_frame.payload = {0x00, 0x00, 0x02, 0x00};

    ASSERT_TRUE(connection.handle_window_update(connection_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096 + 512);
    EXPECT_EQ(connection.streams_[1].send_window, 2048);
}

TEST(Http2ConnectionTest, WindowUpdateReservedIncrementBitDoesNotMutateStateV2073) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 2048;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 1024;

    Http2Connection::Frame connection_frame;
    connection_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    connection_frame.stream_id = 0;
    connection_frame.payload = {0x80, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(connection_frame));
    EXPECT_EQ(connection.connection_send_window_, 2048);
    EXPECT_EQ(connection.streams_[1].send_window, 1024);

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x80, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(stream_frame));
    EXPECT_EQ(connection.connection_send_window_, 2048);
    EXPECT_EQ(connection.streams_[1].send_window, 1024);
}

TEST(Http2ConnectionTest, WindowUpdateInvalidIncrementOnUnknownStreamFailsClosedV2089) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 2048;

    Http2Connection::Frame zero_increment_frame;
    zero_increment_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    zero_increment_frame.stream_id = 9;
    zero_increment_frame.payload = {0x00, 0x00, 0x00, 0x00};

    ASSERT_FALSE(connection.handle_window_update(zero_increment_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.streams_[1].send_window, 2048);
    EXPECT_EQ(connection.streams_.count(9), 0u);

    Http2Connection::Frame overflow_frame;
    overflow_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    overflow_frame.stream_id = 9;
    overflow_frame.payload = {0x80, 0x00, 0x00, 0x01};

    ASSERT_FALSE(connection.handle_window_update(overflow_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.streams_[1].send_window, 2048);
    EXPECT_EQ(connection.streams_.count(9), 0u);
}

TEST(Http2ConnectionTest, SendWindowUpdateRejectsInvalidIncrementV2087) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);

        EXPECT_FALSE(connection.send_window_update(0, 0));
        EXPECT_FALSE(connection.send_window_update(1, 0x80000000u));

        uint8_t raw_frame[13];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, RstStreamRetiresFlowControlStateV2096) {
    Http2Connection connection(-1);
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 2048;
    connection.streams_[1].recv_window = 1024;

    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
    frame.stream_id = 1;
    frame.payload = {0x00, 0x00, 0x00, 0x08};

    ASSERT_TRUE(connection.handle_rst_stream(frame));
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
}

TEST(Http2ConnectionTest, RstStreamClearsPartialResponseStateV2097) {
    Http2Connection connection(-1);
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 2048;
    connection.streams_[1].recv_window = 1024;
    connection.streams_[1].headers_received = true;
    connection.streams_[1].end_stream_received = false;
    connection.streams_[1].response_headers.set(":status", "200");
    connection.streams_[1].response_body = {'p', 'a', 'r', 't'};

    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
    frame.stream_id = 1;
    frame.payload = {0x00, 0x00, 0x00, 0x08};

    ASSERT_TRUE(connection.handle_rst_stream(frame));
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
}

TEST(Http2ConnectionTest, LateFramesAfterRstStreamConsumeConnectionCreditV2097) {
    Http2Connection connection(-1);
    connection.connection_recv_window_ = 60000;

    Http2Connection::Frame rst_frame;
    rst_frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
    rst_frame.stream_id = 7;
    rst_frame.payload = {0x00, 0x00, 0x00, 0x08};

    ASSERT_TRUE(connection.handle_rst_stream(rst_frame));
    EXPECT_EQ(connection.streams_.count(7), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(7), 1u);

    HeaderMap headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");

    Http2Connection::Frame headers_frame;
    headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    headers_frame.flags = Http2Connection::FLAG_END_HEADERS;
    headers_frame.stream_id = 7;
    headers_frame.payload = connection.encoder_.encode_header_list(headers);

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_headers_or_continuation(headers_frame, response, 7));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_EQ(connection.streams_.count(7), 0u);

    Http2Connection::Frame data_frame;
    data_frame.type = Http2Connection::FRAME_TYPE_DATA;
    data_frame.stream_id = 7;
    data_frame.payload.assign(16, 0x41);

    ASSERT_FALSE(connection.handle_data(data_frame, response, 7));
    EXPECT_FALSE(response.has_value());
    EXPECT_EQ(connection.connection_recv_window_, 60000 - 16);
    EXPECT_EQ(connection.streams_.count(7), 0u);

    Http2Connection::Frame window_update_frame;
    window_update_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    window_update_frame.stream_id = 7;
    window_update_frame.payload = {0x00, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(window_update_frame));
    EXPECT_EQ(connection.streams_.count(7), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(7), 1u);
}

TEST(Http2ConnectionTest, RstStreamClearsPendingContinuationBookkeepingV2100) {
    Http2Connection connection(-1);
    connection.continuation_expected_ = true;
    connection.continuation_stream_id_ = 9;
    connection.continuation_end_stream_ = true;
    connection.continuation_header_block_ = {0x82, 0x84};
    connection.streams_[9].state = Http2Connection::StreamState::Open;
    connection.streams_[9].send_window = 1024;
    connection.streams_[9].recv_window = 2048;

    Http2Connection::Frame rst_frame;
    rst_frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
    rst_frame.stream_id = 9;
    rst_frame.payload = {0x00, 0x00, 0x00, 0x08};

    ASSERT_TRUE(connection.handle_rst_stream(rst_frame));
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_TRUE(connection.continuation_header_block_.empty());
    EXPECT_EQ(connection.streams_.count(9), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(9), 1u);
}

TEST(Http2ConnectionTest, LocalRstStreamRetiresStreamAndRejectsLateFramesV2104) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.connection_send_window_ = 4096;
        connection.connection_recv_window_ = 60000;
        connection.continuation_expected_ = true;
        connection.continuation_stream_id_ = 11;
        connection.continuation_end_stream_ = true;
        connection.continuation_header_block_ = {0x82, 0x84};
        connection.streams_[11].state = Http2Connection::StreamState::Open;
        connection.streams_[11].send_window = 2048;
        connection.streams_[11].recv_window = 1024;
        connection.streams_[11].request_url = "https://example.test/local-rst";

        ASSERT_TRUE(connection.send_rst_stream(11, 0x8));
        EXPECT_FALSE(connection.continuation_expected_);
        EXPECT_EQ(connection.continuation_stream_id_, 0u);
        EXPECT_FALSE(connection.continuation_end_stream_);
        EXPECT_TRUE(connection.continuation_header_block_.empty());
        EXPECT_EQ(connection.streams_.count(11), 0u);
        EXPECT_EQ(connection.closed_stream_ids_.count(11), 1u);

        uint8_t raw_frame[13];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));
        EXPECT_EQ(raw_frame[0], 0);
        EXPECT_EQ(raw_frame[1], 0);
        EXPECT_EQ(raw_frame[2], 4);
        EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_RST_STREAM);
        EXPECT_EQ(raw_frame[4], 0);
        EXPECT_EQ(raw_frame[5], 0);
        EXPECT_EQ(raw_frame[6], 0);
        EXPECT_EQ(raw_frame[7], 0);
        EXPECT_EQ(raw_frame[8], 11);

        Http2Connection::Frame window_update_frame;
        window_update_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
        window_update_frame.stream_id = 11;
        window_update_frame.payload = {0x00, 0x00, 0x00, 0x80};
        ASSERT_FALSE(connection.handle_window_update(window_update_frame));
        EXPECT_EQ(connection.connection_send_window_, 4096);
        EXPECT_EQ(connection.streams_.count(11), 0u);

        HeaderMap headers;
        headers.set(":status", "200");
        headers.set("content-type", "text/plain");

        std::optional<Response> response;

        Http2Connection::Frame headers_frame;
        headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
        headers_frame.flags = Http2Connection::FLAG_END_HEADERS;
        headers_frame.stream_id = 11;
        headers_frame.payload = connection.encoder_.encode_header_list(headers);
        ASSERT_FALSE(connection.handle_headers_or_continuation(headers_frame, response, 11));
        EXPECT_FALSE(response.has_value());
        EXPECT_EQ(connection.streams_.count(11), 0u);

        Http2Connection::Frame continuation_frame;
        continuation_frame.type = Http2Connection::FRAME_TYPE_CONTINUATION;
        continuation_frame.flags = Http2Connection::FLAG_END_HEADERS;
        continuation_frame.stream_id = 11;
        continuation_frame.payload = {0x88};
        ASSERT_FALSE(connection.handle_headers_or_continuation(continuation_frame, response, 11));
        EXPECT_FALSE(response.has_value());
        EXPECT_EQ(connection.streams_.count(11), 0u);

        Http2Connection::Frame data_frame;
        data_frame.type = Http2Connection::FRAME_TYPE_DATA;
        data_frame.stream_id = 11;
        data_frame.payload.assign(12, 0x41);
        ASSERT_FALSE(connection.handle_data(data_frame, response, 11));
        EXPECT_FALSE(response.has_value());
        EXPECT_EQ(connection.connection_recv_window_, 60000 - 12);
        EXPECT_EQ(connection.streams_.count(11), 0u);
        EXPECT_EQ(connection.closed_stream_ids_.count(11), 1u);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, RepeatedRstStreamAfterRetirementFailsClosedV2102) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.connection_recv_window_ = 60000;
    connection.streams_[7].state = Http2Connection::StreamState::Open;
    connection.streams_[7].send_window = 2048;
    connection.streams_[7].recv_window = 1024;

    Http2Connection::Frame rst_frame;
    rst_frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
    rst_frame.stream_id = 7;
    rst_frame.payload = {0x00, 0x00, 0x00, 0x08};

    ASSERT_TRUE(connection.handle_rst_stream(rst_frame));
    ASSERT_FALSE(connection.handle_rst_stream(rst_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.connection_recv_window_, 60000);
    EXPECT_EQ(connection.streams_.count(7), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(7), 1u);
}

TEST(Http2ConnectionTest, RetiredStreamStateUpdatesDoNotReviveWindowsV2102) {
    Http2Connection connection(-1);
    connection.remote_initial_window_size_ = 32000;
    connection.local_initial_window_size_ = 24000;
    connection.streams_[1].state = Http2Connection::StreamState::HalfClosedLocal;
    connection.streams_[1].send_window = 1234;
    connection.streams_[1].recv_window = 5678;

    connection.update_stream_state_after_recv(1, true);
    ASSERT_EQ(connection.closed_stream_ids_.count(1), 1u);
    ASSERT_EQ(connection.streams_[1].state, Http2Connection::StreamState::Closed);

    connection.streams_.erase(1);
    connection.update_stream_state_after_send(1, false);
    connection.update_stream_state_after_recv(1, false);

    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
}

TEST(Http2ConnectionTest, SendPathsRejectRetiredStreamsWithoutWritingV2102) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.streams_[1].state = Http2Connection::StreamState::Open;
        connection.streams_[1].send_window = 2048;
        connection.streams_[1].recv_window = 1024;

        Http2Connection::Frame rst_frame;
        rst_frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
        rst_frame.stream_id = 1;
        rst_frame.payload = {0x00, 0x00, 0x00, 0x08};

        ASSERT_TRUE(connection.handle_rst_stream(rst_frame));
        EXPECT_EQ(connection.streams_.count(1), 0u);
        EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);

        HeaderMap headers;
        headers.set(":method", "GET");
        headers.set(":scheme", "https");
        headers.set(":path", "/");
        headers.set(":authority", "example.test");

        EXPECT_FALSE(connection.send_headers(1, headers, true));
        EXPECT_FALSE(connection.send_data(1, {'b', 'o', 'd', 'y'}, true));

        uint8_t raw_frame[32];
        EXPECT_EQ(::recv(fds[1], raw_frame, sizeof(raw_frame), MSG_DONTWAIT), -1);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, LateContinuationAfterRstStreamIsRejectedWithoutStateMutationV2101) {
    Http2Connection connection(-1);
    connection.connection_recv_window_ = 60000;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].request_url = "https://example.test/reset";

    HeaderMap headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");
    auto encoded = connection.encoder_.encode_header_list(headers);
    ASSERT_GT(encoded.size(), 1u);

    const size_t split = std::max<size_t>(1, encoded.size() / 2);

    Http2Connection::Frame initial_frame;
    initial_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    initial_frame.flags = Http2Connection::FLAG_END_STREAM;
    initial_frame.stream_id = 1;
    initial_frame.payload.assign(encoded.begin(), encoded.begin() + split);

    std::optional<Response> response;
    ASSERT_TRUE(connection.handle_headers_or_continuation(initial_frame, response, 1));
    ASSERT_TRUE(connection.continuation_expected_);
    ASSERT_TRUE(connection.continuation_end_stream_);

    Http2Connection::Frame rst_frame;
    rst_frame.type = Http2Connection::FRAME_TYPE_RST_STREAM;
    rst_frame.stream_id = 1;
    rst_frame.payload = {0x00, 0x00, 0x00, 0x08};

    ASSERT_TRUE(connection.handle_rst_stream(rst_frame));
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);

    Http2Connection::Frame continuation_frame;
    continuation_frame.type = Http2Connection::FRAME_TYPE_CONTINUATION;
    continuation_frame.flags = Http2Connection::FLAG_END_HEADERS;
    continuation_frame.stream_id = 1;
    continuation_frame.payload.assign(encoded.begin() + split, encoded.end());

    ASSERT_FALSE(connection.handle_headers_or_continuation(continuation_frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_EQ(connection.connection_recv_window_, 60000);
    EXPECT_EQ(connection.streams_.count(1), 0u);
}

TEST(Http2ConnectionTest, RetiredStreamRejectionClearsStaleContinuationStateV2104) {
    Http2Connection connection(-1);
    connection.closed_stream_ids_.insert(13);
    connection.continuation_expected_ = true;
    connection.continuation_stream_id_ = 13;
    connection.continuation_end_stream_ = true;
    connection.continuation_header_block_ = {0x82, 0x84};

    HeaderMap headers;
    headers.set(":status", "200");

    Http2Connection::Frame headers_frame;
    headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    headers_frame.flags = Http2Connection::FLAG_END_HEADERS;
    headers_frame.stream_id = 13;
    headers_frame.payload = connection.encoder_.encode_header_list(headers);

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_headers_or_continuation(headers_frame, response, 13));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_TRUE(connection.continuation_header_block_.empty());
    EXPECT_EQ(connection.streams_.count(13), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(13), 1u);
}

TEST(Http2ConnectionTest, EndStreamTransitionRetiresFlowControlStateV2096) {
    Http2Connection connection(-1);
    connection.streams_[1].state = Http2Connection::StreamState::HalfClosedLocal;
    connection.streams_[1].send_window = 2048;
    connection.streams_[1].recv_window = 1024;

    connection.update_stream_state_after_recv(1, true);

    EXPECT_EQ(connection.streams_[1].state, Http2Connection::StreamState::Closed);
    EXPECT_EQ(connection.streams_[1].send_window, 0);
    EXPECT_EQ(connection.streams_[1].recv_window, 0);
}

TEST(Http2ConnectionTest, NewStreamsUseRebasedInitialWindowsAfterSettingsTransitionsV2099) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);

        Http2Connection::Frame remote_settings_frame;
        remote_settings_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        remote_settings_frame.stream_id = 0;
        remote_settings_frame.payload = {
            0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
            0x00, 0x00, 0x7D, 0x00
        };

        ASSERT_TRUE(connection.handle_settings(remote_settings_frame));
        EXPECT_EQ(connection.remote_initial_window_size_, 32000u);

        uint8_t remote_ack[9];
        ASSERT_EQ(::read(fds[1], remote_ack, sizeof(remote_ack)),
                  static_cast<ssize_t>(sizeof(remote_ack)));
        EXPECT_EQ(remote_ack[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(remote_ack[4], Http2Connection::FLAG_ACK);

        ASSERT_TRUE(connection.send_settings(
            {{Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE, 24000}}));
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);

        uint8_t local_settings[15];
        ASSERT_EQ(::read(fds[1], local_settings, sizeof(local_settings)),
                  static_cast<ssize_t>(sizeof(local_settings)));
        EXPECT_EQ(local_settings[3], Http2Connection::FRAME_TYPE_SETTINGS);
        EXPECT_EQ(local_settings[4], 0);

        Http2Connection::Frame local_ack_frame;
        local_ack_frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
        local_ack_frame.flags = Http2Connection::FLAG_ACK;
        local_ack_frame.stream_id = 0;

        ASSERT_TRUE(connection.handle_settings(local_ack_frame));
        EXPECT_TRUE(connection.pending_local_settings_.empty());
        EXPECT_EQ(connection.local_initial_window_size_, 24000u);

        connection.update_stream_state_after_send(1, false);
        ASSERT_EQ(connection.streams_.count(1), 1u);
        EXPECT_EQ(connection.streams_[1].state, Http2Connection::StreamState::Open);
        EXPECT_EQ(connection.streams_[1].send_window, 32000);
        EXPECT_EQ(connection.streams_[1].recv_window, 24000);

        connection.update_stream_state_after_recv(3, false);
        ASSERT_EQ(connection.streams_.count(3), 1u);
        EXPECT_EQ(connection.streams_[3].state, Http2Connection::StreamState::Open);
        EXPECT_EQ(connection.streams_[3].send_window, 32000);
        EXPECT_EQ(connection.streams_[3].recv_window, 24000);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, ContinuationStreamMismatchIsRejectedV2066) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.connection_recv_window_ = 60000;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    Http2Connection::PendingLocalSettings pending_settings;
    pending_settings.header_table_size = 1024;
    pending_settings.initial_window_size = 32768;
    connection.pending_local_settings_.push_back(pending_settings);

    HeaderMap headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");
    headers.set("cache-control", "max-age=60");
    auto encoded = connection.encoder_.encode_header_list(headers);
    ASSERT_GT(encoded.size(), 1u);

    const size_t split = encoded.size() / 2;

    Http2Connection::Frame headers_frame;
    headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    headers_frame.stream_id = 1;
    headers_frame.payload.assign(encoded.begin(), encoded.begin() + split);

    std::optional<Response> response;
    ASSERT_TRUE(connection.handle_headers_or_continuation(headers_frame, response, 1));
    ASSERT_TRUE(connection.continuation_expected_);
    ASSERT_EQ(connection.continuation_stream_id_, 1u);

    const auto decoder_dynamic_table_size = connection.decoder_.dynamic_table_size();

    Http2Connection::Frame continuation_frame;
    continuation_frame.type = Http2Connection::FRAME_TYPE_CONTINUATION;
    continuation_frame.flags = Http2Connection::FLAG_END_HEADERS;
    continuation_frame.stream_id = 3;
    continuation_frame.payload.assign(encoded.begin() + split, encoded.end());

    ASSERT_FALSE(connection.handle_headers_or_continuation(continuation_frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_TRUE(connection.continuation_header_block_.empty());
    EXPECT_EQ(connection.decoder_.dynamic_table_size(), decoder_dynamic_table_size);
    EXPECT_FALSE(connection.streams_[1].headers_received);
    EXPECT_EQ(connection.connection_send_window_, 4096);
    EXPECT_EQ(connection.connection_recv_window_, 60000);
    ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
    ASSERT_TRUE(connection.pending_local_settings_.front().header_table_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().header_table_size, 1024u);
    ASSERT_TRUE(connection.pending_local_settings_.front().initial_window_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().initial_window_size, 32768u);
}

TEST(Http2ConnectionTest, InterleavedHeadersDuringContinuationFailsCleanlyV2066) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 8192;
    connection.connection_recv_window_ = 61000;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[3].state = Http2Connection::StreamState::Open;
    connection.streams_[3].response_headers.set(":status", "418");
    Http2Connection::PendingLocalSettings pending_settings;
    pending_settings.header_table_size = 2048;
    pending_settings.initial_window_size = 48000;
    connection.pending_local_settings_.push_back(pending_settings);

    HeaderMap initial_headers;
    initial_headers.set(":status", "200");
    initial_headers.set("content-type", "text/plain");
    initial_headers.set("etag", "\"alpha\"");
    auto initial_encoded = connection.encoder_.encode_header_list(initial_headers);
    ASSERT_GT(initial_encoded.size(), 1u);

    Http2Connection::Frame initial_frame;
    initial_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    initial_frame.stream_id = 1;
    initial_frame.payload.assign(initial_encoded.begin(),
                                 initial_encoded.begin() + (initial_encoded.size() / 2));

    std::optional<Response> response;
    ASSERT_TRUE(connection.handle_headers_or_continuation(initial_frame, response, 1));
    ASSERT_TRUE(connection.continuation_expected_);

    const auto decoder_dynamic_table_size = connection.decoder_.dynamic_table_size();
    const auto prior_stream3_headers = connection.streams_[3].response_headers;

    HeaderMap interleaved_headers;
    interleaved_headers.set(":status", "204");
    auto interleaved_encoded = connection.encoder_.encode_header_list(interleaved_headers);

    Http2Connection::Frame interleaved_frame;
    interleaved_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    interleaved_frame.flags = Http2Connection::FLAG_END_HEADERS;
    interleaved_frame.stream_id = 3;
    interleaved_frame.payload = interleaved_encoded;

    ASSERT_FALSE(connection.handle_headers_or_continuation(interleaved_frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_TRUE(connection.continuation_header_block_.empty());
    EXPECT_EQ(connection.decoder_.dynamic_table_size(), decoder_dynamic_table_size);
    EXPECT_EQ(connection.streams_[3].response_headers.get(":status"),
              prior_stream3_headers.get(":status"));
    EXPECT_FALSE(connection.streams_[3].headers_received);
    EXPECT_EQ(connection.connection_send_window_, 8192);
    EXPECT_EQ(connection.connection_recv_window_, 61000);
    ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
    ASSERT_TRUE(connection.pending_local_settings_.front().header_table_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().header_table_size, 2048u);
    ASSERT_TRUE(connection.pending_local_settings_.front().initial_window_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().initial_window_size, 48000u);
}

TEST(Http2ConnectionTest, ClosedStreamHeadersAreRejectedWithoutStateMutationV2095) {
    Http2Connection connection(-1);
    connection.streams_[1].state = Http2Connection::StreamState::Closed;
    connection.streams_[1].headers_received = true;
    connection.streams_[1].end_stream_received = true;
    connection.streams_[1].send_window = 0;
    connection.streams_[1].recv_window = 0;
    connection.streams_[1].response_headers.set(":status", "204");
    connection.streams_[1].response_body = {'o', 'k'};
    connection.connection_recv_window_ = 60000;

    HeaderMap headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");

    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    frame.flags = Http2Connection::FLAG_END_HEADERS;
    frame.stream_id = 1;
    frame.payload = connection.encoder_.encode_header_list(headers);

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_headers_or_continuation(frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);

    Http2Connection::Frame data_frame;
    data_frame.type = Http2Connection::FRAME_TYPE_DATA;
    data_frame.stream_id = 1;
    data_frame.payload.assign(32, 0x41);

    ASSERT_FALSE(connection.handle_data(data_frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_EQ(connection.connection_recv_window_, 60000 - 32);
    EXPECT_EQ(connection.streams_.count(1), 0u);

    Http2Connection::Frame window_update_frame;
    window_update_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    window_update_frame.stream_id = 1;
    window_update_frame.payload = {0x00, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(window_update_frame));
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
}

TEST(Http2ConnectionTest, FinalizedResponseDropsStreamBookkeepingAndRejectsLateFramesV2100) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.connection_recv_window_ = 60000;
    connection.streams_[1].state = Http2Connection::StreamState::Closed;
    connection.streams_[1].send_window = 0;
    connection.streams_[1].recv_window = 0;
    connection.streams_[1].headers_received = true;
    connection.streams_[1].end_stream_received = true;
    connection.streams_[1].request_url = "https://example.test/";
    connection.streams_[1].response_headers.set(":status", "204");
    connection.streams_[1].response_body = {'o', 'k'};

    auto response = connection.maybe_finalize_response_locked(1);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status, 204);
    EXPECT_EQ(response->url, "https://example.test/");
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);

    Http2Connection::Frame window_update_frame;
    window_update_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    window_update_frame.stream_id = 1;
    window_update_frame.payload = {0x00, 0x00, 0x00, 0x80};
    ASSERT_FALSE(connection.handle_window_update(window_update_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);

    HeaderMap late_headers;
    late_headers.set(":status", "200");
    Http2Connection::Frame headers_frame;
    headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    headers_frame.flags = Http2Connection::FLAG_END_HEADERS;
    headers_frame.stream_id = 1;
    headers_frame.payload = connection.encoder_.encode_header_list(late_headers);

    std::optional<Response> late_response;
    ASSERT_FALSE(connection.handle_headers_or_continuation(headers_frame, late_response, 1));
    EXPECT_FALSE(late_response.has_value());
    EXPECT_EQ(connection.streams_.count(1), 0u);

    Http2Connection::Frame data_frame;
    data_frame.type = Http2Connection::FRAME_TYPE_DATA;
    data_frame.stream_id = 1;
    data_frame.payload.assign(12, 0x41);

    ASSERT_FALSE(connection.handle_data(data_frame, late_response, 1));
    EXPECT_EQ(connection.connection_recv_window_, 60000 - 12);
    EXPECT_EQ(connection.streams_.count(1), 0u);
}

TEST(Http2ConnectionTest,
     EndStreamHeadersSplitAcrossContinuationFinalizeAndRejectLateFramesV2101) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4096;
    connection.connection_recv_window_ = 60000;
    connection.streams_[1].state = Http2Connection::StreamState::Open;
    connection.streams_[1].send_window = 2048;
    connection.streams_[1].recv_window = Http2Connection::kInitialWindowSize;
    connection.streams_[1].request_url = "https://example.test/continued";

    HeaderMap headers;
    headers.set(":status", "204");
    headers.set("etag", "\"split\"");
    auto encoded = connection.encoder_.encode_header_list(headers);
    ASSERT_GT(encoded.size(), 1u);

    const size_t split = std::max<size_t>(1, encoded.size() / 2);

    Http2Connection::Frame headers_frame;
    headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    headers_frame.flags = Http2Connection::FLAG_END_STREAM;
    headers_frame.stream_id = 1;
    headers_frame.payload.assign(encoded.begin(), encoded.begin() + split);

    std::optional<Response> response;
    ASSERT_TRUE(connection.handle_headers_or_continuation(headers_frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_TRUE(connection.continuation_expected_);
    EXPECT_TRUE(connection.continuation_end_stream_);

    Http2Connection::Frame continuation_frame;
    continuation_frame.type = Http2Connection::FRAME_TYPE_CONTINUATION;
    continuation_frame.flags = Http2Connection::FLAG_END_HEADERS;
    continuation_frame.stream_id = 1;
    continuation_frame.payload.assign(encoded.begin() + split, encoded.end());

    ASSERT_TRUE(connection.handle_headers_or_continuation(continuation_frame, response, 1));
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status, 204);
    EXPECT_EQ(response->url, "https://example.test/continued");
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);

    Http2Connection::Frame data_frame;
    data_frame.type = Http2Connection::FRAME_TYPE_DATA;
    data_frame.stream_id = 1;
    data_frame.payload.assign(16, 0x41);

    ASSERT_FALSE(connection.handle_data(data_frame, response, 1));
    EXPECT_EQ(connection.connection_recv_window_, 60000 - 16);
    EXPECT_EQ(connection.streams_.count(1), 0u);

    Http2Connection::Frame window_update_frame;
    window_update_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    window_update_frame.stream_id = 1;
    window_update_frame.payload = {0x00, 0x00, 0x00, 0x80};

    ASSERT_FALSE(connection.handle_window_update(window_update_frame));
    EXPECT_EQ(connection.connection_send_window_, 4096);
}

TEST(Http2ConnectionTest,
     RetiredContinuationFragmentFailsClosedWithoutTouchingSettingsOrFlowControlV2105) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 7777;
    connection.connection_recv_window_ = 55555;
    connection.closed_stream_ids_.insert(17);
    Http2Connection::PendingLocalSettings pending_settings;
    pending_settings.header_table_size = 1024;
    pending_settings.initial_window_size = 32768;
    connection.pending_local_settings_.push_back(pending_settings);
    connection.streams_[3].state = Http2Connection::StreamState::Open;
    connection.streams_[3].send_window = 2048;
    connection.streams_[3].recv_window = 4096;

    Http2Connection::Frame continuation_frame;
    continuation_frame.type = Http2Connection::FRAME_TYPE_CONTINUATION;
    continuation_frame.flags = Http2Connection::FLAG_END_HEADERS;
    continuation_frame.stream_id = 17;
    continuation_frame.payload = {0x88};

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_headers_or_continuation(continuation_frame, response, 3));
    EXPECT_FALSE(response.has_value());
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_TRUE(connection.continuation_header_block_.empty());
    EXPECT_EQ(connection.connection_send_window_, 7777);
    EXPECT_EQ(connection.connection_recv_window_, 55555);
    ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
    ASSERT_TRUE(connection.pending_local_settings_.front().header_table_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().header_table_size, 1024u);
    ASSERT_TRUE(connection.pending_local_settings_.front().initial_window_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().initial_window_size, 32768u);
    EXPECT_EQ(connection.streams_[3].send_window, 2048);
    EXPECT_EQ(connection.streams_[3].recv_window, 4096);
    EXPECT_EQ(connection.streams_.count(17), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(17), 1u);
}

TEST(Http2ConnectionTest,
     GoAwayRejectsHigherStreamHeadersWithoutClearingUnrelatedContinuationV2106) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 7777;
    connection.connection_recv_window_ = 55555;
    connection.continuation_expected_ = true;
    connection.continuation_stream_id_ = 3;
    connection.continuation_end_stream_ = true;
    connection.continuation_header_block_ = {0x82, 0x84};
    Http2Connection::PendingLocalSettings pending_settings;
    pending_settings.header_table_size = 1024;
    pending_settings.initial_window_size = 32768;
    connection.pending_local_settings_.push_back(pending_settings);
    connection.streams_[3].state = Http2Connection::StreamState::Open;
    connection.streams_[3].send_window = 2048;
    connection.streams_[3].recv_window = 4096;

    Http2Connection::Frame goaway_frame;
    goaway_frame.type = Http2Connection::FRAME_TYPE_GOAWAY;
    goaway_frame.stream_id = 0;
    goaway_frame.payload = {0x00, 0x00, 0x00, 0x03,
                            0x00, 0x00, 0x00, 0x00};

    ASSERT_TRUE(connection.handle_goaway(goaway_frame));
    EXPECT_TRUE(connection.goaway_received_);
    EXPECT_EQ(connection.goaway_last_stream_id_, 3u);
    EXPECT_TRUE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 3u);
    EXPECT_EQ(connection.continuation_header_block_,
              (std::vector<uint8_t>{0x82, 0x84}));

    HeaderMap headers;
    headers.set(":status", "200");
    headers.set("content-type", "text/plain");

    Http2Connection::Frame headers_frame;
    headers_frame.type = Http2Connection::FRAME_TYPE_HEADERS;
    headers_frame.flags = Http2Connection::FLAG_END_HEADERS;
    headers_frame.stream_id = 5;
    headers_frame.payload = connection.encoder_.encode_header_list(headers);

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_headers_or_continuation(headers_frame, response, 3));
    EXPECT_FALSE(response.has_value());
    EXPECT_TRUE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 3u);
    EXPECT_TRUE(connection.continuation_end_stream_);
    EXPECT_EQ(connection.continuation_header_block_,
              (std::vector<uint8_t>{0x82, 0x84}));
    EXPECT_EQ(connection.connection_send_window_, 7777);
    EXPECT_EQ(connection.connection_recv_window_, 55555);
    ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
    ASSERT_TRUE(connection.pending_local_settings_.front().header_table_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().header_table_size, 1024u);
    ASSERT_TRUE(connection.pending_local_settings_.front().initial_window_size.has_value());
    EXPECT_EQ(*connection.pending_local_settings_.front().initial_window_size, 32768u);
    EXPECT_EQ(connection.streams_[3].send_window, 2048);
    EXPECT_EQ(connection.streams_[3].recv_window, 4096);
    EXPECT_EQ(connection.streams_.count(5), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(5), 1u);
}

TEST(Http2ConnectionTest, GoAwayClearsOnlyRetiredContinuationStateV2106) {
    Http2Connection connection(-1);
    connection.connection_send_window_ = 4321;
    connection.connection_recv_window_ = 54321;
    connection.continuation_expected_ = true;
    connection.continuation_stream_id_ = 5;
    connection.continuation_end_stream_ = true;
    connection.continuation_header_block_ = {0x82, 0x84};
    connection.streams_[3].state = Http2Connection::StreamState::Open;
    connection.streams_[3].send_window = 2222;
    connection.streams_[3].recv_window = 3333;
    connection.streams_[5].state = Http2Connection::StreamState::Open;
    connection.streams_[5].send_window = 4444;
    connection.streams_[5].recv_window = 5555;

    Http2Connection::Frame goaway_frame;
    goaway_frame.type = Http2Connection::FRAME_TYPE_GOAWAY;
    goaway_frame.stream_id = 0;
    goaway_frame.payload = {0x00, 0x00, 0x00, 0x03,
                            0x00, 0x00, 0x00, 0x00};

    ASSERT_TRUE(connection.handle_goaway(goaway_frame));
    EXPECT_TRUE(connection.goaway_received_);
    EXPECT_EQ(connection.goaway_last_stream_id_, 3u);
    EXPECT_FALSE(connection.continuation_expected_);
    EXPECT_EQ(connection.continuation_stream_id_, 0u);
    EXPECT_FALSE(connection.continuation_end_stream_);
    EXPECT_TRUE(connection.continuation_header_block_.empty());
    EXPECT_EQ(connection.connection_send_window_, 4321);
    EXPECT_EQ(connection.connection_recv_window_, 54321);
    EXPECT_EQ(connection.streams_[3].send_window, 2222);
    EXPECT_EQ(connection.streams_[3].recv_window, 3333);
    EXPECT_EQ(connection.streams_[5].send_window, 4444);
    EXPECT_EQ(connection.streams_[5].recv_window, 5555);
}

TEST(Http2ConnectionTest,
     GoAwayRetiredDataConsumesConnectionCreditOnlyAndPreservesSettingsV2106) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.connection_recv_window_ =
            Http2Connection::kWindowUpdateThreshold + 1;
        Http2Connection::PendingLocalSettings pending_settings;
        pending_settings.header_table_size = 1024;
        pending_settings.initial_window_size = 32768;
        connection.pending_local_settings_.push_back(pending_settings);
        connection.streams_[3].state = Http2Connection::StreamState::Open;
        connection.streams_[3].send_window = 2048;
        connection.streams_[3].recv_window = 4096;

        Http2Connection::Frame goaway_frame;
        goaway_frame.type = Http2Connection::FRAME_TYPE_GOAWAY;
        goaway_frame.stream_id = 0;
        goaway_frame.payload = {0x00, 0x00, 0x00, 0x03,
                                0x00, 0x00, 0x00, 0x00};

        ASSERT_TRUE(connection.handle_goaway(goaway_frame));

        Http2Connection::Frame data_frame;
        data_frame.type = Http2Connection::FRAME_TYPE_DATA;
        data_frame.stream_id = 5;
        data_frame.payload.assign(4, 0x41);

        std::optional<Response> response;
        ASSERT_FALSE(connection.handle_data(data_frame, response, 3));
        EXPECT_FALSE(response.has_value());
        EXPECT_EQ(connection.connection_recv_window_,
                  static_cast<int64_t>(Http2Connection::kInitialWindowSize));
        EXPECT_EQ(connection.streams_[3].recv_window, 4096);
        EXPECT_EQ(connection.streams_[3].send_window, 2048);
        ASSERT_EQ(connection.pending_local_settings_.size(), 1u);
        ASSERT_TRUE(connection.pending_local_settings_.front().header_table_size.has_value());
        EXPECT_EQ(*connection.pending_local_settings_.front().header_table_size, 1024u);
        ASSERT_TRUE(connection.pending_local_settings_.front().initial_window_size.has_value());
        EXPECT_EQ(*connection.pending_local_settings_.front().initial_window_size, 32768u);
        EXPECT_EQ(connection.streams_.count(5), 0u);
        EXPECT_EQ(connection.closed_stream_ids_.count(5), 1u);

        uint8_t raw_frame[13];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));
        EXPECT_EQ(raw_frame[0], 0);
        EXPECT_EQ(raw_frame[1], 0);
        EXPECT_EQ(raw_frame[2], 4);
        EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_WINDOW_UPDATE);
        EXPECT_EQ(raw_frame[4], 0);
        EXPECT_EQ(raw_frame[5], 0);
        EXPECT_EQ(raw_frame[6], 0);
        EXPECT_EQ(raw_frame[7], 0);
        EXPECT_EQ(raw_frame[8], 0);

        uint32_t increment = ((static_cast<uint32_t>(raw_frame[9]) << 24) |
                              (static_cast<uint32_t>(raw_frame[10]) << 16) |
                              (static_cast<uint32_t>(raw_frame[11]) << 8) |
                              static_cast<uint32_t>(raw_frame[12])) &
                             0x7FFFFFFF;
        EXPECT_EQ(increment, Http2Connection::kWindowUpdateThreshold + 2);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, DataFramesEmitThresholdedWindowUpdatesAfterConsumption) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.streams_[1].state = Http2Connection::StreamState::Open;

        Http2Connection::Frame first;
        first.type = Http2Connection::FRAME_TYPE_DATA;
        first.stream_id = 1;
        first.payload.assign(Http2Connection::kWindowUpdateThreshold - 1, 0x41);

        std::optional<Response> response;
        ASSERT_TRUE(connection.handle_data(first, response, 1));

        Http2Connection::Frame second;
        second.type = Http2Connection::FRAME_TYPE_DATA;
        second.stream_id = 1;
        second.payload = {0x42, 0x43};

        ASSERT_TRUE(connection.handle_data(second, response, 1));
        EXPECT_EQ(connection.streams_[1].recv_window,
                  static_cast<int64_t>(Http2Connection::kInitialWindowSize));
        EXPECT_EQ(connection.connection_recv_window_,
                  static_cast<int64_t>(Http2Connection::kInitialWindowSize));

        uint8_t raw_frames[26];
        ASSERT_EQ(::read(fds[1], raw_frames, sizeof(raw_frames)),
                  static_cast<ssize_t>(sizeof(raw_frames)));

        bool saw_connection_update = false;
        bool saw_stream_update = false;
        uint32_t expected_increment = Http2Connection::kWindowUpdateThreshold + 1;
        for (size_t offset = 0; offset < sizeof(raw_frames); offset += 13) {
            const uint8_t* raw_frame = raw_frames + offset;
            uint32_t length = (static_cast<uint32_t>(raw_frame[0]) << 16) |
                              (static_cast<uint32_t>(raw_frame[1]) << 8) |
                              static_cast<uint32_t>(raw_frame[2]);
            uint32_t stream_id = ((static_cast<uint32_t>(raw_frame[5]) << 24) |
                                  (static_cast<uint32_t>(raw_frame[6]) << 16) |
                                  (static_cast<uint32_t>(raw_frame[7]) << 8) |
                                  static_cast<uint32_t>(raw_frame[8])) &
                                 0x7FFFFFFF;
            uint32_t increment = ((static_cast<uint32_t>(raw_frame[9]) << 24) |
                                  (static_cast<uint32_t>(raw_frame[10]) << 16) |
                                  (static_cast<uint32_t>(raw_frame[11]) << 8) |
                                  static_cast<uint32_t>(raw_frame[12])) &
                                 0x7FFFFFFF;

            EXPECT_EQ(length, 4u);
            EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_WINDOW_UPDATE);
            EXPECT_EQ(raw_frame[4], 0);
            EXPECT_EQ(increment, expected_increment);

            if (stream_id == 0) {
                saw_connection_update = true;
            } else if (stream_id == 1) {
                saw_stream_update = true;
            }
        }

        EXPECT_TRUE(saw_connection_update);
        EXPECT_TRUE(saw_stream_update);
    }

    close(fds[1]);
}

TEST(Http2ConnectionTest, HalfClosedStreamDataAfterRemoteEndDoesNotConsumeCreditV2095) {
    Http2Connection connection(-1);
    connection.connection_recv_window_ = 60000;
    connection.streams_[1].state = Http2Connection::StreamState::HalfClosedRemote;
    connection.streams_[1].recv_window = 50000;
    connection.streams_[1].response_body = {'o', 'k'};

    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_DATA;
    frame.stream_id = 1;
    frame.payload.assign(32, 0x41);

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_data(frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_EQ(connection.connection_recv_window_, 60000);
    EXPECT_EQ(connection.streams_[1].recv_window, 50000);
    EXPECT_EQ(connection.streams_[1].response_body,
              (std::vector<uint8_t>{'o', 'k'}));
}

TEST(Http2ConnectionTest, ClosedStreamDataConsumesConnectionCreditOnlyV2095) {
    Http2Connection connection(-1);
    connection.connection_recv_window_ = 60000;
    connection.streams_[1].state = Http2Connection::StreamState::Closed;
    connection.streams_[1].recv_window = 50000;
    connection.streams_[1].response_body = {'o', 'k'};

    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_DATA;
    frame.stream_id = 1;
    frame.payload.assign(32, 0x41);

    std::optional<Response> response;
    ASSERT_FALSE(connection.handle_data(frame, response, 1));
    EXPECT_FALSE(response.has_value());
    EXPECT_EQ(connection.connection_recv_window_, 60000 - 32);
    EXPECT_EQ(connection.streams_.count(1), 0u);
    EXPECT_EQ(connection.closed_stream_ids_.count(1), 1u);
}

TEST(Http2ConnectionTest, LateRetiredDataOnlyRefreshesConnectionWindowV2103) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    {
        Http2Connection connection(fds[0]);
        connection.connection_recv_window_ =
            Http2Connection::kWindowUpdateThreshold + 1;
        connection.closed_stream_ids_.insert(11);

        Http2Connection::Frame frame;
        frame.type = Http2Connection::FRAME_TYPE_DATA;
        frame.stream_id = 11;
        frame.payload.assign(4, 0x41);

        std::optional<Response> response;
        ASSERT_FALSE(connection.handle_data(frame, response, 11));
        EXPECT_FALSE(response.has_value());
        EXPECT_EQ(connection.connection_recv_window_,
                  static_cast<int64_t>(Http2Connection::kInitialWindowSize));
        EXPECT_EQ(connection.streams_.count(11), 0u);
        EXPECT_EQ(connection.closed_stream_ids_.count(11), 1u);

        uint8_t raw_frame[13];
        ASSERT_EQ(::read(fds[1], raw_frame, sizeof(raw_frame)),
                  static_cast<ssize_t>(sizeof(raw_frame)));
        EXPECT_EQ(raw_frame[0], 0);
        EXPECT_EQ(raw_frame[1], 0);
        EXPECT_EQ(raw_frame[2], 4);
        EXPECT_EQ(raw_frame[3], Http2Connection::FRAME_TYPE_WINDOW_UPDATE);
        EXPECT_EQ(raw_frame[4], 0);
        EXPECT_EQ(raw_frame[5], 0);
        EXPECT_EQ(raw_frame[6], 0);
        EXPECT_EQ(raw_frame[7], 0);
        EXPECT_EQ(raw_frame[8], 0);

        uint32_t increment = ((static_cast<uint32_t>(raw_frame[9]) << 24) |
                              (static_cast<uint32_t>(raw_frame[10]) << 16) |
                              (static_cast<uint32_t>(raw_frame[11]) << 8) |
                              static_cast<uint32_t>(raw_frame[12])) &
                             0x7FFFFFFF;
        EXPECT_EQ(increment, Http2Connection::kWindowUpdateThreshold + 2);
    }

    close(fds[1]);
}

// ===========================================================================
// Integration Tests
// ===========================================================================

TEST(HpackIntegrationTest, EncodeDecodeRoundTrip) {
    HeaderMap original;
    original.set("content-type", "text/html");
    original.set("content-length", "42");
    original.set("cache-control", "max-age=3600");

    HpackEncoder encoder;
    auto encoded = encoder.encode_header_list(original);
    EXPECT_FALSE(encoded.empty());

    HpackDecoder decoder;
    auto decoded = decoder.decode(encoded.data(), encoded.size());

    EXPECT_EQ(decoded.get("content-type").value_or(""), "text/html");
    EXPECT_EQ(decoded.get("content-length").value_or(""), "42");
    EXPECT_EQ(decoded.get("cache-control").value_or(""), "max-age=3600");
}

TEST(HpackIntegrationTest, MultipleRoundTrips) {
    HpackEncoder encoder;
    HpackDecoder decoder;

    HeaderMap headers1;
    headers1.set("user-agent", "Mozilla/5.0");
    auto enc1 = encoder.encode_header_list(headers1);

    HeaderMap headers2;
    headers2.set("accept", "text/html");
    auto enc2 = encoder.encode_header_list(headers2);

    EXPECT_FALSE(enc1.empty());
    EXPECT_FALSE(enc2.empty());

    auto dec1 = decoder.decode(enc1.data(), enc1.size());
    EXPECT_EQ(dec1.get("user-agent").value_or(""), "Mozilla/5.0");
}
