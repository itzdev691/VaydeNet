#pragma once

#include <cstdint>

#include "Esp32BoardInfo.h"
#include "VaydeNet/config/NodeSettings.h"

enum class NodeSettingsLoadStatus : std::uint8_t {
    Ok,
    NotConfigured,
    ReadFailed
};

NodeSettingsLoadStatus loadNodeSettings(
    const BoardInformation& board_information,
    NodeSettings& output
);
