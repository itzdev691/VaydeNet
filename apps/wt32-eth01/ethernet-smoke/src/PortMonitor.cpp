#include "PortMonitor.h"

#include <Arduino.h>
#include <WiFiClient.h>

#include "EthernetNetwork.h"

PortMonitor::PortMonitor(EthernetNetwork &network)
    : network_(network) {
    reset(IPAddress(), false);
}

void PortMonitor::update(uint32_t nowMs) {
    const EthernetNetwork::Snapshot network = network_.snapshot();
    const bool targetAvailable = network.linkUp && network.dhcpReady;

    if (!targetAvailable) {
        if (targetAvailable_) {
            reset(IPAddress(), false);
        }
        return;
    }

    if (!targetAvailable_ || target_ != network.gateway) {
        reset(network.gateway, true);
    }

    if (nowMs - lastProbeMs_ < AppConfig::kPortProbeIntervalMs) {
        return;
    }
    lastProbeMs_ = nowMs;

    Result &result = results_[nextProbeIndex_];
    WiFiClient client;
    const uint32_t startedMs = millis();
    result.open = client.connect(
        target_,
        result.port,
        AppConfig::kPortProbeConnectTimeoutMs) == 1;
    result.latencyMs = millis() - startedMs;
    result.lastCheckedMs = millis();
    result.tested = true;
    client.stop();

    Serial.printf(
        "TCP probe %s:%u %s (%lu ms)\n",
        target_.toString().c_str(),
        result.port,
        result.open ? "open" : "closed/unreachable",
        static_cast<unsigned long>(result.latencyMs));

    nextProbeIndex_ = (nextProbeIndex_ + 1) % results_.size();
    if (nextProbeIndex_ == 0) {
        ++completedSweeps_;
    }
}

PortMonitor::Snapshot PortMonitor::snapshot() const {
    Snapshot current;
    current.targetAvailable = targetAvailable_;
    current.target = target_;
    current.results = results_;
    current.completedSweeps = completedSweeps_;
    return current;
}

void PortMonitor::printStatus() const {
    if (!targetAvailable_) {
        Serial.println("TCP probes: waiting for DHCP gateway");
        return;
    }

    Serial.printf("TCP probes against %s:", target_.toString().c_str());
    for (const Result &result : results_) {
        Serial.printf(
            " %u=%s",
            result.port,
            !result.tested ? "pending" : (result.open ? "open" : "closed"));
    }
    Serial.println();
}

void PortMonitor::reset(const IPAddress &target, bool targetAvailable) {
    targetAvailable_ = targetAvailable;
    target_ = target;
    nextProbeIndex_ = 0;
    completedSweeps_ = 0;
    lastProbeMs_ = 0;

    for (size_t index = 0; index < results_.size(); ++index) {
        results_[index] = {};
        results_[index].port = AppConfig::kProbePorts[index];
    }
}
