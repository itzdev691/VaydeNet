#pragma once

#include <cstdint>

struct EngineStartupContext;
struct HardwareIdentity;
struct NodeSettings;
class TransportInterface;

enum class EngineStartStatus : std::uint8_t {
    Ok,
    StartupFailed
};

enum class EngineReceiveStatus : std::uint8_t {
    PacketDequeued,
    QueueEmpty,
    NotStarted,
    TransportNotReady
};

class VaydeEngine {
public:
    EngineStartStatus start(
        const EngineStartupContext& context
    );
    EngineReceiveStatus consumeNextPacket();

private:
    const HardwareIdentity* identity_{nullptr};
    const NodeSettings* settings_{nullptr};
    TransportInterface* transport_{nullptr};
    bool started_{false};
};
