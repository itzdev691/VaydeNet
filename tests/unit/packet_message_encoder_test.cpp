#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "VaydeNet/message/PacketMessageEncoder.h"
#include "VaydeNet/packet/PacketValidation.h"

namespace {

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, const char* message) {
    if (!condition) {
        fail(message);
    }
}

bool isCleared(const Packet& packet) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&packet);

    for (std::size_t index = 0; index < sizeof(packet); ++index) {
        if (bytes[index] != 0) {
            return false;
        }
    }

    return true;
}

Packet makeDirtyPacket() {
    Packet packet{};
    std::memset(&packet, 0xA5, sizeof(packet));
    return packet;
}

OutboundMessage makeValidMessage() {
    OutboundMessage message{};
    message.type = 1;
    message.flags = 3;
    message.ttl = 2;
    message.payload_length = 3;
    message.payload[0] = 0x10U;
    message.payload[1] = 0x20U;
    message.payload[2] = 0x30U;
    return message;
}

}  // namespace

int main() {
    constexpr std::uint64_t kSenderId = 0x1122334455667788ULL;
    constexpr std::uint32_t kSequenceNumber = 73;

    OutboundMessage message = makeValidMessage();
    Packet packet = makeDirtyPacket();

    expect(
        encodeOutboundMessage(
            message,
            kSenderId,
            kSequenceNumber,
            packet
        ) == PacketMessageEncodeStatus::Encoded,
        "valid message was not encoded"
    );
    expect(
        packet.version == 1 &&
            packet.type == message.type &&
            packet.flags == message.flags &&
            packet.ttl == message.ttl,
        "encoded metadata is incorrect"
    );
    expect(
        packet.length == message.payload_length &&
            packet.senderID == kSenderId &&
            packet.sequenceNumber == kSequenceNumber,
        "encoded identity or length is incorrect"
    );
    expect(
        packet.payload[0] == 0x10U &&
            packet.payload[1] == 0x20U &&
            packet.payload[2] == 0x30U &&
            packet.payload[3] == 0,
        "encoded payload was not copied into a cleared packet"
    );
    expect(
        packet.crc == computePacketCrc(packet),
        "encoded packet CRC is incorrect"
    );
    expect(
        validatePacket(packet) == PacketValidationStatus::Valid,
        "encoded packet failed packet validation"
    );

    message = makeValidMessage();
    message.type = 0;
    packet = makeDirtyPacket();
    expect(
        encodeOutboundMessage(message, 1, 1, packet) ==
            PacketMessageEncodeStatus::InvalidType,
        "zero message type was accepted"
    );
    expect(isCleared(packet), "invalid type did not clear the output packet");

    message = makeValidMessage();
    message.type = 2;
    packet = makeDirtyPacket();
    expect(
        encodeOutboundMessage(message, 1, 1, packet) ==
            PacketMessageEncodeStatus::InvalidType,
        "unsupported message type was accepted"
    );
    expect(
        isCleared(packet),
        "unsupported type did not clear the output packet"
    );

    message = makeValidMessage();
    message.ttl = 0;
    packet = makeDirtyPacket();
    expect(
        encodeOutboundMessage(message, 1, 1, packet) ==
            PacketMessageEncodeStatus::InvalidTtl,
        "zero TTL was accepted"
    );
    expect(isCleared(packet), "invalid TTL did not clear the output packet");

    message = makeValidMessage();
    message.payload_length = kMaximumOutboundPayloadSize + 1U;
    packet = makeDirtyPacket();
    expect(
        encodeOutboundMessage(message, 1, 1, packet) ==
            PacketMessageEncodeStatus::InvalidLength,
        "oversized message was accepted"
    );
    expect(
        isCleared(packet),
        "invalid length did not clear the output packet"
    );

    message = makeValidMessage();
    message.payload_length = kMaximumOutboundPayloadSize;
    for (std::size_t index = 0; index < message.payload.size(); ++index) {
        message.payload[index] = static_cast<std::uint8_t>(index);
    }

    expect(
        encodeOutboundMessage(message, 1, 1, packet) ==
            PacketMessageEncodeStatus::Encoded,
        "maximum-length message was rejected"
    );
    expect(
        packet.length == sizeof(packet.payload) &&
            packet.payload[0] == 0 &&
            packet.payload[199] == 199,
        "maximum-length payload was encoded incorrectly"
    );
    expect(
        validatePacket(packet) == PacketValidationStatus::Valid,
        "maximum-length encoded packet failed validation"
    );

    std::puts("PASS: outbound message encoding");
    return EXIT_SUCCESS;
}
