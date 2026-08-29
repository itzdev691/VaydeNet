#include "EspNowTransport.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace {

constexpr std::uint16_t kMinimumWifiChannel = 1;
constexpr std::uint16_t kMaximumWifiChannel = 14;

}  // namespace

bool EspNowTransport::configureChannel(std::uint16_t channel) {
    if (
        channel < kMinimumWifiChannel ||
        channel > kMaximumWifiChannel
    ) {
        return false;
    }

    if (initialized_) {
        return channel_ == channel;
    }

    channel_ = static_cast<std::uint8_t>(channel);
    return true;
}

TransportStatus EspNowTransport::initialize() {
    if (initialized_) {
        return TransportStatus::Ok;
    }

    if (channel_ == 0) {
        return TransportStatus::InitializationFailed;
    }

    if (nvs_flash_init() != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    if (esp_netif_init() != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    if (esp_event_loop_create_default() != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    wifi_init_config_t wifi_configuration = WIFI_INIT_CONFIG_DEFAULT();

    if (esp_wifi_init(&wifi_configuration) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    if (esp_wifi_start() != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    if (
        esp_wifi_set_channel(
            channel_,
            WIFI_SECOND_CHAN_NONE
        ) != ESP_OK
    ) {
        return TransportStatus::InitializationFailed;
    }

    if (esp_now_init() != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    initialized_ = true;
    return TransportStatus::Ok;
}
