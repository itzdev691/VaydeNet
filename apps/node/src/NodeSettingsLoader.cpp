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
    const HardwareIdentity&,
    NodeSettings& settings
) {
    const Esp32SettingsStorageInitializationStatus initialization_status =
        initializeEsp32NodeSettingsStorage();

    if (
        initialization_status !=
        Esp32SettingsStorageInitializationStatus::Ok
    ) {
        return NodeSettingsLoadStatus::ReadFailed;
    }

    const Esp32SettingsStorageReadStatus storage_status =
        readEsp32NodeSettingsFromStorage(settings);

    switch (storage_status) {
        case Esp32SettingsStorageReadStatus::Configured:
            return NodeSettingsLoadStatus::Ok;

        case Esp32SettingsStorageReadStatus::NotConfigured: {
            NodeSettings initial_settings =
                createDefaultNodeSettings();

            const Esp32SettingsStorageWriteStatus write_status =
                writeEsp32NodeSettingsToStorage(initial_settings);

            if (write_status != Esp32SettingsStorageWriteStatus::Ok) {
                return NodeSettingsLoadStatus::WriteFailed;
            }

            settings = initial_settings;
            return NodeSettingsLoadStatus::Provisioned;
        }

        case Esp32SettingsStorageReadStatus::ReadFailed:
            return NodeSettingsLoadStatus::ReadFailed;
    }

    return NodeSettingsLoadStatus::ReadFailed;
}
