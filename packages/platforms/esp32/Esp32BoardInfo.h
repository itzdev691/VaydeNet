#pragma once

#include <array>
#include <cstdint>

struct BoardInformation {
    std::array<std::uint8_t, 6> device_uid{};
    const char* board_model{nullptr};
};

enum class BoardInfoStatus : std::uint8_t {
    Ok,
    UidReadFailed,
    MissingBoardModel,
};

BoardInfoStatus readEsp32DeviceUid(
    std::array<std::uint8_t, 6>& outputUid
);

const char* getEsp32BoardModel();

BoardInfoStatus retrieveEsp32HardwareIdentity(
    BoardInformation& output
);
