#pragma once

#include <array>
#include <cstdint>

struct HardwareIdentity {
    std::array<std::uint8_t, 6> device_uid{};
    const char* board_model{nullptr};
};
