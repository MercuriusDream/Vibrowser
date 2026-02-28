// SecureTransport is deprecated on macOS but still works and avoids
// pulling in external TLS libraries.  We suppress the warnings for the
// entire translation unit since every SecureTransport symbol triggers them.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <clever/net/tls_socket.h>

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

namespace clever::net {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

TlsSocket::TlsSocket() = default;

TlsSocket::~TlsSocket() {
    close();
}

// ---------------------------------------------------------------------------
// I/O callbacks for SecureTransport
// ---------------------------------------------------------------------------

OSStatus TlsSocket::read_callback(SSLConnectionRef conn, void* data, size_t* len) {
    auto* self = reinterpret_cast<TlsSocket*>(const_cast<void*>(conn));
    if (self->fd_ < 0) {
        *len = 0;
        return errSSLClosedAbort;
    }

    size_t requested = *len;
    size_t total_read = 0;
    auto* buf = static_cast<uint8_t*>(data);

    while (total_read < requested) {
        ssize_t n = ::read(self->fd_, buf + total_read, requested - total_read);
        if (n > 0) {
            total_read += static_cast<size_t>(n);
        } else if (n == 0) {
            // EOF
            *len = total_read;
            if (total_read == 0) {
                return errSSLClosedGraceful;
            }
            return errSSLWouldBlock;
        } else {
            // Error
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *len = total_read;
                if (total_read > 0) {
                    return errSSLWouldBlock;
                }
                return errSSLWouldBlock;
            }
            *len = total_read;
            return errSSLClosedAbort;
        }
    }

    *len = total_read;
    return noErr;
}

OSStatus TlsSocket::write_callback(SSLConnectionRef conn, const void* data, size_t* len) {
    auto* self = reinterpret_cast<TlsSocket*>(const_cast<void*>(conn));
    if (self->fd_ < 0) {
        *len = 0;
        return errSSLClosedAbort;
    }

    size_t requested = *len;
    size_t total_written = 0;
    const auto* buf = static_cast<const uint8_t*>(data);

    while (total_written < requested) {
        ssize_t n = ::write(self->fd_, buf + total_written, requested - total_written);
        if (n > 0) {
            total_written += static_cast<size_t>(n);
        } else if (n == 0) {
            break;
        } else {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *len = total_written;
                return errSSLWouldBlock;
            }
            *len = total_written;
            return errSSLClosedAbort;
        }
    }

    *len = total_written;
    return noErr;
}

// ---------------------------------------------------------------------------
// connect — perform TLS handshake over an already-connected socket
// ---------------------------------------------------------------------------

bool TlsSocket::connect(const std::string& host, uint16_t /*port*/, int fd) {
    // Clean up any previous session
    close();

    fd_ = fd;

    // Create SSL context
    ssl_context_ = SSLCreateContext(kCFAllocatorDefault, kSSLClientSide, kSSLStreamType);
    if (ssl_context_ == nullptr) {
        return false;
    }

    // Set I/O callbacks
    OSStatus status = SSLSetIOFuncs(ssl_context_, read_callback, write_callback);
    if (status != noErr) {
        close();
        return false;
    }

    // Set the connection reference to 'this' so the callbacks can access the fd
    status = SSLSetConnection(ssl_context_, reinterpret_cast<SSLConnectionRef>(this));
    if (status != noErr) {
        close();
        return false;
    }

    // Set peer domain name for SNI (Server Name Indication) and certificate
    // verification
    status = SSLSetPeerDomainName(ssl_context_, host.c_str(), host.size());
    if (status != noErr) {
        close();
        return false;
    }

    // Perform TLS handshake (may need multiple calls)
    do {
        status = SSLHandshake(ssl_context_);
    } while (status == errSSLWouldBlock);

    if (status != noErr) {
        close();
        return false;
    }

    connected_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// send — write data through the TLS session
// ---------------------------------------------------------------------------

bool TlsSocket::send(const uint8_t* data, size_t len) {
    if (!connected_ || ssl_context_ == nullptr) {
        return false;
    }

    size_t total_sent = 0;
    while (total_sent < len) {
        size_t processed = 0;
        OSStatus status = SSLWrite(ssl_context_, data + total_sent, len - total_sent, &processed);
        total_sent += processed;

        if (status == noErr) {
            continue;
        }
        if (status == errSSLWouldBlock) {
            // Partial write, loop to send the rest
            continue;
        }
        // Any other error is a failure
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// recv — read data from the TLS session
// ---------------------------------------------------------------------------

std::optional<std::vector<uint8_t>> TlsSocket::recv() {
    if (!connected_ || ssl_context_ == nullptr) {
        return std::nullopt;
    }

    constexpr size_t kChunkSize = 16384;
    uint8_t buf[kChunkSize];
    size_t processed = 0;

    OSStatus status = SSLRead(ssl_context_, buf, kChunkSize, &processed);

    if (processed > 0) {
        return std::vector<uint8_t>(buf, buf + processed);
    }

    if (status == errSSLClosedGraceful || status == errSSLClosedNoNotify) {
        // Connection closed — return empty vector to signal EOF
        return std::vector<uint8_t>{};
    }

    if (status == errSSLWouldBlock) {
        // No data available right now but connection is still alive
        return std::vector<uint8_t>{};
    }

    // Error
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// close — tear down the TLS session
// ---------------------------------------------------------------------------

void TlsSocket::close() {
    if (ssl_context_ != nullptr) {
        if (connected_) {
            SSLClose(ssl_context_);
        }
        CFRelease(ssl_context_);
        ssl_context_ = nullptr;
    }
    connected_ = false;
    fd_ = -1;
}

// ---------------------------------------------------------------------------
// is_connected
// ---------------------------------------------------------------------------

bool TlsSocket::is_connected() const {
    return connected_;
}

} // namespace clever::net

#pragma clang diagnostic pop
