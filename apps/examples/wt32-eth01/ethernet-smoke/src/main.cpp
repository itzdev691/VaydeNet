#include <Arduino.h>
#include "AppConfig.h"
#include "EthernetNetwork.h"
#include "VaydeBroadcaster.h"
#include "WebDashboard.h"

namespace {

EthernetNetwork network;
VaydeBroadcaster broadcaster(network);
WebDashboard dashboard(network, broadcaster);
uint32_t lastStatisticsMs = 0;

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);

    broadcaster.begin();
    if (!network.begin()) {
        Serial.println("Ethernet initialization failed");
    }
    dashboard.begin();
}

void loop() {
    const uint32_t now = millis();

    dashboard.update();
    broadcaster.update(now);

    if (now - lastStatisticsMs >= AppConfig::kStatisticsIntervalMs) {
        lastStatisticsMs = now;
        broadcaster.printStatistics();
    }

    delay(10);
}
