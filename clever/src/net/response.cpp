#include <clever/net/response.h>
#include <algorithm>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <zlib.h>

namespace clever::net {

std::string Response::body_as_string() const {
    return std::string(body.begin(), body.end());
}

namespace {

// Find \r\n\r\n in a byte vector, return position of the first byte after the separator.
// Returns std::string::npos if not found.
size_t find_header_end(const std::vector<uint8_t>& data) {
    const char* sep = "\r\n\r\n";
    const size_t sep_len = 4;
    if (data.size() < sep_len) return std::string::npos;

    for (size_t i = 0; i <= data.size() - sep_len; ++i) {
        if (std::memcmp(data.data() + i, sep, sep_len) == 0) {
            return i + sep_len;
        }
    }
    return std::string::npos;
}

// Parse chunked transfer encoding body from data starting at body_start.
std::vector<uint8_t> parse_chunked_body(const uint8_t* data, size_t len) {
    std::vector<uint8_t> result;
    size_t pos = 0;

    while (pos < len) {
        // Find the end of the chunk size line
        size_t line_end = pos;
        while (line_end + 1 < len) {
            if (data[line_end] == '\r' && data[line_end + 1] == '\n') {
                break;
            }
            ++line_end;
        }

        if (line_end + 1 >= len) break;

        // Parse chunk size (hex)
        std::string_view size_str(reinterpret_cast<const char*>(data + pos),
                                  line_end - pos);

        // Handle chunk extensions (after semicolon)
        auto semi = size_str.find(';');
        if (semi != std::string_view::npos) {
            size_str = size_str.substr(0, semi);
        }

        size_t chunk_size = 0;
        auto [ptr, ec] = std::from_chars(size_str.data(),
                                          size_str.data() + size_str.size(),
                                          chunk_size, 16);
        if (ec != std::errc{}) break;

        // 0-length chunk means end
        if (chunk_size == 0) break;

        // Move past the \r\n after chunk size
        size_t chunk_data_start = line_end + 2;

        // Ensure we have enough data
        if (chunk_data_start + chunk_size > len) break;

        // Copy chunk data
        result.insert(result.end(),
                      data + chunk_data_start,
                      data + chunk_data_start + chunk_size);

        // Move past chunk data and trailing \r\n
        pos = chunk_data_start + chunk_size + 2;
    }

    return result;
}

// Try to inflate data with a specific windowBits setting.
// Returns true on success and fills 'output'; false on error.
bool try_inflate(const std::vector<uint8_t>& compressed, int window_bits,
                 std::vector<uint8_t>& output) {
    z_stream strm{};
    if (inflateInit2(&strm, window_bits) != Z_OK) return false;

    strm.avail_in = static_cast<uInt>(compressed.size());
    strm.next_in = const_cast<Bytef*>(compressed.data());

    output.clear();
    output.reserve(compressed.size() * 4);

    uint8_t buffer[32768];
    int ret;
    do {
        strm.avail_out = sizeof(buffer);
        strm.next_out = buffer;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR ||
            ret == Z_MEM_ERROR || ret == Z_NEED_DICT) {
            inflateEnd(&strm);
            return false;
        }
        if (ret == Z_BUF_ERROR) {
            // No progress possible — input exhausted before stream end
            inflateEnd(&strm);
            return false;
        }
        size_t have = sizeof(buffer) - strm.avail_out;
        output.insert(output.end(), buffer, buffer + have);
    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    return true;
}

// Decompress gzip/deflate content using zlib.
// Tries auto-detection first (gzip + zlib-wrapped deflate), then falls back
// to raw deflate if auto-detection fails.
std::vector<uint8_t> decompress_gzip(const std::vector<uint8_t>& compressed) {
    if (compressed.empty()) return {};

    std::vector<uint8_t> result;

    // 15 + 32 enables automatic gzip / zlib-wrapped deflate detection
    if (try_inflate(compressed, 15 + 32, result)) {
        return result;
    }

    // Fallback: try raw deflate (windowBits = -15) for servers that send
    // raw deflate under Content-Encoding: deflate
    if (try_inflate(compressed, -15, result)) {
        return result;
    }

    // All attempts failed — return original data as fallback
    return compressed;
}

} // anonymous namespace

std::optional<Response> Response::parse(const std::vector<uint8_t>& data) {
    // Find end of headers
    size_t header_end = find_header_end(data);
    if (header_end == std::string::npos) {
        return std::nullopt;
    }

    // Convert header section to string for easier parsing
    std::string header_section(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(header_end));

    // Parse status line: HTTP/1.1 200 OK\r\n
    auto first_crlf = header_section.find("\r\n");
    if (first_crlf == std::string::npos) {
        return std::nullopt;
    }

    std::string status_line = header_section.substr(0, first_crlf);

    // Parse: "HTTP/1.1 <status_code> <status_text>"
    // Find first space (after HTTP version)
    auto sp1 = status_line.find(' ');
    if (sp1 == std::string::npos) {
        return std::nullopt;
    }

    // Find second space (after status code)
    auto sp2 = status_line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) {
        return std::nullopt;
    }

    Response resp;

    // Parse status code
    std::string code_str = status_line.substr(sp1 + 1, sp2 - sp1 - 1);
    try {
        resp.status = static_cast<uint16_t>(std::stoi(code_str));
    } catch (...) {
        return std::nullopt;
    }

    // Status text
    resp.status_text = status_line.substr(sp2 + 1);

    // Parse headers (everything between first CRLF and the blank line)
    size_t pos = first_crlf + 2;
    while (pos < header_end - 4) {  // -4 for the trailing \r\n\r\n
        auto line_end = header_section.find("\r\n", pos);
        if (line_end == std::string::npos || line_end == pos) break;

        std::string line = header_section.substr(pos, line_end - pos);
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string name = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim leading whitespace from value
            size_t val_start = 0;
            while (val_start < value.size() && value[val_start] == ' ') {
                ++val_start;
            }
            value = value.substr(val_start);

            resp.headers.append(name, value);
        }

        pos = line_end + 2;
    }

    // Parse body
    size_t body_start = header_end;

    // Check for chunked transfer encoding
    auto te = resp.headers.get("transfer-encoding");
    if (te.has_value() && te->find("chunked") != std::string::npos) {
        resp.body = parse_chunked_body(data.data() + body_start,
                                        data.size() - body_start);
    } else {
        // Use Content-Length if available
        auto cl = resp.headers.get("content-length");
        size_t content_length = 0;
        if (cl.has_value()) {
            try {
                content_length = std::stoull(*cl);
            } catch (...) {
                content_length = 0;
            }
        } else {
            // No content-length and not chunked: use remaining data
            content_length = data.size() - body_start;
        }

        if (content_length > 0 && body_start + content_length <= data.size()) {
            resp.body.assign(data.begin() + static_cast<std::ptrdiff_t>(body_start),
                             data.begin() + static_cast<std::ptrdiff_t>(body_start + content_length));
        }
    }

    // Decompress gzip/deflate content
    auto ce = resp.headers.get("content-encoding");
    if (ce.has_value()) {
        std::string encoding = *ce;
        // Lowercase for comparison
        std::transform(encoding.begin(), encoding.end(), encoding.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (encoding.find("gzip") != std::string::npos ||
            encoding.find("deflate") != std::string::npos) {
            resp.body = decompress_gzip(resp.body);
        }
    }

    return resp;
}

} // namespace clever::net
