#include "Esp32NodeSettingsStorage.h"

#include "nvs.h"
#include "nvs_flash.h"

namespace {

constexpr char kSettingsNamespace[] = "vaydenet";
constexpr char kConfiguredKey[] = "configured";
constexpr char kTransportKey[] = "transport";
constexpr char kChannelKey[] = "channel";

Esp32SettingsStorageReadStatus checkEsp32SettingsConfigured(
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
        return Esp32SettingsStorageReadStatus::NotConfigured;
    }

    if (result != ESP_OK) {
        return Esp32SettingsStorageReadStatus::ReadFailed;
    }

    if (configured != 1) {
        return Esp32SettingsStorageReadStatus::NotConfigured;
    }

    return Esp32SettingsStorageReadStatus::Configured;
}

}  // namespace

Esp32SettingsStorageInitializationStatus initializeEsp32NodeSettingsStorage() {
    if (nvs_flash_init() != ESP_OK) {
        return Esp32SettingsStorageInitializationStatus::Failed;
    }

    return Esp32SettingsStorageInitializationStatus::Ok;
}

Esp32SettingsStorageReadStatus readEsp32NodeSettingsFromStorage(
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
        return Esp32SettingsStorageReadStatus::NotConfigured;
    }

    if (open_result != ESP_OK) {
        return Esp32SettingsStorageReadStatus::ReadFailed;
    }

    const Esp32SettingsStorageReadStatus configured_status =
        checkEsp32SettingsConfigured(handle);

    if (configured_status != Esp32SettingsStorageReadStatus::Configured) {
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
        return Esp32SettingsStorageReadStatus::ReadFailed;
    }

    if (
        stored_transport <
            static_cast<std::uint8_t>(TransportType::EspNow) ||
        stored_transport >
            static_cast<std::uint8_t>(TransportType::Ethernet)
    ) {
        nvs_close(handle);
        return Esp32SettingsStorageReadStatus::ReadFailed;
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
        return Esp32SettingsStorageReadStatus::ReadFailed;
    }

    nvs_close(handle);

    settings = candidate;

    return Esp32SettingsStorageReadStatus::Configured;
}

Esp32SettingsStorageWriteStatus writeEsp32NodeSettingsToStorage(
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
        return Esp32SettingsStorageWriteStatus::WriteFailed;
    }

    nvs_handle_t handle{};

    const esp_err_t open_result =
        nvs_open(
            kSettingsNamespace,
            NVS_READWRITE,
            &handle
        );

    if (open_result != ESP_OK) {
        return Esp32SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t transport_result =
        nvs_set_u8(
            handle,
            kTransportKey,
            stored_transport
        );

    if (transport_result != ESP_OK) {
        nvs_close(handle);
        return Esp32SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t channel_result =
        nvs_set_u16(
            handle,
            kChannelKey,
            settings.channel
        );

    if (channel_result != ESP_OK) {
        nvs_close(handle);
        return Esp32SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t configured_result =
        nvs_set_u8(
            handle,
            kConfiguredKey,
            1
        );

    if (configured_result != ESP_OK) {
        nvs_close(handle);
        return Esp32SettingsStorageWriteStatus::WriteFailed;
    }

    const esp_err_t commit_result =
        nvs_commit(handle);

    nvs_close(handle);

    if (commit_result != ESP_OK) {
        return Esp32SettingsStorageWriteStatus::WriteFailed;
    }

    return Esp32SettingsStorageWriteStatus::Ok;
}
