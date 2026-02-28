#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <clever/net/header_map.h>

namespace clever::net {

struct HpackHeaderField {
    std::string_view name;
    std::string_view value;
};

inline constexpr std::array<HpackHeaderField, 61> kHpackStaticTable = {{
    {":authority", ""},                          // 1
    {":method", "GET"},                          // 2
    {":method", "POST"},                         // 3
    {":path", "/"},                              // 4
    {":path", "/index.html"},                    // 5
    {":scheme", "http"},                         // 6
    {":scheme", "https"},                        // 7
    {":status", "200"},                          // 8
    {":status", "204"},                          // 9
    {":status", "206"},                          // 10
    {":status", "304"},                          // 11
    {":status", "400"},                          // 12
    {":status", "404"},                          // 13
    {":status", "500"},                          // 14
    {"accept-charset", ""},                      // 15
    {"accept-encoding", "gzip, deflate"},        // 16
    {"accept-language", ""},                     // 17
    {"accept-ranges", ""},                       // 18
    {"accept", ""},                              // 19
    {"access-control-allow-origin", ""},         // 20
    {"age", ""},                                 // 21
    {"allow", ""},                               // 22
    {"authorization", ""},                       // 23
    {"cache-control", ""},                       // 24
    {"content-disposition", ""},                 // 25
    {"content-encoding", ""},                    // 26
    {"content-language", ""},                    // 27
    {"content-length", ""},                      // 28
    {"content-location", ""},                    // 29
    {"content-range", ""},                       // 30
    {"content-type", ""},                        // 31
    {"cookie", ""},                              // 32
    {"date", ""},                                // 33
    {"etag", ""},                                // 34
    {"expect", ""},                              // 35
    {"expires", ""},                             // 36
    {"from", ""},                                // 37
    {"host", ""},                                // 38
    {"if-match", ""},                            // 39
    {"if-modified-since", ""},                   // 40
    {"if-none-match", ""},                       // 41
    {"if-range", ""},                            // 42
    {"if-unmodified-since", ""},                 // 43
    {"last-modified", ""},                       // 44
    {"link", ""},                                // 45
    {"location", ""},                            // 46
    {"max-forwards", ""},                        // 47
    {"proxy-authenticate", ""},                  // 48
    {"proxy-authorization", ""},                 // 49
    {"range", ""},                               // 50
    {"referer", ""},                             // 51
    {"refresh", ""},                             // 52
    {"retry-after", ""},                         // 53
    {"server", ""},                              // 54
    {"set-cookie", ""},                          // 55
    {"strict-transport-security", ""},           // 56
    {"transfer-encoding", ""},                   // 57
    {"user-agent", ""},                          // 58
    {"vary", ""},                                // 59
    {"via", ""},                                 // 60
    {"www-authenticate", ""},                    // 61
}};

std::vector<uint8_t> hpack_huffman_encode(std::string_view input);
std::optional<std::string> hpack_huffman_decode(const uint8_t* data, size_t len);

class HpackEncoder {
public:
    explicit HpackEncoder(size_t max_dynamic_table_size = 4096);

    void set_max_dynamic_table_size(size_t max_dynamic_table_size);
    size_t max_dynamic_table_size() const;
    size_t dynamic_table_size() const;

    std::vector<uint8_t> encode_header_list(const HeaderMap& headers);

private:
    struct DynamicEntry {
        std::string name;
        std::string value;
        size_t size = 0;
    };

    std::deque<DynamicEntry> dynamic_table_;
    size_t dynamic_table_size_ = 0;
    size_t max_dynamic_table_size_ = 4096;

    void add_dynamic_entry(const std::string& name, const std::string& value);
};

class HpackDecoder {
public:
    explicit HpackDecoder(size_t max_dynamic_table_size = 4096);

    void set_max_dynamic_table_size(size_t max_dynamic_table_size);
    size_t max_dynamic_table_size() const;
    size_t dynamic_table_size() const;

    HeaderMap decode(const uint8_t* data, size_t len);

private:
    struct DynamicEntry {
        std::string name;
        std::string value;
        size_t size = 0;
    };

    std::deque<DynamicEntry> dynamic_table_;
    size_t dynamic_table_size_ = 0;
    size_t max_dynamic_table_size_ = 4096;

    void add_dynamic_entry(const std::string& name, const std::string& value);
};

} // namespace clever::net

