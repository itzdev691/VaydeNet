#pragma once

#include <cstdint>

#include "VaydeNet/packet/PacketValidation.h"
#include "VaydeNet/transmit/EngineTransmit.h"

struct EngineStartupContext;
struct HardwareIdentity;
struct NodeSettings;
struct TransmitRequest;

class MessageSink;
class TransportInterface;

enum class EngineStartStatus : std::uint8_t {
    Ok,
    StartupFailed
};

enum class EngineProcessStatus : std::uint8_t {
    MessageDelivered,
    PacketRejected,
    UnsupportedMessageType,
    InvalidMessageLength,
    MessageRejected,
    QueueEmpty,
    NotStarted,
    TransportNotReady,
    MessageSinkUnavailable
};

struct EngineProcessResult {
    EngineProcessStatus status;
    PacketValidationStatus validation;
};

class VaydeEngine {
public:
    EngineStartStatus start(
        const EngineStartupContext& context
    );

    EngineProcessResult processNextPacket();
    EngineTransmitStatus tryTransmit(
        const TransmitRequest& request
    );
    EngineTransmitCompletionStatus pollTransmitCompletion();

private:
    const HardwareIdentity* identity_{nullptr};
    const NodeSettings* settings_{nullptr};
    TransportInterface* transport_{nullptr};
    MessageSink* message_sink_{nullptr};
    std::uint32_t next_sequence_number_{0};
    bool started_{false};
};
