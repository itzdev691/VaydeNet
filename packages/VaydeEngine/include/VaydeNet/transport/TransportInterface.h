#pragma once

#include <cstdint>

#include "VaydeNet/packet/Packet.h"

enum class TransportStatus : std::uint8_t {
    Ok,
    InitializationFailed
};

enum class TransportReceiveStatus : std::uint8_t {
    Received,
    Empty,
    NotInitialized
};

enum class TransportTransmitStatus : std::uint8_t {
    Queued,
    Busy,
    NotInitialized,
    Unavailable,
    Failed
};

enum class TransportTransmitCompletionStatus : std::uint8_t {
    Sent,
    Failed,
    Pending,
    Empty,
    NotInitialized,
    Unavailable
};

class TransportInterface {
public:
    virtual ~TransportInterface() = default;

    virtual TransportStatus initialize() = 0;
    virtual TransportReceiveStatus tryReceive(Packet& packet) = 0;
    virtual TransportTransmitStatus tryTransmit(const Packet& packet) = 0;
    virtual TransportTransmitCompletionStatus pollTransmitCompletion() = 0;
};
