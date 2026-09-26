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

constexpr UBaseType_t kTransmitCompletionQueueDepth = 1;
constexpr std::uint8_t kBroadcastAddress[ESP_NOW_ETH_ALEN] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
constexpr std::uint16_t kMinimumWifiChannel = 1;
constexpr std::uint16_t kMaximumWifiChannel = 14;
constexpr UBaseType_t kReceiveQueueDepth = 4;
constexpr char kLogTag[] = "EspNowTransport";

}  // namespace

EspNowTransport* EspNowTransport::active_instance_ = nullptr;

EspNowTransport::~EspNowTransport() {
    shutdown();
}

void EspNowTransport::shutdown() {
    if (active_instance_ == this) {
        if (initialized_) {
            (void)esp_now_del_peer(kBroadcastAddress);
        }

        (void)esp_now_unregister_send_cb();
        (void)esp_now_unregister_recv_cb();
        active_instance_ = nullptr;
    }

    if (initialized_) {
        (void)esp_now_deinit();
        initialized_ = false;
    }

    if (receive_queue_ != nullptr) {
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
    }

    if (transmit_completion_queue_ != nullptr) {
        vQueueDelete(transmit_completion_queue_);
        transmit_completion_queue_ = nullptr;
    }

    receive_activity_callback_ = nullptr;
    receive_activity_context_ = nullptr;
    transmit_pending_ = false;
}

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

    transmit_completion_queue_ = xQueueCreate(
        kTransmitCompletionQueueDepth,
        sizeof(TransportTransmitCompletionStatus)
    );

    if (transmit_completion_queue_ == nullptr) {
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    active_instance_ = this;

    if (esp_now_register_recv_cb(EspNowTransport::onDataReceived) != ESP_OK) {
        active_instance_ = nullptr;
        vQueueDelete(transmit_completion_queue_);
        transmit_completion_queue_ = nullptr;
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    if (esp_now_register_send_cb(EspNowTransport::onDataSent) != ESP_OK) {
        (void)esp_now_unregister_recv_cb();
        active_instance_ = nullptr;
        vQueueDelete(transmit_completion_queue_);
        transmit_completion_queue_ = nullptr;
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    esp_now_peer_info_t broadcast_peer{};
    std::memcpy(
        broadcast_peer.peer_addr,
        kBroadcastAddress,
        sizeof(kBroadcastAddress)
    );
    broadcast_peer.channel = channel_;
    broadcast_peer.ifidx = WIFI_IF_STA;
    broadcast_peer.encrypt = false;

    if (esp_now_add_peer(&broadcast_peer) != ESP_OK) {
        (void)esp_now_unregister_send_cb();
        (void)esp_now_unregister_recv_cb();
        active_instance_ = nullptr;
        vQueueDelete(transmit_completion_queue_);
        transmit_completion_queue_ = nullptr;
        vQueueDelete(receive_queue_);
        receive_queue_ = nullptr;
        (void)esp_now_deinit();
        return TransportStatus::InitializationFailed;
    }

    initialized_ = true;
    return TransportStatus::Ok;
}

TransportReceiveStatus EspNowTransport::tryReceive(Packet& packet) {
    if (!initialized_ || receive_queue_ == nullptr) {
        return TransportReceiveStatus::NotInitialized;
    }

    if (xQueueReceive(receive_queue_, &packet, 0) != pdTRUE) {
        return TransportReceiveStatus::Empty;
    }

    return TransportReceiveStatus::Received;
}

TransportTransmitStatus EspNowTransport::tryTransmit(const Packet& packet) {
    if (!initialized_ || transmit_completion_queue_ == nullptr) {
        return TransportTransmitStatus::NotInitialized;
    }

    if (transmit_pending_) {
        return TransportTransmitStatus::Busy;
    }

    transmit_pending_ = true;

    if (
        esp_now_send(
            kBroadcastAddress,
            reinterpret_cast<const std::uint8_t*>(&packet),
            sizeof(packet)
        ) != ESP_OK
    ) {
        transmit_pending_ = false;
        return TransportTransmitStatus::Failed;
    }

    return TransportTransmitStatus::Queued;
}

TransportTransmitCompletionStatus
EspNowTransport::pollTransmitCompletion() {
    if (!initialized_ || transmit_completion_queue_ == nullptr) {
        return TransportTransmitCompletionStatus::NotInitialized;
    }

    TransportTransmitCompletionStatus completion{};

    if (
        xQueueReceive(
            transmit_completion_queue_,
            &completion,
            0
        ) == pdTRUE
    ) {
        transmit_pending_ = false;
        return completion;
    }

    if (transmit_pending_) {
        return TransportTransmitCompletionStatus::Pending;
    }

    return TransportTransmitCompletionStatus::Empty;
}

void EspNowTransport::onDataSent(
    const esp_now_send_info_t*,
    esp_now_send_status_t status
) {
    if (
        active_instance_ == nullptr ||
        active_instance_->transmit_completion_queue_ == nullptr
    ) {
        return;
    }

    const TransportTransmitCompletionStatus completion =
        status == ESP_NOW_SEND_SUCCESS
            ? TransportTransmitCompletionStatus::Sent
            : TransportTransmitCompletionStatus::Failed;

    (void)xQueueSend(
        active_instance_->transmit_completion_queue_,
        &completion,
        0
    );
}
