#pragma once

#include <stdint.h>

// VaydeNet's transport-independent packet wire format.
// Keep this packed layout synchronized across every transport adapter.
typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t ttl;
    uint16_t length;
    uint64_t senderID;
    uint32_t sequenceNumber;
    uint8_t payload[200];
    uint16_t crc;
} Packet;

static_assert(sizeof(Packet) == 220, "VaydeNet Packet wire format must remain 220 bytes");
