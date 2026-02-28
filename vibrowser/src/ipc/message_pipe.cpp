#include <clever/ipc/message_pipe.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace clever::ipc {

namespace {

// Write exactly n bytes to fd, handling partial writes and EINTR.
bool write_all(int fd, const uint8_t* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd, data + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        written += static_cast<size_t>(n);
    }
    return true;
}

// Read exactly n bytes from fd, handling partial reads and EINTR.
// Returns false on EOF or error.
bool read_all(int fd, uint8_t* buf, size_t len) {
    size_t total_read = 0;
    while (total_read < len) {
        ssize_t n = ::read(fd, buf + total_read, len - total_read);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false; // EOF
        total_read += static_cast<size_t>(n);
    }
    return true;
}

} // anonymous namespace

std::pair<MessagePipe, MessagePipe> MessagePipe::create_pair() {
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        throw std::runtime_error(
            std::string("socketpair failed: ") + std::strerror(errno));
    }

    // Increase socket buffer sizes to handle large payloads without
    // blocking when send and receive happen sequentially in tests.
    constexpr int buf_size = 256 * 1024;
    for (int fd : fds) {
        ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size));
        ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size));
    }

    return {MessagePipe(fds[0]), MessagePipe(fds[1])};
}

MessagePipe::MessagePipe(int fd) : fd_(fd) {}

MessagePipe::~MessagePipe() {
    close();
}

MessagePipe::MessagePipe(MessagePipe&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)) {}

MessagePipe& MessagePipe::operator=(MessagePipe&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

bool MessagePipe::send(const uint8_t* data, size_t len) {
    if (!is_open()) return false;

    // Write 4-byte length prefix in network byte order
    uint32_t net_len = htonl(static_cast<uint32_t>(len));
    auto* prefix = reinterpret_cast<const uint8_t*>(&net_len);
    if (!write_all(fd_, prefix, 4)) return false;

    // Write payload
    if (len > 0 && data != nullptr) {
        if (!write_all(fd_, data, len)) return false;
    }
    return true;
}

bool MessagePipe::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

std::optional<std::vector<uint8_t>> MessagePipe::receive() {
    if (!is_open()) return std::nullopt;

    // Read 4-byte length prefix
    uint32_t net_len = 0;
    auto* prefix = reinterpret_cast<uint8_t*>(&net_len);
    if (!read_all(fd_, prefix, 4)) return std::nullopt;

    uint32_t len = ntohl(net_len);

    // Read payload
    std::vector<uint8_t> payload(len);
    if (len > 0) {
        if (!read_all(fd_, payload.data(), len)) return std::nullopt;
    }
    return payload;
}

void MessagePipe::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool MessagePipe::is_open() const {
    return fd_ >= 0;
}

} // namespace clever::ipc
