#include <clever/net/hpack.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace clever::net {

// RFC 7541 Huffman code table
static constexpr std::array<uint32_t, 257> kHuffmanCodes = {{
    0x1ff8, 0x7fffd8, 0xffffd8, 0xfffffe0, 0xfffffe1, 0xfffffe2, 0xfffffe3, 0xfffffe4,
    0xfffffe5, 0xfffffe6, 0xfffffe7, 0xfffffe8, 0xfffffe9, 0xfffffea, 0xfffffeb, 0xfffffec,
    0xfffffed, 0xfffffee, 0xfffffef, 0xffffff0, 0xffffff1, 0xffffff2, 0xffffff3, 0xffffff4,
    0xffffff5, 0xffffff6, 0xffffff7, 0xffffff8, 0xffffff9, 0xffffffa, 0xffffffb, 0xffffffc,
    0x14, 0x3f8, 0x3f9, 0xffa, 0x1ff9, 0x15, 0xf8, 0x16,
    0x17, 0xf9, 0x18, 0xfa, 0x19, 0x1a, 0xfb, 0x1b,
    0xf7, 0x1c, 0x1d, 0x1e, 0x1f, 0x5f0, 0x5f1, 0x5f2,
    0x20, 0xffb, 0xffc, 0x1ffa, 0x21, 0x5f3, 0x5f4, 0x5f5,
    0x5f6, 0x5f7, 0x5f8, 0x22, 0xffd, 0x1ffb, 0xffe, 0x5f9,
    0x5fa, 0x5fb, 0x5fc, 0x1ffc, 0x23, 0x5fd, 0x4, 0x1ffd,
    0x5fe, 0x5ff, 0x6, 0x9fe, 0x24, 0x25, 0x5, 0x7f8,
    0x7f9, 0x7fa, 0x7fb, 0x3fa, 0x7fc, 0x3fb, 0x26, 0x27,
    0x28, 0x7fd, 0x3fc, 0xfffe4, 0x1ffe0, 0x1ffe1, 0xfffe5, 0xfffe6,
    0xfffe7, 0xfffe8, 0xfffe9, 0xffffea, 0xfffffeb, 0xfffffec, 0xffffed, 0xffffee,
    0xffffef, 0xffffff0, 0xffffff1, 0xffffff2, 0xffffff3, 0xffffff4, 0xffffff5, 0xffffff6,
    0xffffff7, 0xffffff8, 0xffffff9, 0xffffffa, 0xffffffb, 0x3fc, 0x1ffe2, 0xfffffec,
    0xfffffed, 0x1ffe3, 0x3fd, 0x1ffe4, 0x1ffe5, 0x7fe, 0x1ffe6, 0x3fe,
    0x7ff, 0x1ffe7, 0x1ffe8, 0x9fe, 0x3ff, 0x1ffe9, 0xfffea, 0x28f,
    0x1ffea, 0x2, 0x3, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
    0x1f, 0x20, 0x21, 0x1ffeb, 0x22, 0x1ffec, 0x3c0, 0x1ffed,
    0x23, 0x24, 0x1ffee, 0x25, 0x1ffef, 0x26, 0x27, 0x1fff0,
    0x28, 0x29, 0x1fff1, 0x1fff2, 0x1a0, 0x2a, 0x1fff3, 0x1fff4,
    0x1fff5, 0x3c1, 0x3c2, 0x0, 0x1fff6, 0x1fff7, 0x3c, 0x1fff8,
    0x1a1, 0x1fff9, 0x2b, 0x1fffa, 0x1a2, 0x1a3, 0x1ffb, 0x1ffc,
    0x2c, 0x3c3, 0x3c4, 0x1a4, 0x2d, 0x1a5, 0x1ffdb, 0x1ffdc,
    0x3c5, 0x1ffdd, 0x1a6, 0x26c, 0x26d, 0x26e, 0x26f, 0x1a7,
    0x1a8, 0x1ffde, 0x270, 0x1a9, 0x1a10, 0x1a11, 0x1ffd, 0x1aa,
    0x1a12, 0x1a13, 0x1a14, 0x1a15, 0x1a16, 0x1a17, 0x1a18, 0x1a19,
}};

static constexpr std::array<uint8_t, 257> kHuffmanBits = {{
    13, 23, 28, 28, 28, 28, 28, 28, 28, 24, 30, 28, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 20, 24, 28, 28, 28, 28, 28, 28,
    6, 10, 10, 12, 13, 6, 8, 11, 11, 10, 13, 8, 11, 8, 8, 11,
    13, 10, 11, 8, 3, 14, 14, 14, 6, 13, 11, 7, 13, 14, 13, 6,
    12, 13, 7, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
    8, 13, 10, 14, 14, 14, 14, 10, 13, 8, 14, 14, 14, 14, 14, 14,
    13, 13, 13, 12, 13, 14, 14, 14, 14, 14, 14, 13, 12, 14, 13, 14,
    6, 10, 13, 8, 12, 10, 13, 10, 10, 10, 13, 10, 13, 10, 13, 12,
    8, 13, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 11,
    5, 7, 7, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 14, 11, 14,
    6, 14, 6, 14, 6, 14, 12, 13, 5, 14, 7, 13, 13, 13, 14, 11,
    6, 14, 11, 11, 13, 13, 6, 13, 6, 13, 6, 13, 8, 12, 8, 12,
    8, 12, 8, 12, 8, 12, 8, 12, 8, 13, 8, 13, 8, 13, 12, 6,
    8, 14, 6, 8, 10, 12, 14, 8, 12, 10, 14, 8, 12, 10, 12, 10,
    14, 14, 12, 13, 6, 14, 10, 14, 10, 14, 10, 14, 10, 14, 10, 14,
    10, 14, 10, 14, 10, 14, 10, 14, 10, 14, 10, 14, 10, 14, 12, 12,
}};

std::vector<uint8_t> hpack_huffman_encode(std::string_view input) {
    std::vector<uint8_t> result;
    uint32_t buffer = 0;
    uint8_t bits = 0;

    for (uint8_t ch : input) {
        uint32_t code = kHuffmanCodes[ch];
        uint8_t nbits = kHuffmanBits[ch];

        buffer = (buffer << nbits) | code;
        bits += nbits;

        while (bits >= 8) {
            bits -= 8;
            result.push_back((buffer >> bits) & 0xFF);
        }
    }

    if (bits > 0) {
        result.push_back((buffer << (8 - bits)) & 0xFF);
    }

    return result;
}

std::optional<std::string> hpack_huffman_decode(const uint8_t* data, size_t len) {
    std::string result;
    uint32_t buffer = 0;
    uint8_t bits = 0;

    for (size_t i = 0; i < len; ++i) {
        buffer = (buffer << 8) | data[i];
        bits += 8;

        while (bits >= 13) {
            bits -= 13;
            uint16_t code = (buffer >> bits) & 0x1FFF;

            for (int ch = 0; ch < 257; ++ch) {
                if (kHuffmanCodes[ch] == code && kHuffmanBits[ch] == 13) {
                    result += static_cast<char>(ch);
                    break;
                }
            }
        }
    }

    return result;
}

HpackEncoder::HpackEncoder(size_t max_dynamic_table_size)
    : max_dynamic_table_size_(max_dynamic_table_size) {}

void HpackEncoder::set_max_dynamic_table_size(size_t max_dynamic_table_size) {
    max_dynamic_table_size_ = max_dynamic_table_size;

    while (dynamic_table_size_ > max_dynamic_table_size_ && !dynamic_table_.empty()) {
        dynamic_table_size_ -= dynamic_table_.back().size;
        dynamic_table_.pop_back();
    }
}

size_t HpackEncoder::max_dynamic_table_size() const {
    return max_dynamic_table_size_;
}

size_t HpackEncoder::dynamic_table_size() const {
    return dynamic_table_size_;
}

void HpackEncoder::add_dynamic_entry(const std::string& name, const std::string& value) {
    size_t entry_size = name.size() + value.size() + 32;

    while (dynamic_table_size_ + entry_size > max_dynamic_table_size_ && !dynamic_table_.empty()) {
        dynamic_table_size_ -= dynamic_table_.back().size;
        dynamic_table_.pop_back();
    }

    dynamic_table_.push_front(DynamicEntry{name, value, entry_size});
    dynamic_table_size_ += entry_size;
}

std::vector<uint8_t> HpackEncoder::encode_header_list(const HeaderMap& headers) {
    std::vector<uint8_t> result;

    for (auto it = headers.begin(); it != headers.end(); ++it) {
        std::string name = it->first;
        std::string value = it->second;

        int static_index = -1;
        for (size_t i = 0; i < kHpackStaticTable.size(); ++i) {
            if (kHpackStaticTable[i].name == name) {
                static_index = i + 1;
                break;
            }
        }

        if (static_index > 0) {
            result.push_back(0x40 | static_index);
        } else {
            result.push_back(0x40);
        }

        auto huffman_encoded = hpack_huffman_encode(name);
        result.push_back(0x80 | huffman_encoded.size());
        result.insert(result.end(), huffman_encoded.begin(), huffman_encoded.end());

        auto value_encoded = hpack_huffman_encode(value);
        result.push_back(0x80 | value_encoded.size());
        result.insert(result.end(), value_encoded.begin(), value_encoded.end());

        add_dynamic_entry(name, value);
    }

    return result;
}

HpackDecoder::HpackDecoder(size_t max_dynamic_table_size)
    : max_dynamic_table_size_(max_dynamic_table_size) {}

void HpackDecoder::set_max_dynamic_table_size(size_t max_dynamic_table_size) {
    max_dynamic_table_size_ = max_dynamic_table_size;

    while (dynamic_table_size_ > max_dynamic_table_size_ && !dynamic_table_.empty()) {
        dynamic_table_size_ -= dynamic_table_.back().size;
        dynamic_table_.pop_back();
    }
}

size_t HpackDecoder::max_dynamic_table_size() const {
    return max_dynamic_table_size_;
}

size_t HpackDecoder::dynamic_table_size() const {
    return dynamic_table_size_;
}

void HpackDecoder::add_dynamic_entry(const std::string& name, const std::string& value) {
    size_t entry_size = name.size() + value.size() + 32;

    while (dynamic_table_size_ + entry_size > max_dynamic_table_size_ && !dynamic_table_.empty()) {
        dynamic_table_size_ -= dynamic_table_.back().size;
        dynamic_table_.pop_back();
    }

    dynamic_table_.push_front(DynamicEntry{name, value, entry_size});
    dynamic_table_size_ += entry_size;
}

HeaderMap HpackDecoder::decode(const uint8_t* data, size_t len) {
    HeaderMap result;
    size_t pos = 0;

    while (pos < len) {
        uint8_t byte = data[pos++];

        if ((byte & 0x80) == 0x00) {
            int index = byte & 0x3F;

            if (index > 0 && index <= static_cast<int>(kHpackStaticTable.size())) {
                std::string name(kHpackStaticTable[index - 1].name);

                if (pos < len && (data[pos] & 0x80)) {
                    uint8_t len_byte = data[pos++];
                    size_t str_len = len_byte & 0x7F;
                    if (pos + str_len <= len) {
                        auto decoded = hpack_huffman_decode(&data[pos], str_len);
                        if (decoded) {
                            result.set(name, *decoded);
                            pos += str_len;
                        }
                    }
                }
            }
        } else if ((byte & 0x40) == 0x40) {
            std::string name;
            std::string value;

            if ((byte & 0x3F) > 0) {
                int index = byte & 0x3F;
                if (index > 0 && index <= static_cast<int>(kHpackStaticTable.size())) {
                    name = std::string(kHpackStaticTable[index - 1].name);
                }
            } else {
                if (pos < len && (data[pos] & 0x80)) {
                    uint8_t len_byte = data[pos++];
                    size_t str_len = len_byte & 0x7F;
                    if (pos + str_len <= len) {
                        auto decoded = hpack_huffman_decode(&data[pos], str_len);
                        if (decoded) {
                            name = *decoded;
                            pos += str_len;
                        }
                    }
                }
            }

            if (pos < len && (data[pos] & 0x80)) {
                uint8_t len_byte = data[pos++];
                size_t str_len = len_byte & 0x7F;
                if (pos + str_len <= len) {
                    auto decoded = hpack_huffman_decode(&data[pos], str_len);
                    if (decoded) {
                        value = *decoded;
                        pos += str_len;
                    }
                }
            }

            if (!name.empty()) {
                result.set(name, value);
                add_dynamic_entry(name, value);
            }
        }
    }

    return result;
}

} // namespace clever::net
