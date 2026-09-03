#pragma once

#include <cstdint>

#include "VaydeNet/config/NodeSettings.h"

enum class Esp32SettingsStorageInitializationStatus : std::uint8_t {
    Ok,
    Failed
};

enum class Esp32SettingsStorageReadStatus : std::uint8_t {
    Configured,
    NotConfigured,
    ReadFailed
};

enum class Esp32SettingsStorageWriteStatus : std::uint8_t {
    Ok,
    WriteFailed
};

Esp32SettingsStorageInitializationStatus initializeEsp32NodeSettingsStorage();

Esp32SettingsStorageReadStatus readEsp32NodeSettingsFromStorage(
    NodeSettings& settings
);

Esp32SettingsStorageWriteStatus writeEsp32NodeSettingsToStorage(
    const NodeSettings& settings
);
