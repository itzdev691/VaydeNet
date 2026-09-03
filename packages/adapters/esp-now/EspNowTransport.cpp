#include "EspNowTransport.h"

#include <cstring>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace {

constexpr std::uint16_t kMinimumWifiChannel = 1;
constexpr std::uint16_t kMaximumWifiChannel = 14;
constexpr UBaseType_t kReceiveQueueDepth = 4;
constexpr char kLogTag[] = "EspNowTransport";

}  // namespace

EspNowTransport* EspNowTransport::active_instance_ = nullptr;

void EspNowTransport::setReceiveActivityCallback(
    EspNowReceiveActivityCallback callback,
    void* context
) {
    receive_activity_callback_ = callback;
    receive_activity_context_ = context;
}

void EspNowTransport::onDataReceived(
    const esp_now_recv_info_t* receive_info,
    const std::uint8_t* data,
    int data_length
) {
    if (
        active_instance_ == nullptr ||
        receive_info == nullptr ||
        receive_info->src_addr == nullptr ||
        data == nullptr ||
        data_length <= 0
    ) {
        ESP_LOGW(kLogTag, "Invalid ESP-NOW receive callback data");
        return;
    }

    ESP_LOGI(
        kLogTag,
        "ESPNOW RX sender=" MACSTR " bytes=%d",
        MAC2STR(receive_info->src_addr),
        data_length
    );

    active_instance_->enqueueReceivedData(data, data_length);
}

void EspNowTransport::enqueueReceivedData(
    const std::uint8_t* data,
    int data_length
) {
    if (receive_queue_ == nullptr || data == nullptr) {
        return;
    }

    if (data_length != static_cast<int>(sizeof(Packet))) {
        ESP_LOGW(
            kLogTag,
            "Dropped ESP-NOW frame with unexpected size: %d",
            data_length
        );
        return;
    }

    Packet packet{};
    std::memcpy(&packet, data, sizeof(packet));

    if (receive_activity_callback_ != nullptr) {
        receive_activity_callback_(receive_activity_context_);
    }

    if (xQueueSend(receive_queue_, &packet, 0) != pdTRUE) {
        ESP_LOGW(kLogTag, "Receive queue full; packet dropped");
    }
}

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

    if (active_instance_ != nullptr && active_instance_ != this) {
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    receive_queue_ = xQueueCreate(kReceiveQueueDepth, sizeof(Packet));

    if (receive_queue_ == nullptr) {
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    active_instance_ = this;

    if (esp_now_register_recv_cb(EspNowTransport::onDataReceived) != ESP_OK) {
        active_instance_ = nullptr;
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    initialized_ = true;
    return TransportStatus::Ok;
}

EspNowReceiveStatus EspNowTransport::tryReceive(Packet& packet) {
    if (!initialized_ || receive_queue_ == nullptr) {
        return EspNowReceiveStatus::NotInitialized;
    }

    if (xQueueReceive(receive_queue_, &packet, 0) != pdTRUE) {
        return EspNowReceiveStatus::Empty;
    }

    return EspNowReceiveStatus::Received;
}
