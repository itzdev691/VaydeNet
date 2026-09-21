#include "VaydeNet/message/PacketMessageEncoder.h"

#include <algorithm>
#include <cstdint>

#include "VaydeNet/packet/PacketValidation.h"

namespace {

constexpr std::uint8_t kPacketVersion = 1;
constexpr std::uint8_t kSupportedMessageType = 1;

}  // namespace

PacketMessageEncodeStatus encodeOutboundMessage(
    const OutboundMessage& message,
    std::uint64_t sender_id,
    std::uint32_t sequence_number,
    Packet& packet
) {
    packet = Packet{};

    if (message.type != kSupportedMessageType) {
        return PacketMessageEncodeStatus::InvalidType;
    }

    if (message.ttl == 0) {
        return PacketMessageEncodeStatus::InvalidTtl;
    }

    if (
        message.payload_length > message.payload.size() ||
        message.payload_length > sizeof(packet.payload)
    ) {
        return PacketMessageEncodeStatus::InvalidLength;
    }

    packet.version = kPacketVersion;
    packet.type = message.type;
    packet.flags = message.flags;
    packet.ttl = message.ttl;
    packet.length = message.payload_length;
    packet.senderID = sender_id;
    packet.sequenceNumber = sequence_number;

    std::copy_n(
        message.payload.begin(),
        message.payload_length,
        packet.payload
    );

    packet.crc = computePacketCrc(packet);
    return PacketMessageEncodeStatus::Encoded;
}
