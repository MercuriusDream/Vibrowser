#include <clever/net/hpack.h>
#include <clever/net/http2_connection.h>
#include <clever/net/header_map.h>
#include <gtest/gtest.h>

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
