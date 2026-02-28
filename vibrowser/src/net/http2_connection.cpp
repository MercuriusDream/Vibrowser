#include <clever/net/http2_connection.h>
#include <clever/net/response.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <algorithm>

namespace clever::net {

Http2Connection::Http2Connection(int fd, TlsSocket* tls_socket)
    : fd_(fd), tls_socket_(tls_socket), encoder_(), decoder_() {}

Http2Connection::~Http2Connection() {
    if (fd_ >= 0 && !tls_socket_) {
        close(fd_);
    }
}

bool Http2Connection::start() {
    if (preface_sent_) {
        return true;
    }

    static constexpr const char kConnectionPreface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    if (!write_all(reinterpret_cast<const uint8_t*>(kConnectionPreface), 24)) {
        return false;
    }

    if (!send_settings({})) {
        return false;
    }

    preface_sent_ = true;
    return true;
}

bool Http2Connection::write_all(const uint8_t* data, size_t len) {
    if (tls_socket_) {
        return tls_socket_->send(data, len);
    }

    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd_, data + written, len - written);
        if (n <= 0) {
            return false;
        }
        written += n;
    }
    return true;
}

bool Http2Connection::read_into_buffer() {
    uint8_t buf[16384];

    if (tls_socket_) {
        auto data = tls_socket_->recv();
        if (!data) {
            return false;
        }
        if (data->empty()) {
            return false;
        }
        recv_buffer_.insert(recv_buffer_.end(), data->begin(), data->end());
        return true;
    }

    ssize_t n = ::read(fd_, buf, sizeof(buf));
    if (n <= 0) {
        return false;
    }

    recv_buffer_.insert(recv_buffer_.end(), buf, buf + n);
    return true;
}

std::optional<Http2Connection::Frame> Http2Connection::try_parse_frame() {
    if (recv_buffer_.size() < 9) {
        return std::nullopt;
    }

    uint32_t length = (static_cast<uint32_t>(recv_buffer_[0]) << 16) |
                      (static_cast<uint32_t>(recv_buffer_[1]) << 8) |
                      static_cast<uint32_t>(recv_buffer_[2]);

    if (recv_buffer_.size() < 9 + length) {
        return std::nullopt;
    }

    uint8_t type = recv_buffer_[3];
    uint8_t flags = recv_buffer_[4];
    uint32_t stream_id = ((static_cast<uint32_t>(recv_buffer_[5]) << 24) |
                          (static_cast<uint32_t>(recv_buffer_[6]) << 16) |
                          (static_cast<uint32_t>(recv_buffer_[7]) << 8) |
                          static_cast<uint32_t>(recv_buffer_[8])) &
                         0x7FFFFFFF;

    Frame frame;
    frame.type = type;
    frame.flags = flags;
    frame.stream_id = stream_id;
    frame.payload.insert(frame.payload.end(), recv_buffer_.begin() + 9,
                         recv_buffer_.begin() + 9 + length);

    recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + 9 + length);

    return frame;
}

bool Http2Connection::send_frame(const Frame& frame) {
    std::lock_guard<std::mutex> lock(send_mutex_);

    uint32_t length = frame.payload.size();
    uint8_t header[9];

    header[0] = (length >> 16) & 0xFF;
    header[1] = (length >> 8) & 0xFF;
    header[2] = length & 0xFF;
    header[3] = frame.type;
    header[4] = frame.flags;
    header[5] = (frame.stream_id >> 24) & 0x7F;
    header[6] = (frame.stream_id >> 16) & 0xFF;
    header[7] = (frame.stream_id >> 8) & 0xFF;
    header[8] = frame.stream_id & 0xFF;

    if (!write_all(header, 9)) {
        return false;
    }

    if (!frame.payload.empty()) {
        if (!write_all(frame.payload.data(), frame.payload.size())) {
            return false;
        }
    }

    return true;
}

std::optional<Http2Connection::Frame> Http2Connection::recv_frame() {
    std::lock_guard<std::mutex> lock(recv_mutex_);

    while (true) {
        auto frame = try_parse_frame();
        if (frame) {
            return frame;
        }

        if (!read_into_buffer()) {
            return std::nullopt;
        }
    }
}

bool Http2Connection::send_headers(uint32_t stream_id, const HeaderMap& headers,
                                   bool end_stream) {
    std::lock_guard<std::mutex> lock(stream_mutex_);

    auto encoded = encoder_.encode_header_list(headers);

    Frame frame;
    frame.type = FRAME_TYPE_HEADERS;
    frame.flags = FLAG_END_HEADERS | (end_stream ? FLAG_END_STREAM : 0);
    frame.stream_id = stream_id;
    frame.payload = encoded;

    update_stream_state_after_send(stream_id, end_stream);
    return send_frame(frame);
}

bool Http2Connection::send_data(uint32_t stream_id, const std::vector<uint8_t>& data,
                                bool end_stream) {
    std::lock_guard<std::mutex> lock(stream_mutex_);

    Frame frame;
    frame.type = FRAME_TYPE_DATA;
    frame.flags = end_stream ? FLAG_END_STREAM : 0;
    frame.stream_id = stream_id;
    frame.payload = data;

    update_stream_state_after_send(stream_id, end_stream);
    return send_frame(frame);
}

bool Http2Connection::send_settings(
    const std::unordered_map<uint16_t, uint32_t>& settings) {
    std::vector<uint8_t> payload;

    std::unordered_map<uint16_t, uint32_t> default_settings;
    default_settings[SETTINGS_HEADER_TABLE_SIZE] = 4096;
    default_settings[SETTINGS_ENABLE_PUSH] = 1;
    default_settings[SETTINGS_MAX_CONCURRENT_STREAMS] = 100;
    default_settings[SETTINGS_INITIAL_WINDOW_SIZE] = kInitialWindowSize;

    for (const auto& [key, value] : settings.empty() ? default_settings : settings) {
        uint8_t buf[6];
        buf[0] = (key >> 8) & 0xFF;
        buf[1] = key & 0xFF;
        buf[2] = (value >> 24) & 0xFF;
        buf[3] = (value >> 16) & 0xFF;
        buf[4] = (value >> 8) & 0xFF;
        buf[5] = value & 0xFF;
        payload.insert(payload.end(), buf, buf + 6);
    }

    Frame frame;
    frame.type = FRAME_TYPE_SETTINGS;
    frame.flags = 0;
    frame.stream_id = 0;
    frame.payload = payload;

    return send_frame(frame);
}

bool Http2Connection::send_window_update(uint32_t stream_id, uint32_t increment) {
    uint8_t payload[4];
    payload[0] = (increment >> 24) & 0x7F;
    payload[1] = (increment >> 16) & 0xFF;
    payload[2] = (increment >> 8) & 0xFF;
    payload[3] = increment & 0xFF;

    Frame frame;
    frame.type = FRAME_TYPE_WINDOW_UPDATE;
    frame.flags = 0;
    frame.stream_id = stream_id;
    frame.payload.insert(frame.payload.end(), payload, payload + 4);

    return send_frame(frame);
}

bool Http2Connection::send_rst_stream(uint32_t stream_id, uint32_t error_code) {
    uint8_t payload[4];
    payload[0] = (error_code >> 24) & 0xFF;
    payload[1] = (error_code >> 16) & 0xFF;
    payload[2] = (error_code >> 8) & 0xFF;
    payload[3] = error_code & 0xFF;

    Frame frame;
    frame.type = FRAME_TYPE_RST_STREAM;
    frame.flags = 0;
    frame.stream_id = stream_id;
    frame.payload.insert(frame.payload.end(), payload, payload + 4);

    return send_frame(frame);
}

uint32_t Http2Connection::allocate_stream_id() {
    uint32_t id = next_stream_id_;
    next_stream_id_ += 2;
    return id;
}

void Http2Connection::update_stream_state_after_send(uint32_t stream_id, bool end_stream) {
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        streams_[stream_id].state = StreamState::Open;
        it = streams_.find(stream_id);
    }

    if (end_stream) {
        if (it->second.state == StreamState::Open) {
            it->second.state = StreamState::HalfClosedLocal;
        } else if (it->second.state == StreamState::HalfClosedRemote) {
            it->second.state = StreamState::Closed;
        }
    }
}

void Http2Connection::update_stream_state_after_recv(uint32_t stream_id, bool end_stream) {
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        streams_[stream_id].state = StreamState::Open;
        it = streams_.find(stream_id);
    }

    if (end_stream) {
        if (it->second.state == StreamState::Open) {
            it->second.state = StreamState::HalfClosedRemote;
        } else if (it->second.state == StreamState::HalfClosedLocal) {
            it->second.state = StreamState::Closed;
        }
    }
}

bool Http2Connection::stream_can_send_data(StreamState state) const {
    return state == StreamState::Open || state == StreamState::HalfClosedRemote;
}

bool Http2Connection::handle_settings(const Frame& frame) {
    if (frame.flags & FLAG_ACK) {
        return true;
    }

    for (size_t i = 0; i + 6 <= frame.payload.size(); i += 6) {
        uint16_t id = (static_cast<uint16_t>(frame.payload[i]) << 8) |
                      static_cast<uint16_t>(frame.payload[i + 1]);
        uint32_t value = (static_cast<uint32_t>(frame.payload[i + 2]) << 24) |
                         (static_cast<uint32_t>(frame.payload[i + 3]) << 16) |
                         (static_cast<uint32_t>(frame.payload[i + 4]) << 8) |
                         static_cast<uint32_t>(frame.payload[i + 5]);

        if (id == SETTINGS_HEADER_TABLE_SIZE) {
            encoder_.set_max_dynamic_table_size(value);
        } else if (id == SETTINGS_INITIAL_WINDOW_SIZE) {
            remote_initial_window_size_ = value;
        }
    }

    return send_settings({});
}

bool Http2Connection::handle_window_update(const Frame& frame) {
    if (frame.payload.size() < 4) {
        return false;
    }

    uint32_t increment = (static_cast<uint32_t>(frame.payload[0]) << 24) |
                         (static_cast<uint32_t>(frame.payload[1]) << 16) |
                         (static_cast<uint32_t>(frame.payload[2]) << 8) |
                         static_cast<uint32_t>(frame.payload[3]);
    increment &= 0x7FFFFFFF;

    if (frame.stream_id == 0) {
        connection_send_window_ += increment;
    } else {
        auto it = streams_.find(frame.stream_id);
        if (it != streams_.end()) {
            it->second.send_window += increment;
        }
    }

    return true;
}

bool Http2Connection::handle_rst_stream(const Frame& frame) {
    auto it = streams_.find(frame.stream_id);
    if (it != streams_.end()) {
        it->second.state = StreamState::Closed;
    }
    return true;
}

bool Http2Connection::handle_headers_or_continuation(const Frame& frame,
                                                     std::optional<Response>& out,
                                                     uint32_t target_stream_id) {
    if (frame.stream_id == 0) {
        return false;
    }

    if (frame.type == FRAME_TYPE_HEADERS) {
        continuation_header_block_ = frame.payload;
        continuation_expected_ = !(frame.flags & FLAG_END_HEADERS);
        continuation_stream_id_ = frame.stream_id;
    } else if (frame.type == FRAME_TYPE_CONTINUATION) {
        continuation_header_block_.insert(continuation_header_block_.end(),
                                         frame.payload.begin(), frame.payload.end());
        continuation_expected_ = !(frame.flags & FLAG_END_HEADERS);
    }

    if (!continuation_expected_) {
        auto it = streams_.find(frame.stream_id);
        if (it == streams_.end()) {
            streams_[frame.stream_id].state = StreamState::Open;
            it = streams_.find(frame.stream_id);
        }

        HeaderMap headers = decoder_.decode(continuation_header_block_.data(),
                                            continuation_header_block_.size());
        it->second.response_headers = headers;
        it->second.headers_received = true;

        if (frame.flags & FLAG_END_STREAM) {
            it->second.end_stream_received = true;
            update_stream_state_after_recv(frame.stream_id, true);
        }

        continuation_header_block_.clear();
        continuation_stream_id_ = 0;

        if (frame.stream_id == target_stream_id) {
            out = maybe_finalize_response_locked(frame.stream_id);
        }
    }

    return true;
}

bool Http2Connection::handle_data(const Frame& frame, std::optional<Response>& out,
                                  uint32_t target_stream_id) {
    if (frame.stream_id == 0) {
        return false;
    }

    auto it = streams_.find(frame.stream_id);
    if (it == streams_.end()) {
        return false;
    }

    it->second.response_body.insert(it->second.response_body.end(),
                                    frame.payload.begin(), frame.payload.end());

    if (frame.flags & FLAG_END_STREAM) {
        it->second.end_stream_received = true;
        update_stream_state_after_recv(frame.stream_id, true);

        if (frame.stream_id == target_stream_id) {
            out = maybe_finalize_response_locked(frame.stream_id);
        }
    }

    uint32_t increment = kWindowUpdateThreshold - it->second.recv_window;
    if (increment > 0) {
        it->second.recv_window += increment;
        send_window_update(frame.stream_id, increment);
    }

    return true;
}

std::optional<Response> Http2Connection::maybe_finalize_response_locked(
    uint32_t stream_id) {
    auto it = streams_.find(stream_id);
    if (it == streams_.end() || !it->second.headers_received ||
        !it->second.end_stream_received) {
        return std::nullopt;
    }

    Response response;
    response.url = it->second.request_url;
    response.headers = it->second.response_headers;
    response.body = it->second.response_body;

    if (auto status = it->second.response_headers.get(":status")) {
        try {
            response.status = static_cast<uint16_t>(std::stoi(*status));
        } catch (...) {
            response.status = 200;
        }
    }

    return response;
}

std::optional<Response> Http2Connection::make_http2_request(const Request& request) {
    std::lock_guard<std::mutex> lock(stream_mutex_);

    if (!ensure_started()) {
        return std::nullopt;
    }

    uint32_t stream_id = allocate_stream_id();

    HeaderMap headers = request.headers;
    headers.set(":method", method_to_string(request.method));
    headers.set(":scheme", request.use_tls ? "https" : "http");
    headers.set(":path", request.path.empty() ? "/" : request.path);
    if (!request.query.empty()) {
        std::string path = headers.get(":path").value_or("/");
        headers.set(":path", path + "?" + request.query);
    }
    headers.set(":authority", request.host);

    streams_[stream_id].request_url = request.url;

    bool has_body = !request.body.empty();
    if (!send_headers(stream_id, headers, !has_body)) {
        return std::nullopt;
    }

    if (has_body) {
        if (!send_data(stream_id, request.body, true)) {
            return std::nullopt;
        }
    }

    while (true) {
        auto frame = recv_frame();
        if (!frame) {
            return std::nullopt;
        }

        std::optional<Response> response;

        if (frame->type == FRAME_TYPE_HEADERS || frame->type == FRAME_TYPE_CONTINUATION) {
            if (!handle_headers_or_continuation(*frame, response, stream_id)) {
                return std::nullopt;
            }
        } else if (frame->type == FRAME_TYPE_DATA) {
            if (!handle_data(*frame, response, stream_id)) {
                return std::nullopt;
            }
        } else if (frame->type == FRAME_TYPE_SETTINGS) {
            if (!handle_settings(*frame)) {
                return std::nullopt;
            }
        } else if (frame->type == FRAME_TYPE_WINDOW_UPDATE) {
            if (!handle_window_update(*frame)) {
                return std::nullopt;
            }
        } else if (frame->type == FRAME_TYPE_RST_STREAM) {
            if (!handle_rst_stream(*frame)) {
                return std::nullopt;
            }
        }

        if (response) {
            return response;
        }
    }

    return std::nullopt;
}

bool Http2Connection::ensure_started() {
    return preface_sent_ || start();
}

} // namespace clever::net
