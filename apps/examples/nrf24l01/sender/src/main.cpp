#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include <stdint.h>
#include <stdio.h>

namespace {

constexpr uint8_t kSckPin = 4;
constexpr uint8_t kMisoPin = 5;
constexpr uint8_t kMosiPin = 6;
constexpr uint8_t kCsnPin = 7;
constexpr uint8_t kCePin = 8;

constexpr uint8_t kRadioChannel = 76;
constexpr uint32_t kSendIntervalMs = 1000;
constexpr uint8_t kPipeAddress[6] = "VYD01";

struct RadioPayload {
    uint32_t sequenceNumber;
    char message[28];
};

static_assert(sizeof(RadioPayload) == 32, "nRF24L01+ payload must not exceed 32 bytes");

RF24 radio(kCePin, kCsnPin);
uint32_t sequenceNumber = 0;
bool radioReady = false;

bool startRadio() {
    SPI.begin(kSckPin, kMisoPin, kMosiPin, kCsnPin);

    if (!radio.begin(&SPI)) {
        Serial.println("nRF24L01+ not detected");
        return false;
    }

    radio.setChannel(kRadioChannel);
    radio.setDataRate(RF24_1MBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.setPALevel(RF24_PA_MIN);
    radio.setAutoAck(true);
    radio.setRetries(5, 15);
    radio.openWritingPipe(kPipeAddress);
    radio.stopListening();

    return true;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("VaydeNet nRF24L01+ sender booting");

    radioReady = startRadio();
    if (radioReady) {
        Serial.println("nRF24L01+ ready");
    }
}

void loop() {
    if (!radioReady) {
        delay(kSendIntervalMs);
        return;
    }

    RadioPayload payload{};
    payload.sequenceNumber = ++sequenceNumber;
    snprintf(payload.message, sizeof(payload.message), "Hello from VaydeNet");

    const bool sent = radio.write(&payload, sizeof(payload));
    Serial.printf(
        "Packet %lu: %s\n",
        static_cast<unsigned long>(payload.sequenceNumber),
        sent ? "ACK received" : "send failed");

    delay(kSendIntervalMs);
}
