#pragma once

#include <cstdint>

#include "VaydeNet/message/OutboundMessage.h"
#include "VaydeNet/packet/Packet.h"

enum class PacketMessageEncodeStatus : std::uint8_t {
    Encoded,
    InvalidType,
    InvalidTtl,
    InvalidLength
};

// Encodes a locally originated type-1 message into the current VaydeNet V1
// prototype packet. The caller supplies engine-owned identity and sequence
// values. The output packet is cleared even when validation fails.
PacketMessageEncodeStatus encodeOutboundMessage(
    const OutboundMessage& message,
    std::uint64_t sender_id,
    std::uint32_t sequence_number,
    Packet& packet
);
