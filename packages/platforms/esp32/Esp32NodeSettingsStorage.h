#pragma once

#include <cstdint>

#include "VaydeNet/config/NodeSettings.h"

enum class SettingsStorageInitializationStatus : std::uint8_t {
    Ok,
    Failed
};

enum class SettingsStorageStatus : std::uint8_t {
    Configured,
    NotConfigured,
    ReadFailed
};

enum class SettingsStorageWriteStatus : std::uint8_t {
    Ok,
    WriteFailed
};

SettingsStorageInitializationStatus initializeNodeSettingsStorage();

SettingsStorageStatus readNodeSettingsFromStorage(
    NodeSettings& settings
);

SettingsStorageWriteStatus writeNodeSettingsToStorage(
    const NodeSettings& settings
);
