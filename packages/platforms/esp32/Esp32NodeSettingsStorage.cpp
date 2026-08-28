#include "Esp32NodeSettingsStorage.h"

#include "nvs.h"
#include "nvs_flash.h"

SettingsStorageStatus initializeNodeSettingsStorage() {
    if (nvs_flash_init() != ESP_OK) {
        return SettingsStorageStatus::InitializationFailed;
    }

    return SettingsStorageStatus::Ok;
}

SettingsStorageStatus readNodeSettingsFromStorage(
    NodeSettings& settings
) {
    settings.transport = TransportType::EspNow;
    settings.channel = 1;
    // The NVS namespace and key schema still need to be defined.
    return SettingsStorageStatus::Ok;
}
