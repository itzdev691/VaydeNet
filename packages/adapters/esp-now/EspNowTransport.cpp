#include "EspNowTransport.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

TransportStatus EspNowTransport::initialize() {
    if (initialized_) {
        return TransportStatus::Ok;
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

    if (esp_now_init() != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    initialized_ = true;
    return TransportStatus::Ok;
}
