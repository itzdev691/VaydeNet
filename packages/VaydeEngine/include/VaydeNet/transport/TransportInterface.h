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

class TransportInterface {
public:
    virtual ~TransportInterface() = default;

    virtual TransportStatus initialize() = 0;
    virtual TransportReceiveStatus tryReceive(Packet& packet) = 0;
};
