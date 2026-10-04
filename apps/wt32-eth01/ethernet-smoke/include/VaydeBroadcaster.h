#pragma once

#include <atomic>
#include <cstdint>

#include <esp_err.h>
#include <esp_now.h>

#include <VaydeNet/VaydeEngine.h>
#include <VaydeNet/transport/TransportInterface.h>
#include <VaydeNet/message/MessageSink.h>
#include <VaydeNet/startup/HardwareIdentity.h>
#include <VaydeNet/config/NodeSettings.h>
#include <VaydeNet/transmit/TransmitRequest.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class EthernetNetwork;
class PortMonitor;

class VaydeBroadcaster final : private TransportInterface, private MessageSink {
public:
    struct Snapshot {
        bool ready = false;
        uint8_t channel = 0;
        uint8_t stationMac[6]{};
        uint32_t attempts = 0;
        uint32_t queueAccepted = 0;
        uint32_t queueRejected = 0;
        uint32_t deliverySucceeded = 0;
        uint32_t deliveryFailed = 0;
        uint64_t packetBytesQueued = 0;
        uint32_t sequenceNumber = 0;
        esp_err_t lastQueueResult = ESP_OK;
        bool deliveryStatusAvailable = false;
        esp_now_send_status_t lastDeliveryStatus = ESP_NOW_SEND_FAIL;
    };

    VaydeBroadcaster(
        EthernetNetwork &network,
        const PortMonitor &portMonitor);

    bool begin();
    void update(uint32_t nowMs);
    void printStatistics() const;
    Snapshot snapshot() const;

private:
    static void onDataSent(
        const uint8_t *peerAddress,
        esp_now_send_status_t status);
    TransmitRequest prepareRequest() const;
    void drainQueue();
    TransportStatus initialize() override;
    TransportReceiveStatus tryReceive(Packet&) override;
    TransportTransmitStatus tryTransmit(const Packet&) override;
    TransportTransmitCompletionStatus pollTransmitCompletion() override;
    MessageSinkStatus deliver(const Message&) override;

    VaydeEngine engine_{};
    HardwareIdentity identity_{};
    NodeSettings settings_{};
    static constexpr size_t kQueueDepth = 4;
    StaticQueue_t outboundQueueControl_{};
    uint8_t outboundQueueStorage_[kQueueDepth * sizeof(TransmitRequest)]{};
    QueueHandle_t outboundQueue_{nullptr};
    bool transmitPending_{false};
    std::atomic<int> completion_{-1};

    static VaydeBroadcaster *activeInstance_;

    EthernetNetwork &network_;
    const PortMonitor &portMonitor_;
    uint8_t stationMac_[6]{};
    std::atomic<bool> ready_{false};
    std::atomic<uint32_t> attempts_{0};
    std::atomic<uint32_t> queueAccepted_{0};
    std::atomic<uint32_t> queueRejected_{0};
    std::atomic<uint32_t> deliverySucceeded_{0};
    std::atomic<uint32_t> deliveryFailed_{0};
    std::atomic<uint64_t> packetBytesQueued_{0};
    std::atomic<uint32_t> sequenceNumber_{0};
    std::atomic<esp_err_t> lastQueueResult_{ESP_OK};
    std::atomic<bool> deliveryStatusAvailable_{false};
    std::atomic<esp_now_send_status_t> lastDeliveryStatus_{ESP_NOW_SEND_FAIL};
    uint32_t lastTransmitMs_ = 0;
};
