#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

Packet makeValidPacket() {
    Packet packet{};
    packet.version = 1;
    packet.type = 1;
    packet.ttl = 1;
    packet.length = 0;
    packet.crc = computePacketCrc(packet);
    return packet;
}

void refreshCrc(Packet& packet) {
    packet.crc = computePacketCrc(packet);
}

}  // namespace

int main() {
    Packet packet{};
    packet.version = 1;
    packet.type = 1;
    packet.ttl = 1;

    expect(
        computePacketCrc(packet) == 0x09A1U,
        "CRC-16/CCITT-FALSE result changed"
    );

    packet = makeValidPacket();
    expect(
        validatePacket(packet) == PacketValidationStatus::Valid,
        "valid zero-length packet was rejected"
    );

    packet = makeValidPacket();
    packet.length = sizeof(packet.payload);

    for (
        std::size_t index = 0;
        index < sizeof(packet.payload);
        ++index
    ) {
        packet.payload[index] = static_cast<std::uint8_t>(index);
    }

    refreshCrc(packet);

    expect(
        validatePacket(packet) == PacketValidationStatus::Valid,
        "valid maximum-length packet was rejected"
    );

    packet = makeValidPacket();
    packet.version = 2;
    refreshCrc(packet);

    expect(
        validatePacket(packet) ==
            PacketValidationStatus::UnsupportedVersion,
        "unsupported version was accepted"
    );

    packet = makeValidPacket();
    packet.type = 0;
    refreshCrc(packet);

    expect(
        validatePacket(packet) == PacketValidationStatus::InvalidType,
        "zero packet type was accepted"
    );

    packet = makeValidPacket();
    packet.type = 0xFFU;
    refreshCrc(packet);

    expect(
        validatePacket(packet) == PacketValidationStatus::Valid,
        "nonzero flexible packet type was rejected"
    );

    packet = makeValidPacket();
    packet.ttl = 0;
    refreshCrc(packet);

    expect(
        validatePacket(packet) == PacketValidationStatus::InvalidTtl,
        "zero TTL was accepted"
    );

    packet = makeValidPacket();
    packet.length = sizeof(packet.payload) + 1U;
    refreshCrc(packet);

    expect(
        validatePacket(packet) == PacketValidationStatus::InvalidLength,
        "oversized payload length was accepted"
    );

    packet = makeValidPacket();
    packet.length = 1;
    packet.payload[0] = 0x42U;
    refreshCrc(packet);

    packet.payload[0] ^= 0xFFU;

    expect(
        validatePacket(packet) == PacketValidationStatus::CrcMismatch,
        "payload corruption was accepted"
    );

    std::puts("PASS: packet validation");
    return EXIT_SUCCESS;
}
