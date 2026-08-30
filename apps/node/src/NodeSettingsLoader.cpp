#include "NodeSettingsLoader.h"
#include "Esp32NodeSettingsStorage.h"

namespace {

constexpr std::uint16_t kDefaultEspNowChannel = 1;

NodeSettings createDefaultNodeSettings() {
    NodeSettings settings{};
    settings.transport = TransportType::EspNow;
    settings.channel = kDefaultEspNowChannel;
    return settings;
}

}  // namespace

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

        case SettingsStorageStatus::NotConfigured: {
            NodeSettings initial_settings =
                createDefaultNodeSettings();

            const SettingsStorageWriteStatus write_status =
                writeNodeSettingsToStorage(initial_settings);

            if (write_status != SettingsStorageWriteStatus::Ok) {
                return NodeSettingsLoadStatus::WriteFailed;
            }

            settings = initial_settings;
            return NodeSettingsLoadStatus::Provisioned;
        }

        case SettingsStorageStatus::ReadFailed:
            return NodeSettingsLoadStatus::ReadFailed;
    }

    return NodeSettingsLoadStatus::ReadFailed;
}
