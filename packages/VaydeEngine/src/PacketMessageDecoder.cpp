#include "VaydeNet/message/PacketMessageDecoder.h"

#include <algorithm>
#include <cstdint>

namespace {

// The current compatible sender uses prototype packet type 1.
// Additional types require explicit decoding rules.
constexpr std::uint8_t kSupportedPacketType = 1;

}  // namespace

PacketMessageDecodeStatus decodeValidatedPacket(
    const Packet& packet,
    Message& message
) {
    message = Message{};

    if (packet.length > kMaximumMessagePayloadSize) {
        return PacketMessageDecodeStatus::InvalidLength;
    }

    if (packet.type != kSupportedPacketType) {
        return PacketMessageDecodeStatus::UnsupportedType;
    }

    message.protocol_version = packet.version;
    message.type = packet.type;
    message.flags = packet.flags;
    message.ttl = packet.ttl;
    message.source_node_id = packet.senderID;
    message.sequence_number = packet.sequenceNumber;
    message.payload_length = packet.length;

    std::copy_n(
        packet.payload,
        packet.length,
        message.payload.begin()
    );

    return PacketMessageDecodeStatus::Decoded;
}
