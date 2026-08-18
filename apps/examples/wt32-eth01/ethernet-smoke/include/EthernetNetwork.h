#pragma once

#include <atomic>
#include <cstdint>

#include <IPAddress.h>
#include <WiFi.h>
#include <esp_eth.h>

class EthernetNetwork final {
public:
    struct Snapshot {
        bool linkUp = false;
        bool dhcpReady = false;
        uint8_t mac[6]{};
        IPAddress ipv4{};
        IPAddress subnet{};
        IPAddress gateway{};
        IPAddress dns{};
        uint32_t speedMbps = 0;
        bool fullDuplex = false;
        uint32_t linkUpEvents = 0;
        uint32_t linkDownEvents = 0;
    };

    bool begin();

    bool ready() const;
    bool linkUp() const;
    esp_eth_handle_t driverHandle() const;
    bool readMac(uint8_t mac[6]) const;
    Snapshot snapshot() const;

    void printConfiguration() const;
    void printStatus() const;

private:
    void handleEvent(WiFiEvent_t event, WiFiEventInfo_t info);

    std::atomic<esp_eth_handle_t> driverHandle_{nullptr};
    std::atomic<bool> linkUp_{false};
    std::atomic<bool> dhcpReady_{false};
    std::atomic<uint32_t> linkUpEvents_{0};
    std::atomic<uint32_t> linkDownEvents_{0};
};
