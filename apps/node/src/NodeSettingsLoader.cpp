#include "NodeSettingsLoader.h"
#include "Esp32NodeSettingsStorage.h"

NodeSettingsLoadStatus loadNodeSettings(
    const BoardInformation&,
    NodeSettings& settings
) {
    const SettingsStorageStatus initialization_status =
        initializeNodeSettingsStorage();

    if (initialization_status != SettingsStorageStatus::Ok) {
        return NodeSettingsLoadStatus::ReadFailed;
    }

    const SettingsStorageStatus storage_status =
        readNodeSettingsFromStorage(settings);

    switch (storage_status) {
        case SettingsStorageStatus::Ok:
            return NodeSettingsLoadStatus::Ok;

        case SettingsStorageStatus::NotFound:
            return NodeSettingsLoadStatus::NotConfigured;

        case SettingsStorageStatus::InitializationFailed:
        case SettingsStorageStatus::ReadFailed:
            return NodeSettingsLoadStatus::ReadFailed;
    }

    return NodeSettingsLoadStatus::ReadFailed;
}
