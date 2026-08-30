#include "Esp32NodeSettingsStorage.h"

#include "nvs.h"
#include "nvs_flash.h"

namespace {

constexpr char kSettingsNamespace[] = "vaydenet";
constexpr char kConfiguredKey[] = "configured";
constexpr char kTransportKey[] = "transport";
constexpr char kChannelKey[] = "channel";

SettingsStorageStatus checkConfigured(
    nvs_handle_t handle
) {
    std::uint8_t configured = 0;

    const esp_err_t result =
        nvs_get_u8(
            handle,
            kConfiguredKey,
            &configured
        );

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return SettingsStorageStatus::NotConfigured;
    }

    if (result != ESP_OK) {
        return SettingsStorageStatus::ReadFailed;
    }

    if (configured != 1) {
        return SettingsStorageStatus::NotConfigured;
    }

    return SettingsStorageStatus::Configured;
}

}  // namespace

SettingsStorageInitializationStatus initializeNodeSettingsStorage() {
    if (nvs_flash_init() != ESP_OK) {
        return SettingsStorageInitializationStatus::Failed;
    }

    return SettingsStorageInitializationStatus::Ok;
}

SettingsStorageStatus readNodeSettingsFromStorage(
    NodeSettings& settings
) {
    nvs_handle_t handle{};

    const esp_err_t open_result =
        nvs_open(
            kSettingsNamespace,
            NVS_READONLY,
            &handle
        );

    if (open_result == ESP_ERR_NVS_NOT_FOUND) {
        return SettingsStorageStatus::NotConfigured;
    }

    if (open_result != ESP_OK) {
        return SettingsStorageStatus::ReadFailed;
    }

    const SettingsStorageStatus configured_status =
        checkConfigured(handle);

    if (configured_status != SettingsStorageStatus::Configured) {
        nvs_close(handle);
        return configured_status;
    }

    NodeSettings candidate{};
    std::uint8_t stored_transport = 0;

    const esp_err_t transport_result =
        nvs_get_u8(
            handle,
            kTransportKey,
            &stored_transport
        );

    if (transport_result != ESP_OK) {
        nvs_close(handle);
        return SettingsStorageStatus::ReadFailed;
    }

    if (
        stored_transport <
            static_cast<std::uint8_t>(TransportType::EspNow) ||
        stored_transport >
            static_cast<std::uint8_t>(TransportType::Ethernet)
    ) {
        nvs_close(handle);
        return SettingsStorageStatus::ReadFailed;
    }

    candidate.transport =
        static_cast<TransportType>(stored_transport);

    const esp_err_t channel_result =
        nvs_get_u16(
            handle,
            kChannelKey,
            &candidate.channel
        );

    if (channel_result != ESP_OK) {
        nvs_close(handle);
        return SettingsStorageStatus::ReadFailed;
    }

    nvs_close(handle);

    settings = candidate;

    return SettingsStorageStatus::Configured;
}

SettingsStorageWriteStatus writeNodeSettingsToStorage(
    const NodeSettings& settings
) {
    const std::uint8_t stored_transport =
        static_cast<std::uint8_t>(settings.transport);

    if (
        stored_transport <
            static_cast<std::uint8_t>(TransportType::EspNow) ||
        stored_transport >
            static_cast<std::uint8_t>(TransportType::Ethernet)
    ) {
        return SettingsStorageWriteStatus::WriteFailed;
    }

    nvs_handle_t handle{};

    const esp_err_t open_result =
        nvs_open(
            kSettingsNamespace,
            NVS_READWRITE,
            &handle
        );

    if (open_result != ESP_OK) {
        return SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t transport_result =
        nvs_set_u8(
            handle,
            kTransportKey,
            stored_transport
        );

    if (transport_result != ESP_OK) {
        nvs_close(handle);
        return SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t channel_result =
        nvs_set_u16(
            handle,
            kChannelKey,
            settings.channel
        );

    if (channel_result != ESP_OK) {
        nvs_close(handle);
        return SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t configured_result =
        nvs_set_u8(
            handle,
            kConfiguredKey,
            1
        );

    if (configured_result != ESP_OK) {
        nvs_close(handle);
        return SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t commit_result =
        nvs_commit(handle);

    nvs_close(handle);

    if (commit_result != ESP_OK) {
        return SettingsStorageWriteStatus::WriteFailed;
    }

    return SettingsStorageWriteStatus::Ok;
}
