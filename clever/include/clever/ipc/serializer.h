#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace clever::ipc {

class Serializer {
public:
    void write_u8(uint8_t value);
    void write_u16(uint16_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);
    void write_i32(int32_t value);
    void write_i64(int64_t value);
    void write_f64(double value);
    void write_bool(bool value);
    void write_string(std::string_view str);
    void write_bytes(const uint8_t* data, size_t len);

    const std::vector<uint8_t>& data() const { return buffer_; }
    std::vector<uint8_t> take_data() { return std::move(buffer_); }

private:
    std::vector<uint8_t> buffer_;
};

class Deserializer {
public:
    explicit Deserializer(const uint8_t* data, size_t size);
    explicit Deserializer(const std::vector<uint8_t>& data);

    uint8_t read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    int32_t read_i32();
    int64_t read_i64();
    double read_f64();
    bool read_bool();
    std::string read_string();
    std::vector<uint8_t> read_bytes();

    bool has_remaining() const;
    size_t remaining() const;

private:
    const uint8_t* data_;
    size_t size_;
    size_t offset_ = 0;

    void check_remaining(size_t needed) const;
};

} // namespace clever::ipc
