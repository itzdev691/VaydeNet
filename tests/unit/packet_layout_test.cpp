#include <cstddef>

#include "VaydeNet/packet/Packet.h"

static_assert(sizeof(Packet) == 220);
static_assert(offsetof(Packet, version) == 0);
static_assert(offsetof(Packet, type) == 1);
static_assert(offsetof(Packet, flags) == 2);
static_assert(offsetof(Packet, ttl) == 3);
static_assert(offsetof(Packet, length) == 4);
static_assert(offsetof(Packet, senderID) == 6);
static_assert(offsetof(Packet, sequenceNumber) == 14);
static_assert(offsetof(Packet, payload) == 18);
static_assert(offsetof(Packet, crc) == 218);

int main() {
    return 0;
}
