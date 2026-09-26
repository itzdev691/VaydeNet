#pragma once

#include <cstdint>

enum class EngineTransmitStatus : std::uint8_t {
    Queued,
    Busy,
    InvalidMessageType,
    InvalidMessageTtl,
    InvalidMessageLength,
    NotStarted,
    TransportNotReady,
    Unavailable,
    Failed
};

enum class EngineTransmitCompletionStatus : std::uint8_t {
    Sent,
    Failed,
    Pending,
    Empty,
    NotStarted,
    TransportNotReady,
    Unavailable
};
