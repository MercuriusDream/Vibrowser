#pragma once
#include <clever/ipc/message.h>
#include <clever/ipc/serializer.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace clever::ipc {

inline constexpr size_t kMessagePipeRawFrameOverheadBytes = sizeof(uint32_t) * 3;
inline constexpr size_t kMaxMessagePipeFrameBytes =
    kMaxSerializedPayloadBytes + kMessagePipeRawFrameOverheadBytes;

class MessagePipe {
public:
    // Create a pair of connected pipes (for in-process testing)
    static std::pair<MessagePipe, MessagePipe> create_pair();

    // Create from existing file descriptors
    explicit MessagePipe(int fd);

    ~MessagePipe();

    // Move-only
    MessagePipe(MessagePipe&& other) noexcept;
    MessagePipe& operator=(MessagePipe&& other) noexcept;
    MessagePipe(const MessagePipe&) = delete;
    MessagePipe& operator=(const MessagePipe&) = delete;

    // Send raw bytes (prefixed with length)
    bool send(const uint8_t* data, size_t len);
    bool send(const std::vector<uint8_t>& data);

    // Receive raw bytes (reads length prefix, then payload)
    std::optional<std::vector<uint8_t>> receive();

    // Close the pipe
    void close();

    // Check if pipe is open
    bool is_open() const;

    int fd() const { return fd_; }

private:
    int fd_ = -1;
};

} // namespace clever::ipc
