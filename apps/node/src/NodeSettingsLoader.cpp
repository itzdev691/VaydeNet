#include "NodeSettingsLoader.h"
#include "Esp32NodeSettingsStorage.h"

NodeSettingsLoadStatus loadNodeSettings(
    const BoardInformation&,
    NodeSettings& settings
) {
    const SettingsStorageInitializationStatus initialization_status =
        initializeNodeSettingsStorage();

    if (
        initialization_status !=
        SettingsStorageInitializationStatus::Ok
    ) {
        return NodeSettingsLoadStatus::ReadFailed;
    }

    const SettingsStorageStatus storage_status =
        readNodeSettingsFromStorage(settings);

    switch (storage_status) {
        case SettingsStorageStatus::Configured:
            return NodeSettingsLoadStatus::Ok;

        case SettingsStorageStatus::NotConfigured:
            return NodeSettingsLoadStatus::NotConfigured;

        case SettingsStorageStatus::ReadFailed:
            return NodeSettingsLoadStatus::ReadFailed;
    }

    return NodeSettingsLoadStatus::ReadFailed;
}
