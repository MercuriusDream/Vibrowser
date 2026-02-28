#include <clever/ipc/serializer.h>
#include <stdexcept>

namespace clever::ipc {

// ---------------------------------------------------------------------------
// Serializer
// ---------------------------------------------------------------------------

void Serializer::write_u8(uint8_t value) {
    buffer_.push_back(value);
}

void Serializer::write_u16(uint16_t value) {
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Serializer::write_u32(uint32_t value) {
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Serializer::write_u64(uint64_t value) {
    buffer_.push_back(static_cast<uint8_t>((value >> 56) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 48) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 40) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 32) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer_.push_back(static_cast<uint8_t>(value & 0xFF));
}

void Serializer::write_i32(int32_t value) {
    uint32_t uval;
    std::memcpy(&uval, &value, sizeof(uval));
    write_u32(uval);
}

void Serializer::write_i64(int64_t value) {
    uint64_t uval;
    std::memcpy(&uval, &value, sizeof(uval));
    write_u64(uval);
}

void Serializer::write_f64(double value) {
    uint64_t uval;
    std::memcpy(&uval, &value, sizeof(uval));
    write_u64(uval);
}

void Serializer::write_bool(bool value) {
    write_u8(value ? 1 : 0);
}

void Serializer::write_string(std::string_view str) {
    write_u32(static_cast<uint32_t>(str.size()));
    const auto* bytes = reinterpret_cast<const uint8_t*>(str.data());
    buffer_.insert(buffer_.end(), bytes, bytes + str.size());
}

void Serializer::write_bytes(const uint8_t* data, size_t len) {
    write_u32(static_cast<uint32_t>(len));
    if (data != nullptr && len > 0) {
        buffer_.insert(buffer_.end(), data, data + len);
    }
}

// ---------------------------------------------------------------------------
// Deserializer
// ---------------------------------------------------------------------------

Deserializer::Deserializer(const uint8_t* data, size_t size)
    : data_(data), size_(size) {}

Deserializer::Deserializer(const std::vector<uint8_t>& data)
    : data_(data.data()), size_(data.size()) {}

void Deserializer::check_remaining(size_t needed) const {
    if (offset_ + needed > size_) {
        throw std::runtime_error(
            "Deserializer underflow: need " + std::to_string(needed) +
            " bytes but only " + std::to_string(size_ - offset_) + " remaining");
    }
}

uint8_t Deserializer::read_u8() {
    check_remaining(1);
    return data_[offset_++];
}

uint16_t Deserializer::read_u16() {
    check_remaining(2);
    uint16_t value = static_cast<uint16_t>(data_[offset_]) << 8
                   | static_cast<uint16_t>(data_[offset_ + 1]);
    offset_ += 2;
    return value;
}

uint32_t Deserializer::read_u32() {
    check_remaining(4);
    uint32_t value = static_cast<uint32_t>(data_[offset_]) << 24
                   | static_cast<uint32_t>(data_[offset_ + 1]) << 16
                   | static_cast<uint32_t>(data_[offset_ + 2]) << 8
                   | static_cast<uint32_t>(data_[offset_ + 3]);
    offset_ += 4;
    return value;
}

uint64_t Deserializer::read_u64() {
    check_remaining(8);
    uint64_t value = static_cast<uint64_t>(data_[offset_]) << 56
                   | static_cast<uint64_t>(data_[offset_ + 1]) << 48
                   | static_cast<uint64_t>(data_[offset_ + 2]) << 40
                   | static_cast<uint64_t>(data_[offset_ + 3]) << 32
                   | static_cast<uint64_t>(data_[offset_ + 4]) << 24
                   | static_cast<uint64_t>(data_[offset_ + 5]) << 16
                   | static_cast<uint64_t>(data_[offset_ + 6]) << 8
                   | static_cast<uint64_t>(data_[offset_ + 7]);
    offset_ += 8;
    return value;
}

int32_t Deserializer::read_i32() {
    uint32_t uval = read_u32();
    int32_t result;
    std::memcpy(&result, &uval, sizeof(result));
    return result;
}

int64_t Deserializer::read_i64() {
    uint64_t uval = read_u64();
    int64_t result;
    std::memcpy(&result, &uval, sizeof(result));
    return result;
}

double Deserializer::read_f64() {
    uint64_t uval = read_u64();
    double result;
    std::memcpy(&result, &uval, sizeof(result));
    return result;
}

bool Deserializer::read_bool() {
    return read_u8() != 0;
}

std::string Deserializer::read_string() {
    uint32_t len = read_u32();
    check_remaining(len);
    std::string result(reinterpret_cast<const char*>(data_ + offset_), len);
    offset_ += len;
    return result;
}

std::vector<uint8_t> Deserializer::read_bytes() {
    uint32_t len = read_u32();
    check_remaining(len);
    std::vector<uint8_t> result(data_ + offset_, data_ + offset_ + len);
    offset_ += len;
    return result;
}

bool Deserializer::has_remaining() const {
    return offset_ < size_;
}

size_t Deserializer::remaining() const {
    return size_ - offset_;
}

} // namespace clever::ipc
