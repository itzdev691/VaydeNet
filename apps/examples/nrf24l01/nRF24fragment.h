#pragma once
#include <cstdint>

struct __attribute__((packed)) Nrf24Fragment {
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t ttl;
    uint16_t length;
    uint64_t senderID;
    uint32_t sequenceNumber;

    uint8_t fragmentIndex;
    uint8_t fragmentCount;
    uint8_t data[10];

    uint16_t crc;
};

static_assert(sizeof(Nrf24Fragment) == 32);
