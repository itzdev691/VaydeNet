#pragma once

#include <cstdint>

#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/startup/HardwareIdentity.h"

enum class NodeSettingsLoadStatus : std::uint8_t {
    Ok,
    Provisioned,
    ReadFailed,
    WriteFailed
};

NodeSettingsLoadStatus loadNodeSettings(
    const HardwareIdentity& hardware_identity,
    NodeSettings& output
);
