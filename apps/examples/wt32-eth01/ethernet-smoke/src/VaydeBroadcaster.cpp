#include "VaydeBroadcaster.h"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <esp_now.h>
#include <esp_wifi.h>

#include "AppConfig.h"
#include "EthernetNetwork.h"
#include "PortMonitor.h"

namespace {

constexpr uint8_t kBroadcastAddress[6] = {
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
};

static_assert(sizeof(Packet) == 220, "The canonical VaydeNet Packet must remain 220 bytes");
static_assert(sizeof(Packet) <= ESP_NOW_MAX_DATA_LEN, "VaydeNet Packet exceeds ESP-NOW payload limit");

}  // namespace

VaydeBroadcaster *VaydeBroadcaster::activeInstance_ = nullptr;

VaydeBroadcaster::VaydeBroadcaster(
    EthernetNetwork &network,
    const PortMonitor &portMonitor)
    : network_(network),
      portMonitor_(portMonitor) {
}

bool VaydeBroadcaster::begin() {
    ready_.store(false);

    if (!WiFi.mode(WIFI_STA)) {
        Serial.println("ESP-NOW failed: Wi-Fi station mode unavailable");
        return false;
    }
    WiFi.disconnect();

    esp_err_t result = esp_wifi_set_channel(
        AppConfig::kEspNowChannel,
        WIFI_SECOND_CHAN_NONE);
    if (result != ESP_OK) {
        Serial.printf(
            "ESP-NOW failed to set channel %u: %s (%d)\n",
            AppConfig::kEspNowChannel,
            esp_err_to_name(result),
            static_cast<int>(result));
        return false;
    }

    result = esp_now_init();
    if (result != ESP_OK) {
        Serial.printf(
            "ESP-NOW initialization failed: %s (%d)\n",
            esp_err_to_name(result),
            static_cast<int>(result));
        return false;
    }

    activeInstance_ = this;
    result = esp_now_register_send_cb(onDataSent);
    if (result != ESP_OK) {
        Serial.printf(
            "ESP-NOW send callback registration failed: %s (%d)\n",
            esp_err_to_name(result),
            static_cast<int>(result));
        activeInstance_ = nullptr;
        esp_now_deinit();
        return false;
    }

    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, kBroadcastAddress, sizeof(kBroadcastAddress));
    peer.channel = AppConfig::kEspNowChannel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
        Serial.printf(
            "ESP-NOW broadcast peer registration failed: %s (%d)\n",
            esp_err_to_name(result),
            static_cast<int>(result));
        activeInstance_ = nullptr;
        esp_now_deinit();
        return false;
    }

    result = esp_wifi_get_mac(WIFI_IF_STA, stationMac_);
    if (result != ESP_OK) {
        Serial.printf(
            "ESP-NOW failed to read station MAC: %s (%d)\n",
            esp_err_to_name(result),
            static_cast<int>(result));
        activeInstance_ = nullptr;
        esp_now_deinit();
        return false;
    }

    ready_.store(true);
    Serial.println("VaydeNet WT32-ETH01 ESP-NOW broadcaster");
    Serial.println("Destination: FF:FF:FF:FF:FF:FF");
    Serial.printf(
        "Channel: %u, VaydeNet packet: %u bytes\n",
        AppConfig::kEspNowChannel,
        static_cast<unsigned>(sizeof(Packet)));
    Serial.printf(
        "Station MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        stationMac_[0],
        stationMac_[1],
        stationMac_[2],
        stationMac_[3],
        stationMac_[4],
        stationMac_[5]);
    return true;
}

void VaydeBroadcaster::update(uint32_t nowMs) {
    if (!ready_.load()
        || nowMs - lastTransmitMs_ < AppConfig::kTransmitIntervalMs) {
        return;
    }

    lastTransmitMs_ = nowMs;
    ++attempts_;

    const esp_err_t result = transmit();
    lastQueueResult_.store(result);

    if (result == ESP_OK) {
        ++queueAccepted_;
        packetBytesQueued_ += sizeof(Packet);
        Serial.printf(
            "ESP-NOW sequence %lu queued\n",
            static_cast<unsigned long>(packet_.sequenceNumber));
        return;
    }

    ++queueRejected_;
    Serial.printf(
        "ESP-NOW sequence %lu queue rejected: %s (%d)\n",
        static_cast<unsigned long>(packet_.sequenceNumber),
        esp_err_to_name(result),
        static_cast<int>(result));
}

void VaydeBroadcaster::printStatistics() const {
    Serial.println("--- Ethernet and ESP-NOW statistics ---");
    network_.printStatus();
    Serial.printf(
        "ESP-NOW channel: %u, attempts: %lu, queued: %lu, rejected: %lu\n",
        AppConfig::kEspNowChannel,
        static_cast<unsigned long>(attempts_.load()),
        static_cast<unsigned long>(queueAccepted_.load()),
        static_cast<unsigned long>(queueRejected_.load()));
    Serial.printf(
        "Delivery callbacks: %lu success, %lu failed; queued bytes: %llu\n",
        static_cast<unsigned long>(deliverySucceeded_.load()),
        static_cast<unsigned long>(deliveryFailed_.load()),
        static_cast<unsigned long long>(packetBytesQueued_.load()));
    const esp_err_t lastResult = lastQueueResult_.load();
    Serial.printf(
        "Last queue result: %s (%d)\n",
        esp_err_to_name(lastResult),
        static_cast<int>(lastResult));
    Serial.println("Receiver output is required to confirm end-to-end delivery.");
}

VaydeBroadcaster::Snapshot VaydeBroadcaster::snapshot() const {
    Snapshot current;
    current.ready = ready_.load();
    current.channel = AppConfig::kEspNowChannel;
    std::memcpy(current.stationMac, stationMac_, sizeof(stationMac_));
    current.attempts = attempts_.load();
    current.queueAccepted = queueAccepted_.load();
    current.queueRejected = queueRejected_.load();
    current.deliverySucceeded = deliverySucceeded_.load();
    current.deliveryFailed = deliveryFailed_.load();
    current.packetBytesQueued = packetBytesQueued_.load();
    current.sequenceNumber = sequenceNumber_.load();
    current.lastQueueResult = lastQueueResult_.load();
    current.deliveryStatusAvailable = deliveryStatusAvailable_.load();
    current.lastDeliveryStatus = lastDeliveryStatus_.load();
    return current;
}

void VaydeBroadcaster::onDataSent(
    const uint8_t *peerAddress,
    esp_now_send_status_t status) {
    (void)peerAddress;

    VaydeBroadcaster *instance = activeInstance_;
    if (instance == nullptr) {
        return;
    }

    instance->deliveryStatusAvailable_.store(true);
    instance->lastDeliveryStatus_.store(status);

    if (status == ESP_NOW_SEND_SUCCESS) {
        ++instance->deliverySucceeded_;
        return;
    }

    ++instance->deliveryFailed_;
}

uint64_t VaydeBroadcaster::senderIdFromMac(const uint8_t mac[6]) {
    uint64_t senderId = 0;
    for (size_t index = 0; index < 6; ++index) {
        senderId = (senderId << 8U) | mac[index];
    }
    return senderId;
}

void VaydeBroadcaster::preparePacket() {
    packet_ = {};
    packet_.senderID = senderIdFromMac(stationMac_);
    packet_.sequenceNumber = ++sequenceNumber_;

    const EthernetNetwork::Snapshot network = network_.snapshot();
    const PortMonitor::Snapshot ports = portMonitor_.snapshot();
    const String ipv4 = network.dhcpReady ? network.ipv4.toString() : String("--");
    const String gateway = network.dhcpReady ? network.gateway.toString() : String("--");

    int written = std::snprintf(
        reinterpret_cast<char *>(packet_.payload),
        sizeof(packet_.payload),
        "ETH link=%s dhcp=%s ip=%s gateway=%s speed=%luMbps duplex=%s tcp=",
        network.linkUp ? "up" : "down",
        network.dhcpReady ? "ready" : "waiting",
        ipv4.c_str(),
        gateway.c_str(),
        static_cast<unsigned long>(network.speedMbps),
        network.linkUp ? (network.fullDuplex ? "full" : "half") : "unknown");

    if (written < 0) {
        packet_.length = 0;
        packet_.payload[0] = '\0';
        return;
    }

    size_t used = std::min(
        static_cast<size_t>(written),
        sizeof(packet_.payload) - 1);

    for (size_t index = 0;
         index < ports.results.size() && used < sizeof(packet_.payload) - 1;
         ++index) {
        const PortMonitor::Result &probe = ports.results[index];
        written = std::snprintf(
            reinterpret_cast<char *>(packet_.payload) + used,
            sizeof(packet_.payload) - used,
            "%s%u:%c",
            index == 0 ? "" : ",",
            probe.port,
            !probe.tested ? '?' : (probe.open ? 'O' : 'C'));

        if (written < 0) {
            break;
        }
        used += std::min(
            static_cast<size_t>(written),
            sizeof(packet_.payload) - used - 1);
    }

    packet_.payload[sizeof(packet_.payload) - 1] = '\0';
    packet_.length = static_cast<uint16_t>(
        strnlen(
            reinterpret_cast<const char *>(packet_.payload),
            sizeof(packet_.payload))
        + 1);

    // VaydeNet does not yet define canonical values for version, type, flags,
    // TTL, or the CRC algorithm. Those fields remain zero instead of inventing
    // ESP-NOW-specific protocol semantics.
}

esp_err_t VaydeBroadcaster::transmit() {
    if (!ready_.load()) {
        return ESP_ERR_INVALID_STATE;
    }

    preparePacket();
    return esp_now_send(
        kBroadcastAddress,
        reinterpret_cast<const uint8_t *>(&packet_),
        sizeof(packet_));
}
