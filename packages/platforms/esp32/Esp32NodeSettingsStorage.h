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

SettingsStorageInitializationStatus initializeNodeSettingsStorage();

SettingsStorageStatus readNodeSettingsFromStorage(
    NodeSettings& settings
);
