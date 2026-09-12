#pragma once

#include <cstdint>
#include "VaydeNet/packet/PacketValidation.h"

struct EngineStartupContext;
struct HardwareIdentity;
struct NodeSettings;
class TransportInterface;

enum class EngineStartStatus : std::uint8_t {
    Ok,
    StartupFailed
};

enum class EngineReceiveStatus : std::uint8_t {
    PacketAccepted,
    PacketRejected,
    QueueEmpty,
    NotStarted,
    TransportNotReady
};

struct EngineReceiveResult {
    EngineReceiveStatus status;
    PacketValidationStatus validation;
    Packet packet;
};

class VaydeEngine {
public:
    EngineStartStatus start(
        const EngineStartupContext& context
    );
    EngineReceiveResult consumeNextPacket();

private:
    const HardwareIdentity* identity_{nullptr};
    const NodeSettings* settings_{nullptr};
    TransportInterface* transport_{nullptr};
    bool started_{false};
};
