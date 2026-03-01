#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <clever/net/hpack.h>
#include <clever/net/request.h>
#include <clever/net/response.h>
#include <clever/net/tls_socket.h>

namespace clever::net {

class Http2Connection {
public:
    struct Frame {
        uint8_t type = 0;
        uint8_t flags = 0;
        uint32_t stream_id = 0;
        std::vector<uint8_t> payload;
    };

    static constexpr uint8_t FRAME_TYPE_DATA = 0x0;
    static constexpr uint8_t FRAME_TYPE_HEADERS = 0x1;
    static constexpr uint8_t FRAME_TYPE_RST_STREAM = 0x3;
    static constexpr uint8_t FRAME_TYPE_SETTINGS = 0x4;
    static constexpr uint8_t FRAME_TYPE_WINDOW_UPDATE = 0x8;
    static constexpr uint8_t FRAME_TYPE_CONTINUATION = 0x9;

    static constexpr uint8_t FLAG_ACK = 0x1;
    static constexpr uint8_t FLAG_END_STREAM = 0x1;
    static constexpr uint8_t FLAG_END_HEADERS = 0x4;
    static constexpr uint8_t FLAG_PADDED = 0x8;
    static constexpr uint8_t FLAG_PRIORITY = 0x20;

    static constexpr uint16_t SETTINGS_HEADER_TABLE_SIZE = 0x1;
    static constexpr uint16_t SETTINGS_ENABLE_PUSH = 0x2;
    static constexpr uint16_t SETTINGS_MAX_CONCURRENT_STREAMS = 0x3;
    static constexpr uint16_t SETTINGS_INITIAL_WINDOW_SIZE = 0x4;

    explicit Http2Connection(int fd, TlsSocket* tls_socket = nullptr);
    ~Http2Connection();

    Http2Connection(const Http2Connection&) = delete;
    Http2Connection& operator=(const Http2Connection&) = delete;
    Http2Connection(Http2Connection&&) = delete;
    Http2Connection& operator=(Http2Connection&&) = delete;

    bool start();

    bool send_frame(const Frame& frame);
    std::optional<Frame> recv_frame();

    bool send_headers(uint32_t stream_id, const HeaderMap& headers, bool end_stream);
    bool send_data(uint32_t stream_id, const std::vector<uint8_t>& data, bool end_stream);
    bool send_settings(const std::unordered_map<uint16_t, uint32_t>& settings = {});
    bool send_window_update(uint32_t stream_id, uint32_t increment);
    bool send_rst_stream(uint32_t stream_id, uint32_t error_code);

    std::optional<Response> make_http2_request(const Request& request);

private:
    enum class StreamState {
        Idle,
        Open,
        HalfClosedLocal,
        HalfClosedRemote,
        Closed
    };

    struct StreamContext {
        StreamState state = StreamState::Idle;
        int64_t send_window = 65535;
        int64_t recv_window = 65535;
        HeaderMap response_headers;
        std::vector<uint8_t> response_body;
        bool headers_received = false;
        bool end_stream_received = false;
        std::string request_url;
    };

    static constexpr uint32_t kInitialWindowSize = 65535;
    static constexpr uint32_t kDefaultMaxFrameSize = 16384;
    static constexpr uint32_t kWindowUpdateThreshold = 32768;

    int fd_ = -1;
    TlsSocket* tls_socket_ = nullptr;

    std::mutex send_mutex_;
    std::mutex recv_mutex_;
    std::mutex stream_mutex_;

    std::vector<uint8_t> recv_buffer_;

    bool preface_sent_ = false;
    bool continuation_expected_ = false;
    uint32_t continuation_stream_id_ = 0;
    std::vector<uint8_t> continuation_header_block_;

    uint32_t next_stream_id_ = 1;
    uint32_t remote_initial_window_size_ = kInitialWindowSize;
    [[maybe_unused]] uint32_t local_initial_window_size_ = kInitialWindowSize;
    int64_t connection_send_window_ = kInitialWindowSize;
    [[maybe_unused]] int64_t connection_recv_window_ = kInitialWindowSize;
    [[maybe_unused]] uint32_t max_frame_size_ = kDefaultMaxFrameSize;

    HpackEncoder encoder_;
    HpackDecoder decoder_;

    std::unordered_map<uint32_t, StreamContext> streams_;

    bool ensure_started();
    uint32_t allocate_stream_id();
    bool write_all(const uint8_t* data, size_t len);
    bool read_into_buffer();
    std::optional<Frame> try_parse_frame();

    bool handle_settings(const Frame& frame);
    bool handle_window_update(const Frame& frame);
    bool handle_rst_stream(const Frame& frame);
    bool handle_headers_or_continuation(const Frame& frame, std::optional<Response>& out, uint32_t target_stream_id);
    bool handle_data(const Frame& frame, std::optional<Response>& out, uint32_t target_stream_id);
    std::optional<Response> maybe_finalize_response_locked(uint32_t stream_id);
    void update_stream_state_after_send(uint32_t stream_id, bool end_stream);
    void update_stream_state_after_recv(uint32_t stream_id, bool end_stream);
    bool stream_can_send_data(StreamState state) const;
};

} // namespace clever::net

