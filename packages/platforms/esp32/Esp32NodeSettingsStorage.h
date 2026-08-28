#pragma once

#include <cstdint>

#include "VaydeNet/config/NodeSettings.h"

enum class SettingsStorageStatus : std::uint8_t {
    Ok,
    NotFound,
    InitializationFailed,
    ReadFailed
};

SettingsStorageStatus initializeNodeSettingsStorage();

SettingsStorageStatus readNodeSettingsFromStorage(
    NodeSettings& settings
);
