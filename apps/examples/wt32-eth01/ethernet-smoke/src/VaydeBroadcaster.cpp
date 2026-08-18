#include "VaydeBroadcaster.h"

#include <Arduino.h>

#include <array>
#include <cstring>

#include <esp_eth.h>

#include "AppConfig.h"
#include "EthernetNetwork.h"

namespace {

constexpr size_t kFrameSize = AppConfig::kEthernetHeaderSize + sizeof(Packet);

static_assert(sizeof(Packet) == 220, "The canonical VaydeNet Packet must remain 220 bytes");
static_assert(
    sizeof(AppConfig::kPayloadMessage) <= sizeof(Packet::payload),
    "Payload message is too large");
static_assert(kFrameSize == 234, "Unexpected Ethernet frame size");

}  // namespace

VaydeBroadcaster::VaydeBroadcaster(EthernetNetwork &network)
    : network_(network) {
}

void VaydeBroadcaster::begin() {
    packet_ = {};
    packet_.length = static_cast<uint16_t>(sizeof(AppConfig::kPayloadMessage));
    std::memcpy(
        packet_.payload,
        AppConfig::kPayloadMessage,
        sizeof(AppConfig::kPayloadMessage));

    // VaydeNet does not yet define canonical values for version, type, flags,
    // TTL, or the CRC algorithm. Their zero values are preserved here rather
    // than creating transport-specific protocol semantics.
    Serial.println("VaydeNet WT32-ETH01 raw Ethernet broadcaster");
    Serial.println("Destination: FF:FF:FF:FF:FF:FF");
    Serial.printf(
        "EtherType: 0x%04X, VaydeNet packet: %u bytes, Ethernet frame: %u bytes\n",
        AppConfig::kExperimentalEtherType,
        static_cast<unsigned>(sizeof(Packet)),
        static_cast<unsigned>(kFrameSize));
}

void VaydeBroadcaster::update(uint32_t nowMs) {
    if (!network_.ready()
        || nowMs - lastTransmitMs_ < AppConfig::kTransmitIntervalMs) {
        return;
    }

    lastTransmitMs_ = nowMs;
    ++statistics_.attempts;

    const esp_err_t result = transmit();
    statistics_.lastResult = result;

    if (result == ESP_OK) {
        ++statistics_.driverAccepted;
        statistics_.frameBytesAccepted += kFrameSize;
        Serial.printf(
            "Broadcast sequence %lu accepted by Ethernet driver\n",
            static_cast<unsigned long>(packet_.sequenceNumber));
        return;
    }

    ++statistics_.driverRejected;
    Serial.printf(
        "Broadcast sequence %lu rejected: %s (%d)\n",
        static_cast<unsigned long>(packet_.sequenceNumber),
        esp_err_to_name(result),
        static_cast<int>(result));
}

void VaydeBroadcaster::printStatistics() const {
    Serial.println("--- Ethernet broadcast statistics ---");
    network_.printStatus();
    Serial.printf(
        "TX attempts: %lu, driver accepted: %lu, driver rejected: %lu\n",
        static_cast<unsigned long>(statistics_.attempts),
        static_cast<unsigned long>(statistics_.driverAccepted),
        static_cast<unsigned long>(statistics_.driverRejected));
    Serial.printf(
        "Accepted frame bytes: %llu, last result: %s (%d)\n",
        static_cast<unsigned long long>(statistics_.frameBytesAccepted),
        esp_err_to_name(statistics_.lastResult),
        static_cast<int>(statistics_.lastResult));
    Serial.println("Driver acceptance does not confirm reception by the switch or router.");
}

VaydeBroadcaster::Snapshot VaydeBroadcaster::snapshot() const {
    Snapshot current;
    current.attempts = statistics_.attempts;
    current.driverAccepted = statistics_.driverAccepted;
    current.driverRejected = statistics_.driverRejected;
    current.frameBytesAccepted = statistics_.frameBytesAccepted;
    current.sequenceNumber = packet_.sequenceNumber;
    current.lastResult = statistics_.lastResult;
    return current;
}

uint64_t VaydeBroadcaster::senderIdFromMac(const uint8_t mac[6]) {
    uint64_t senderId = 0;
    for (size_t index = 0; index < 6; ++index) {
        senderId = (senderId << 8U) | mac[index];
    }
    return senderId;
}

esp_err_t VaydeBroadcaster::transmit() {
    const esp_eth_handle_t handle = network_.driverHandle();
    if (handle == nullptr || !network_.ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    std::array<uint8_t, kFrameSize> frame{};
    std::memset(frame.data(), 0xFF, 6);

    uint8_t *sourceMac = frame.data() + 6;
    if (!network_.readMac(sourceMac)) {
        return ESP_FAIL;
    }
    packet_.senderID = senderIdFromMac(sourceMac);

    frame[12] = static_cast<uint8_t>(AppConfig::kExperimentalEtherType >> 8U);
    frame[13] = static_cast<uint8_t>(AppConfig::kExperimentalEtherType & 0xFFU);

    ++packet_.sequenceNumber;
    std::memcpy(
        frame.data() + AppConfig::kEthernetHeaderSize,
        &packet_,
        sizeof(packet_));

    return esp_eth_transmit(handle, frame.data(), frame.size());
}
