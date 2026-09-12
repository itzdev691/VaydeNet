#include "VaydeNet/packet/PacketValidation.h"

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::uint8_t kSupportedPacketVersion = 1;
constexpr std::uint16_t kCrcInitialValue = 0xFFFFU;
constexpr std::uint16_t kCrcPolynomial = 0x1021U;

}  // namespace

std::uint16_t computePacketCrc(const Packet& packet) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&packet);
    std::uint16_t crc = kCrcInitialValue;

    // The CRC covers the complete packed packet preceding Packet::crc.
    for (std::size_t index = 0; index < offsetof(Packet, crc); ++index) {
        crc ^= static_cast<std::uint16_t>(bytes[index]) << 8U;

        for (std::uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0) {
                crc = static_cast<std::uint16_t>(
                    (crc << 1U) ^ kCrcPolynomial
                );
            } else {
                crc = static_cast<std::uint16_t>(crc << 1U);
            }
        }
    }

    return crc;
}

PacketValidationStatus validatePacket(const Packet& packet) {
    if (packet.version != kSupportedPacketVersion) {
        return PacketValidationStatus::UnsupportedVersion;
    }

    if (packet.length > sizeof(packet.payload)) {
        return PacketValidationStatus::InvalidLength;
    }

    if (packet.type == 0) {
        return PacketValidationStatus::InvalidType;
    }

    if (packet.ttl == 0) {
        return PacketValidationStatus::InvalidTtl;
    }

    if (computePacketCrc(packet) != packet.crc) {
        return PacketValidationStatus::CrcMismatch;
    }

    return PacketValidationStatus::Valid;
}
