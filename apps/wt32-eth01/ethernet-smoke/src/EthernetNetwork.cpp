#include "EthernetNetwork.h"

#include <Arduino.h>
#include <ETH.h>

#include "AppConfig.h"

namespace {

void printMacAddress(const uint8_t mac[6]) {
    Serial.printf(
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

}  // namespace

bool EthernetNetwork::begin() {
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        handleEvent(event, info);
    });

    return ETH.begin();
}

bool EthernetNetwork::ready() const {
    return linkUp_.load() && dhcpReady_.load() && driverHandle_.load() != nullptr;
}

bool EthernetNetwork::linkUp() const {
    return linkUp_.load();
}

esp_eth_handle_t EthernetNetwork::driverHandle() const {
    return driverHandle_.load();
}

bool EthernetNetwork::readMac(uint8_t mac[6]) const {
    const esp_eth_handle_t handle = driverHandle();
    return handle != nullptr
        && esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac) == ESP_OK;
}

EthernetNetwork::Snapshot EthernetNetwork::snapshot() const {
    Snapshot current;
    current.linkUp = linkUp_.load();
    current.dhcpReady = dhcpReady_.load();
    current.linkUpEvents = linkUpEvents_.load();
    current.linkDownEvents = linkDownEvents_.load();

    ETH.macAddress(current.mac);

    if (current.linkUp) {
        current.speedMbps = ETH.linkSpeed();
        current.fullDuplex = ETH.fullDuplex();
    }

    if (current.dhcpReady) {
        current.ipv4 = ETH.localIP();
        current.subnet = ETH.subnetMask();
        current.gateway = ETH.gatewayIP();
        current.dns = ETH.dnsIP();
    }

    return current;
}

void EthernetNetwork::printConfiguration() const {
    uint8_t mac[6]{};
    ETH.macAddress(mac);

    Serial.print("Ethernet MAC: ");
    printMacAddress(mac);
    Serial.println();
    Serial.print("IPv4 address: ");
    Serial.println(ETH.localIP());
    Serial.print("Subnet mask: ");
    Serial.println(ETH.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(ETH.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(ETH.dnsIP());
    Serial.printf(
        "Link: %u Mbps, %s duplex\n",
        ETH.linkSpeed(),
        ETH.fullDuplex() ? "full" : "half");
}

void EthernetNetwork::printStatus() const {
    const bool connected = linkUp();
    Serial.printf(
        "Link: %s, DHCP: %s, speed: %u Mbps, duplex: %s\n",
        connected ? "up" : "down",
        dhcpReady_.load() ? "ready" : "waiting",
        connected ? ETH.linkSpeed() : 0,
        connected && ETH.fullDuplex() ? "full" : "unknown/half");
    Serial.printf(
        "Link-up events: %lu, link-down events: %lu\n",
        static_cast<unsigned long>(linkUpEvents_.load()),
        static_cast<unsigned long>(linkDownEvents_.load()));
}

void EthernetNetwork::handleEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("Ethernet started");
            ETH.setHostname(AppConfig::kHostname);
            break;

        case ARDUINO_EVENT_ETH_CONNECTED:
            driverHandle_.store(info.eth_connected);
            linkUp_.store(true);
            ++linkUpEvents_;
            Serial.println("Ethernet link connected");
            break;

        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.println("DHCP lease acquired");
            printConfiguration();
            dhcpReady_.store(true);
            break;

        case ARDUINO_EVENT_ETH_DISCONNECTED:
            dhcpReady_.store(false);
            linkUp_.store(false);
            driverHandle_.store(nullptr);
            ++linkDownEvents_;
            Serial.println("Ethernet link disconnected");
            break;

        case ARDUINO_EVENT_ETH_STOP:
            dhcpReady_.store(false);
            linkUp_.store(false);
            driverHandle_.store(nullptr);
            Serial.println("Ethernet stopped");
            break;

        default:
            break;
    }
}
