#include "WebDashboard.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <cstdio>

#include <esp_err.h>

#include "AppConfig.h"
#include "EthernetNetwork.h"
#include "PortMonitor.h"
#include "VaydeBroadcaster.h"

namespace {

String formatMacAddress(const uint8_t mac[6]) {
    char formatted[18]{};
    std::snprintf(
        formatted,
        sizeof(formatted),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
    return String(formatted);
}

void appendUnsigned(String &json, uint64_t value) {
    char formatted[24]{};
    std::snprintf(
        formatted,
        sizeof(formatted),
        "%llu",
        static_cast<unsigned long long>(value));
    json += formatted;
}

void appendBoolean(String &json, bool value) {
    json += value ? F("true") : F("false");
}

}  // namespace

WebDashboard::WebDashboard(
    EthernetNetwork &network,
    const PortMonitor &portMonitor,
    const VaydeBroadcaster &broadcaster)
    : network_(network),
      portMonitor_(portMonitor),
      broadcaster_(broadcaster),
      server_(AppConfig::kDashboardPort) {
}

bool WebDashboard::begin() {
    filesystemReady_ = LittleFS.begin(false);
    configureRoutes();
    server_.begin();

    Serial.printf(
        "HTTP dashboard listening on TCP port %u\n",
        AppConfig::kDashboardPort);

    if (!filesystemReady_) {
        Serial.println(
            "LittleFS unavailable; upload the data directory with the uploadfs target");
    }

    return filesystemReady_;
}

void WebDashboard::update() {
    server_.handleClient();

    const uint32_t now = millis();
    if (now - lastAnnouncementCheckMs_ < 500) {
        return;
    }
    lastAnnouncementCheckMs_ = now;

    const EthernetNetwork::Snapshot network = network_.snapshot();
    const uint32_t currentIp = static_cast<uint32_t>(network.ipv4);

    if (!network.dhcpReady) {
        announcedIp_ = 0;
        return;
    }

    if (currentIp != announcedIp_) {
        announcedIp_ = currentIp;
        Serial.printf(
            "Dashboard: http://%s:%u/\n",
            network.ipv4.toString().c_str(),
            AppConfig::kDashboardPort);
    }
}

void WebDashboard::configureRoutes() {
    server_.on("/", HTTP_GET, [this]() {
        serveAsset("/index.html", "text/html; charset=utf-8");
    });
    server_.on("/index.html", HTTP_GET, [this]() {
        serveAsset("/index.html", "text/html; charset=utf-8");
    });
    server_.on("/styles.css", HTTP_GET, [this]() {
        serveAsset("/styles.css", "text/css; charset=utf-8");
    });
    server_.on("/app.js", HTTP_GET, [this]() {
        serveAsset("/app.js", "application/javascript; charset=utf-8");
    });
    server_.on("/api/status", HTTP_GET, [this]() {
        sendStatus();
    });
    server_.on("/health", HTTP_GET, [this]() {
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(200, "text/plain; charset=utf-8", "ok\n");
    });
    server_.on("/favicon.ico", HTTP_GET, [this]() {
        server_.send(204);
    });
    server_.onNotFound([this]() {
        sendNotFound();
    });
}

void WebDashboard::serveAsset(const char *path, const char *contentType) {
    if (!filesystemReady_) {
        server_.send(
            503,
            "text/plain; charset=utf-8",
            "Dashboard assets are unavailable. Upload the LittleFS image.\n");
        return;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        server_.send(404, "text/plain; charset=utf-8", "Asset not found.\n");
        return;
    }

    server_.sendHeader("Cache-Control", "no-cache");
    server_.streamFile(file, contentType);
    file.close();
}

void WebDashboard::sendStatus() {
    const EthernetNetwork::Snapshot network = network_.snapshot();
    const PortMonitor::Snapshot ports = portMonitor_.snapshot();
    const VaydeBroadcaster::Snapshot espNow = broadcaster_.snapshot();

    String json;
    json.reserve(1400);

    json += F("{\"device\":{");
    json += F("\"hostname\":\"");
    json += AppConfig::kHostname;
    json += F("\",\"dashboardPort\":");
    appendUnsigned(json, AppConfig::kDashboardPort);
    json += F(",\"uptimeMs\":");
    appendUnsigned(json, millis());

    json += F("},\"network\":{");
    json += F("\"ready\":");
    appendBoolean(json, network.linkUp && network.dhcpReady);
    json += F(",\"linkUp\":");
    appendBoolean(json, network.linkUp);
    json += F(",\"dhcpReady\":");
    appendBoolean(json, network.dhcpReady);
    json += F(",\"mac\":\"");
    json += formatMacAddress(network.mac);
    json += F("\",\"ipv4\":\"");
    json += network.ipv4.toString();
    json += F("\",\"subnet\":\"");
    json += network.subnet.toString();
    json += F("\",\"gateway\":\"");
    json += network.gateway.toString();
    json += F("\",\"dns\":\"");
    json += network.dns.toString();
    json += F("\",\"speedMbps\":");
    appendUnsigned(json, network.speedMbps);
    json += F(",\"duplex\":\"");
    json += network.linkUp
        ? (network.fullDuplex ? F("full") : F("half"))
        : F("unknown");
    json += F("\",\"linkUpEvents\":");
    appendUnsigned(json, network.linkUpEvents);
    json += F(",\"linkDownEvents\":");
    appendUnsigned(json, network.linkDownEvents);

    json += F("},\"portProbes\":{");
    json += F("\"targetAvailable\":");
    appendBoolean(json, ports.targetAvailable);
    json += F(",\"target\":\"");
    json += ports.targetAvailable ? ports.target.toString() : String("--");
    json += F("\",\"probeIntervalMs\":");
    appendUnsigned(json, AppConfig::kPortProbeIntervalMs);
    json += F(",\"completedSweeps\":");
    appendUnsigned(json, ports.completedSweeps);
    json += F(",\"ports\":[");
    for (size_t index = 0; index < ports.results.size(); ++index) {
        const PortMonitor::Result &result = ports.results[index];
        if (index != 0) {
            json += ',';
        }
        json += F("{\"port\":");
        appendUnsigned(json, result.port);
        json += F(",\"tested\":");
        appendBoolean(json, result.tested);
        json += F(",\"open\":");
        appendBoolean(json, result.open);
        json += F(",\"latencyMs\":");
        appendUnsigned(json, result.latencyMs);
        json += F(",\"lastCheckedMs\":");
        appendUnsigned(json, result.lastCheckedMs);
        json += '}';
    }
    json += ']';

    json += F("},\"espNow\":{");
    json += F("\"ready\":");
    appendBoolean(json, espNow.ready);
    json += F(",\"destination\":\"FF:FF:FF:FF:FF:FF\",");
    json += F("\"stationMac\":\"");
    json += formatMacAddress(espNow.stationMac);
    json += F("\",\"channel\":");
    appendUnsigned(json, espNow.channel);
    json += F(",\"packetBytes\":220,\"intervalMs\":");
    appendUnsigned(json, AppConfig::kTransmitIntervalMs);
    json += F(",\"sequenceNumber\":");
    appendUnsigned(json, espNow.sequenceNumber);
    json += F(",\"attempts\":");
    appendUnsigned(json, espNow.attempts);
    json += F(",\"queueAccepted\":");
    appendUnsigned(json, espNow.queueAccepted);
    json += F(",\"queueRejected\":");
    appendUnsigned(json, espNow.queueRejected);
    json += F(",\"deliverySucceeded\":");
    appendUnsigned(json, espNow.deliverySucceeded);
    json += F(",\"deliveryFailed\":");
    appendUnsigned(json, espNow.deliveryFailed);
    json += F(",\"queuedBytes\":");
    appendUnsigned(json, espNow.packetBytesQueued);
    json += F(",\"lastQueueResult\":\"");
    json += esp_err_to_name(espNow.lastQueueResult);
    json += F("\",\"lastQueueResultCode\":");
    json += static_cast<int>(espNow.lastQueueResult);
    json += F(",\"lastDelivery\":\"");
    json += espNow.deliveryStatusAvailable
        ? (espNow.lastDeliveryStatus == ESP_NOW_SEND_SUCCESS
            ? F("success")
            : F("failed"))
        : F("pending");
    json += F("\"}}");

    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json; charset=utf-8", json);
}

void WebDashboard::sendNotFound() {
    server_.send(
        404,
        "application/json; charset=utf-8",
        "{\"error\":\"not_found\"}");
}
