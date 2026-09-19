#pragma once

#include <cstdint>

#include "VaydeNet/message/Message.h"
#include "VaydeNet/packet/Packet.h"

enum class PacketMessageDecodeStatus : std::uint8_t {
    Decoded,
    UnsupportedType,
    InvalidLength
};

// The caller must validate the packet before decoding it.
PacketMessageDecodeStatus decodeValidatedPacket(
    const Packet& packet,
    Message& message
);
