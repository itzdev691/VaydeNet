#include <Arduino.h>
#include "AppConfig.h"
#include "EthernetNetwork.h"
#include "PortMonitor.h"
#include "VaydeBroadcaster.h"
#include "WebDashboard.h"

namespace {

EthernetNetwork network;
PortMonitor portMonitor(network);
VaydeBroadcaster broadcaster(network, portMonitor);
WebDashboard dashboard(network, portMonitor, broadcaster);
uint32_t lastStatisticsMs = 0;

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!network.begin()) {
        Serial.println("Ethernet initialization failed");
    }
    if (!broadcaster.begin()) {
        Serial.println("ESP-NOW broadcaster initialization failed");
    }
    dashboard.begin();
}

void loop() {
    const uint32_t now = millis();

    dashboard.update();
    portMonitor.update(now);
    broadcaster.update(now);

    if (now - lastStatisticsMs >= AppConfig::kStatisticsIntervalMs) {
        lastStatisticsMs = now;
        broadcaster.printStatistics();
        portMonitor.printStatus();
    }

    delay(10);
}
