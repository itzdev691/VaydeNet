#pragma once

#include <cstdint>
#include "VaydeNet/packet/Packet.h"

enum class PacketValidationStatus : std::uint8_t {
    NotChecked,
    Valid,
    UnsupportedVersion,
    InvalidType,
    InvalidTtl,
    InvalidLength,
    CrcMismatch
};

std::uint16_t computePacketCrc(const Packet& packet);

PacketValidationStatus validatePacket(const Packet& packet);
