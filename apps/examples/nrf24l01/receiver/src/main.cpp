#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

#include "../../nRF24fragment.h"

namespace {

constexpr uint8_t kCePin = 0;
constexpr uint8_t kCsnPin = 1;
constexpr uint8_t kMisoPin = 3;
constexpr uint8_t kSckPin = 4;
constexpr uint8_t kMosiPin = 5;

constexpr uint8_t kRadioChannel = 76;
constexpr uint8_t kPipeAddress[6] = "VYD01";

RF24 radio(kCePin, kCsnPin);
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
    radio.setPayloadSize(sizeof(Nrf24Fragment));

    radio.openReadingPipe(1, kPipeAddress);
    radio.startListening();

    return true;
}

void printFragment(const Nrf24Fragment& fragment) {
    Serial.printf(
        "Vayde fragment: sender=%llu sequence=%lu fragment=%u/%u length=%u data=",
        static_cast<unsigned long long>(fragment.senderID),
        static_cast<unsigned long>(fragment.sequenceNumber),
        fragment.fragmentIndex + 1,
        fragment.fragmentCount,
        fragment.length);

    for (uint8_t byte : fragment.data) {
        Serial.printf("%02X ", byte);
    }

    Serial.println();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    radioReady = startRadio();
    Serial.println(radioReady
        ? "nRF24L01+ receiver ready"
        : "nRF24L01+ receiver failed");
}

void loop() {
    if (!radioReady) {
        delay(1000);
        return;
    }

    while (radio.available()) {
        Nrf24Fragment fragment{};
        radio.read(&fragment, sizeof(fragment));
        printFragment(fragment);
    }

    delay(10);
}