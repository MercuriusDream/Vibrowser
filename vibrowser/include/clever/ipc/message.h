#pragma once
#include <cstdint>
#include <vector>

namespace clever::ipc {

struct Message {
    uint32_t type = 0;
    uint32_t request_id = 0;
    std::vector<uint8_t> payload;
};

} // namespace clever::ipc
