#include <clever/ipc/message_channel.h>

#include <utility>

namespace clever::ipc {

MessageChannel::MessageChannel(MessagePipe pipe)
    : pipe_(std::move(pipe)) {}

bool MessageChannel::send(const Message& msg) {
    // Wire format: type(4) + request_id(4) + payload_len(4) + payload(N)
    // All multi-byte integers in big-endian (handled by Serializer).
    Serializer s;
    s.write_u32(msg.type);
    s.write_u32(msg.request_id);
    s.write_u32(static_cast<uint32_t>(msg.payload.size()));

    // Append raw payload bytes after the header.
    // Serializer::write_bytes would add its own length prefix, so we
    // build a frame vector instead.
    const auto& header = s.data();

    std::vector<uint8_t> frame;
    frame.reserve(header.size() + msg.payload.size());
    frame.insert(frame.end(), header.begin(), header.end());
    frame.insert(frame.end(), msg.payload.begin(), msg.payload.end());

    return pipe_.send(frame);
}

std::optional<Message> MessageChannel::receive() {
    auto raw = pipe_.receive();
    if (!raw) return std::nullopt;

    // Deserialize: type(4) + request_id(4) + payload_len(4) + payload
    Deserializer d(*raw);

    if (d.remaining() < 12) return std::nullopt;

    Message msg;
    msg.type = d.read_u32();
    msg.request_id = d.read_u32();
    uint32_t payload_len = d.read_u32();

    if (d.remaining() < payload_len) return std::nullopt;

    if (payload_len > 0) {
        msg.payload.resize(payload_len);
        for (uint32_t i = 0; i < payload_len; ++i) {
            msg.payload[i] = d.read_u8();
        }
    }

    return msg;
}

void MessageChannel::on(uint32_t message_type, MessageHandler handler) {
    handlers_[message_type] = std::move(handler);
}

void MessageChannel::dispatch(const Message& msg) {
    auto it = handlers_.find(msg.type);
    if (it != handlers_.end()) {
        it->second(msg);
    }
}

bool MessageChannel::is_open() const {
    return pipe_.is_open();
}

void MessageChannel::close() {
    pipe_.close();
}

} // namespace clever::ipc
