#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <esp_arduino_version.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <VaydeNet/packet/Packet.h>

namespace {

constexpr int8_t kTftCs = 10;
constexpr int8_t kTftDc = 9;
constexpr int8_t kTftRst = 8;
constexpr int8_t kTftSck = 12;
constexpr int8_t kTftMosi = 11;
constexpr uint8_t kEspNowChannel = 1;

constexpr uint16_t kTftBlack = 0x0000;
constexpr uint16_t kTftWhite = 0xFFFF;
constexpr uint16_t kTftRed = 0xF800;

Packet incomingPacket{};
volatile bool packetReady = false;

Arduino_DataBus *bus = new Arduino_ESP32SPI(kTftDc, kTftCs, kTftSck, kTftMosi, -1);
Arduino_GFX *display = new Arduino_ILI9488_18bit(bus, kTftRst, 2, false);

void drawPacketLine(int16_t &y, const char *label, uint64_t value) {
    char valueText[21];
    snprintf(valueText, sizeof(valueText), "%llu", static_cast<unsigned long long>(value));
    display->setCursor(0, y);
    display->print(label);
    display->print(valueText);
    y += 28;
}

void drawPacketLine(int16_t &y, const char *label, uint32_t value) {
    display->setCursor(0, y);
    display->print(label);
    display->print(value);
    y += 28;
}

void drawPacketLine(int16_t &y, const char *label, uint16_t value) {
    display->setCursor(0, y);
    display->print(label);
    display->print(value);
    y += 28;
}

void drawPacketLine(int16_t &y, const char *label, uint8_t value) {
    display->setCursor(0, y);
    display->print(label);
    display->print(value);
    y += 28;
}

void copyPayloadText(const Packet &packet, char (&message)[201]) {
    const size_t messageLength = strnlen(
        reinterpret_cast<const char *>(packet.payload),
        sizeof(packet.payload));
    memcpy(message, packet.payload, messageLength);
    message[messageLength] = '\0';
}

void renderPacketToDisplay(const Packet &packet) {
    display->fillScreen(kTftBlack);
    display->setCursor(0, 0);
    display->setTextSize(4);
    display->setTextColor(kTftRed, kTftBlack);
    display->print("Vayde");
    display->setTextColor(kTftWhite, kTftBlack);
    display->println("ESP");
    display->setTextSize(2);

    int16_t y = 44;
    drawPacketLine(y, "Ver: ", packet.version);
    drawPacketLine(y, "Type: ", packet.type);
    drawPacketLine(y, "Flags: ", packet.flags);
    drawPacketLine(y, "TTL: ", packet.ttl);
    drawPacketLine(y, "Len: ", packet.length);
    drawPacketLine(y, "Sender: ", packet.senderID);
    drawPacketLine(y, "Seq: ", packet.sequenceNumber);

    char message[201];
    copyPayloadText(packet, message);
    display->setCursor(0, y);
    display->print("Msg: ");
    display->println(message);
    y += 24;

    display->setCursor(0, y);
    display->print("CRC: ");
    display->println(packet.crc);
}

void printPacketToSerial(const Packet &packet) {
    char message[201];
    copyPayloadText(packet, message);

    Serial.println("Packet received");
    Serial.printf("Version: %u\n", packet.version);
    Serial.printf("Type: %u\n", packet.type);
    Serial.printf("Flags: %u\n", packet.flags);
    Serial.printf("TTL: %u\n", packet.ttl);
    Serial.printf("Length: %u\n", packet.length);
    Serial.printf("Sender ID: %llu\n", static_cast<unsigned long long>(packet.senderID));
    Serial.printf("Sequence: %lu\n", static_cast<unsigned long>(packet.sequenceNumber));
    Serial.printf("Message: %s\n", message);
    Serial.printf("CRC: %u\n\n", packet.crc);
}

void handleReceivedData(const uint8_t *data, int length) {
    if (length != static_cast<int>(sizeof(Packet))) {
        Serial.printf("Unexpected packet size: %d\n", length);
        return;
    }

    memcpy(&incomingPacket, data, sizeof(incomingPacket));
    packetReady = true;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataReceived(const esp_now_recv_info_t *receiveInfo, const uint8_t *data, int length) {
    (void)receiveInfo;
#else
void onDataReceived(const uint8_t *macAddress, const uint8_t *data, int length) {
    (void)macAddress;
#endif
    handleReceivedData(data, length);
}

bool startEspNow() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_wifi_set_channel(kEspNowChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        Serial.println("Set channel failed");
        return false;
    }

    Serial.print("Receiver MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.printf("ESP-NOW channel: %u\n", kEspNowChannel);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;
    }

    if (esp_now_register_recv_cb(onDataReceived) != ESP_OK) {
        Serial.println("Register receive callback failed");
        return false;
    }

    return true;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("VaydeESP receiver booting");

    if (!display->begin()) {
        Serial.println("Display init failed");
    }
    display->fillScreen(kTftBlack);
    display->setRotation(2);
    display->setTextWrap(true);

    if (startEspNow()) {
        Serial.println("ESP-NOW receiver ready");
    }
}

void loop() {
    if (packetReady) {
        packetReady = false;
        const Packet packet = incomingPacket;
        printPacketToSerial(packet);
        renderPacketToDisplay(packet);
    }

    static uint32_t lastHeartbeatMs = 0;
    if (millis() - lastHeartbeatMs >= 5000) {
        lastHeartbeatMs = millis();
        Serial.println("Receiver alive");
    }

    delay(10);
}
