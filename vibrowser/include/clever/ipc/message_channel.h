#pragma once
#include <clever/ipc/message.h>
#include <clever/ipc/message_pipe.h>
#include <clever/ipc/serializer.h>
#include <functional>
#include <optional>
#include <unordered_map>

namespace clever::ipc {

class MessageChannel {
public:
    using MessageHandler = std::function<void(const Message&)>;

    explicit MessageChannel(MessagePipe pipe);

    // Send a message
    bool send(const Message& msg);

    // Receive a message (blocking)
    std::optional<Message> receive();

    // Register a handler for a message type
    void on(uint32_t message_type, MessageHandler handler);

    // Dispatch a received message to registered handlers
    void dispatch(const Message& msg);

    // Check if channel is open
    bool is_open() const;

    // Close the channel
    void close();

private:
    MessagePipe pipe_;
    std::unordered_map<uint32_t, MessageHandler> handlers_;
};

} // namespace clever::ipc
