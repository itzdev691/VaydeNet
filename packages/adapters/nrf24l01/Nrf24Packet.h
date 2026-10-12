#pragma once

#include <cstdint>

struct __attribute__((packed)) Nrf24Packet {
    std::uint8_t version;
    std::uint8_t type;
    std::uint8_t flags;
    std::uint8_t ttl;
    std::uint16_t length;
    std::uint64_t senderID;
    std::uint32_t sequenceNumber;
    std::uint8_t payload[12];
    std::uint16_t crc;
};

static_assert(sizeof(Nrf24Packet) == 32,
 "Nrf24Packet size is not 32 bytes");