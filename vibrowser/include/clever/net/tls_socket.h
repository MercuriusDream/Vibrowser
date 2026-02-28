#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// SecureTransport is deprecated but still functional on macOS.
// We suppress the deprecation warnings since we intentionally use it
// to avoid external dependencies.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <Security/Security.h>
#include <Security/SecureTransport.h>
#pragma clang diagnostic pop

namespace clever::net {

class TlsSocket {
public:
    TlsSocket();
    ~TlsSocket();

    // Non-copyable, non-movable
    TlsSocket(const TlsSocket&) = delete;
    TlsSocket& operator=(const TlsSocket&) = delete;
    TlsSocket(TlsSocket&&) = delete;
    TlsSocket& operator=(TlsSocket&&) = delete;

    // Connect to host:port with TLS over an existing connected socket fd.
    // The TlsSocket does NOT own the fd; caller is responsible for closing it.
    bool connect(const std::string& host, uint16_t port, int fd);

    // Send data through TLS
    bool send(const uint8_t* data, size_t len);

    // Receive data through TLS.
    // Reads a chunk of available data and returns it.
    // Returns std::nullopt on error, empty vector on EOF/connection close.
    std::optional<std::vector<uint8_t>> recv();

    // Close the TLS session (does NOT close the underlying fd)
    void close();

    // Check whether the TLS session is active
    bool is_connected() const;

private:
    SSLContextRef ssl_context_ = nullptr;
    int fd_ = -1;
    bool connected_ = false;

    // SSLSetIOFuncs callbacks
    static OSStatus read_callback(SSLConnectionRef conn, void* data, size_t* len);
    static OSStatus write_callback(SSLConnectionRef conn, const void* data, size_t* len);
};

} // namespace clever::net
