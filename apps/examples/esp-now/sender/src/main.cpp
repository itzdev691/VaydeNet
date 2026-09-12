#include <Arduino.h>
#include <WiFi.h>
#include <cstddef>
#include <esp_arduino_version.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <VaydeNet/packet/Packet.h>

namespace {

constexpr uint8_t kEspNowChannel = 1;
constexpr uint32_t kSendIntervalMs = 1000;
constexpr uint8_t kPacketVersion = 1;
constexpr uint8_t kPacketType = 1;
constexpr uint8_t kPacketTtl = 1;
constexpr uint16_t kCrcInitialValue = 0xFFFFU;
constexpr uint16_t kCrcPolynomial = 0x1021U;

// Replace with the receiver's MAC address. FF:FF:FF:FF:FF:FF broadcasts.
uint8_t peerAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const char *message = "Hello from VaydeESP";

Packet packet{};
bool espNowReady = false;

uint16_t computePacketCrc(const Packet& packetToChecksum) {
    const auto* bytes =
        reinterpret_cast<const uint8_t*>(&packetToChecksum);
    uint16_t crc = kCrcInitialValue;

    for (size_t index = 0; index < offsetof(Packet, crc); ++index) {
        crc ^= static_cast<uint16_t>(bytes[index]) << 8U;

        for (uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0) {
                crc = static_cast<uint16_t>(
                    (crc << 1U) ^ kCrcPolynomial
                );
            } else {
                crc = static_cast<uint16_t>(crc << 1U);
            }
        }
    }

    return crc;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataSent(const esp_now_send_info_t *txInfo, esp_now_send_status_t status) {
    (void)txInfo;
#else
void onDataSent(const uint8_t *macAddress, esp_now_send_status_t status) {
    (void)macAddress;
#endif
    Serial.print("Send status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

bool startEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        Serial.println("Set channel failed");
        return false;
    }

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;
    }

    if (esp_now_register_send_cb(onDataSent) != ESP_OK) {
        Serial.println("Register send callback failed");
        return false;
    }

    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, peerAddress, sizeof(peerAddress));
    peerInfo.channel = kEspNowChannel;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Add peer failed");
        return false;
    }

    return true;
}

void preparePacket() {
    packet.version = kPacketVersion;
    packet.type = kPacketType;
    packet.ttl = kPacketTtl;

    const size_t messageLength = strnlen(message, sizeof(packet.payload) - 1);
    memset(packet.payload, 0, sizeof(packet.payload));
    memcpy(packet.payload, message, messageLength);
    packet.length = static_cast<uint16_t>(messageLength);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("VaydeESP sender booting");

    preparePacket();
    espNowReady = startEspNow();
}

void loop() {
    if (!espNowReady) {
        delay(kSendIntervalMs);
        return;
    }

    ++packet.sequenceNumber;
    packet.crc = computePacketCrc(packet);
    const esp_err_t result = esp_now_send(
        peerAddress,
        reinterpret_cast<const uint8_t *>(&packet),
        sizeof(packet));

    Serial.println(result == ESP_OK ? "Sent packet" : "Send failed");
    delay(kSendIntervalMs);
}
