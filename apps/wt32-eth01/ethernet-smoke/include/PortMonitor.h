#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <IPAddress.h>

#include "AppConfig.h"

class EthernetNetwork;

class PortMonitor final {
public:
    struct Result {
        uint16_t port = 0;
        bool tested = false;
        bool open = false;
        uint32_t latencyMs = 0;
        uint32_t lastCheckedMs = 0;
    };

    struct Snapshot {
        bool targetAvailable = false;
        IPAddress target{};
        std::array<Result, AppConfig::kProbePortCount> results{};
        uint32_t completedSweeps = 0;
    };

    explicit PortMonitor(EthernetNetwork &network);

    void update(uint32_t nowMs);
    Snapshot snapshot() const;
    void printStatus() const;

private:
    void reset(const IPAddress &target, bool targetAvailable);

    EthernetNetwork &network_;
    bool targetAvailable_ = false;
    IPAddress target_{};
    std::array<Result, AppConfig::kProbePortCount> results_{};
    size_t nextProbeIndex_ = 0;
    uint32_t completedSweeps_ = 0;
    uint32_t lastProbeMs_ = 0;
};
