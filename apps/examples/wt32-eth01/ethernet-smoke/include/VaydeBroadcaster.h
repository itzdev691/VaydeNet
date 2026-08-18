#pragma once

#include <cstdint>

#include <esp_err.h>

#include <VaydeNet/packet/Packet.h>

class EthernetNetwork;

class VaydeBroadcaster final {
public:
    struct Snapshot {
        uint32_t attempts = 0;
        uint32_t driverAccepted = 0;
        uint32_t driverRejected = 0;
        uint64_t frameBytesAccepted = 0;
        uint32_t sequenceNumber = 0;
        esp_err_t lastResult = ESP_OK;
    };

    explicit VaydeBroadcaster(EthernetNetwork &network);

    void begin();
    void update(uint32_t nowMs);
    void printStatistics() const;
    Snapshot snapshot() const;

private:
    struct TransmitStatistics {
        uint32_t attempts = 0;
        uint32_t driverAccepted = 0;
        uint32_t driverRejected = 0;
        uint64_t frameBytesAccepted = 0;
        esp_err_t lastResult = ESP_OK;
    };

    static uint64_t senderIdFromMac(const uint8_t mac[6]);
    esp_err_t transmit();

    EthernetNetwork &network_;
    Packet packet_{};
    TransmitStatistics statistics_{};
    uint32_t lastTransmitMs_ = 0;
};
