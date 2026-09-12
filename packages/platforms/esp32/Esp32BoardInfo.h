#pragma once

#include <array>
#include <cstdint>

#include "VaydeNet/startup/HardwareIdentity.h"

enum class Esp32BoardInfoStatus : std::uint8_t {
    Ok,
    UidReadFailed,
    MissingBoardModel,
};

Esp32BoardInfoStatus readEsp32DeviceUid(
    std::array<std::uint8_t, 6>& output_uid
);

const char* getEsp32BoardModel();

Esp32BoardInfoStatus retrieveEsp32HardwareIdentity(
    HardwareIdentity& output
);
