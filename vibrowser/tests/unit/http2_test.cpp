#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include <clever/net/http2_connection.h>
#include <clever/net/header_map.h>
#undef private
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <unistd.h>

using namespace clever::net;

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

TEST(Http2ConnectionTest, NonAckSettingsRespondsWithAckOnlyFrame) {
    int sockets[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);

    Http2Connection conn(sockets[0]);
    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
    frame.stream_id = 0;
    frame.payload = {
        0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
        0x00, 0x01, 0x00, 0x00,
    };

    ASSERT_TRUE(conn.handle_settings(frame));

    uint8_t header[9];
    ASSERT_EQ(::read(sockets[1], header, sizeof(header)), static_cast<ssize_t>(sizeof(header)));
    EXPECT_EQ(header[0], 0);
    EXPECT_EQ(header[1], 0);
    EXPECT_EQ(header[2], 0);
    EXPECT_EQ(header[3], Http2Connection::FRAME_TYPE_SETTINGS);
    EXPECT_EQ(header[4], Http2Connection::FLAG_ACK);
    EXPECT_EQ(header[5], 0);
    EXPECT_EQ(header[6], 0);
    EXPECT_EQ(header[7], 0);
    EXPECT_EQ(header[8], 0);

    uint8_t extra_byte = 0;
    EXPECT_EQ(::recv(sockets[1], &extra_byte, 1, MSG_DONTWAIT), -1);

    close(sockets[1]);
}

TEST(Http2ConnectionTest, InitialWindowSizeSettingsPropagateToExistingStreams) {
    int sockets[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0);
    Http2Connection conn(sockets[0]);
    conn.streams_[1].send_window = 65535;
    conn.streams_[3].send_window = 1024;

    Http2Connection::Frame frame;
    frame.type = Http2Connection::FRAME_TYPE_SETTINGS;
    frame.stream_id = 0;
    frame.payload = {
        0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
        0x00, 0x00, 0xFF, 0xFF,
    };

    ASSERT_TRUE(conn.handle_settings(frame));
    EXPECT_EQ(conn.remote_initial_window_size_, 65535u);
    EXPECT_EQ(conn.streams_[1].send_window, 65535);
    EXPECT_EQ(conn.streams_[3].send_window, 1024);

    uint8_t ack_frame_header[9];
    ASSERT_EQ(::read(sockets[1], ack_frame_header, sizeof(ack_frame_header)),
              static_cast<ssize_t>(sizeof(ack_frame_header)));

    frame.payload = {
        0x00, Http2Connection::SETTINGS_INITIAL_WINDOW_SIZE,
        0x00, 0x01, 0x00, 0x00,
    };

    ASSERT_TRUE(conn.handle_settings(frame));
    EXPECT_EQ(conn.remote_initial_window_size_, 65536u);
    EXPECT_EQ(conn.streams_[1].send_window, 65536);
    EXPECT_EQ(conn.streams_[3].send_window, 1025);

    close(sockets[1]);
}

TEST(Http2ConnectionTest, WindowUpdateIncrementZeroFailsClosed) {
    Http2Connection conn(-1);
    conn.connection_send_window_ = 111;
    conn.streams_[1].send_window = 222;

    Http2Connection::Frame connection_frame;
    connection_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    connection_frame.stream_id = 0;
    connection_frame.payload = {0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(conn.handle_window_update(connection_frame));
    EXPECT_EQ(conn.connection_send_window_, 111);

    Http2Connection::Frame stream_frame;
    stream_frame.type = Http2Connection::FRAME_TYPE_WINDOW_UPDATE;
    stream_frame.stream_id = 1;
    stream_frame.payload = {0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(conn.handle_window_update(stream_frame));
    EXPECT_EQ(conn.streams_[1].send_window, 222);
}
