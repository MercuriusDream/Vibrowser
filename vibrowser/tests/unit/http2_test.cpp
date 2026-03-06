#define private public
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
