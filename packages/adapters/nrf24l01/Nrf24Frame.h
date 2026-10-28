#pragma once

#include <cstddef>
#include <cstdint>

#include "VaydeNet/packet/Packet.h"

// One static 32-byte nRF24 payload carrying a slice of a 220-byte Packet.
// A Packet is always sent as kNrf24FramesPerPacket frames in index order;
// the engine validates the reassembled Packet's CRC.
struct __attribute__((packed)) Nrf24Frame {
    std::uint8_t format;      // kNrf24FrameFormat.
    std::uint8_t transfer_id; // Sender-local identifier shared by one Packet's frames.
    std::uint8_t index;       // 0 to count - 1.
    std::uint8_t count;       // kNrf24FramesPerPacket.
    std::uint8_t data[28];
};

static_assert(sizeof(Nrf24Frame) == 32, "Nrf24Frame must fill one 32-byte payload");

constexpr std::uint8_t kNrf24FrameFormat = 0xA1;
constexpr std::size_t kNrf24FrameDataSize = sizeof(Nrf24Frame::data);
constexpr std::uint8_t kNrf24FramesPerPacket =
    (sizeof(Packet) + kNrf24FrameDataSize - 1) / kNrf24FrameDataSize;

static_assert(kNrf24FramesPerPacket == 8, "220-byte Packet must span eight frames");

// Writes frame `index` of `packet`; bytes past the Packet end are zero.
void encodeNrf24Frame(
    const Packet& packet,
    std::uint8_t transfer_id,
    std::uint8_t index,
    Nrf24Frame& frame
);

enum class Nrf24ReassemblyStatus : std::uint8_t {
    Incomplete,
    Complete,
    Rejected
};

// Single-slot, in-order reassembly. Index 0 always starts a new Packet; any
// gap, foreign transfer or malformed header discards the partial Packet.
class Nrf24Reassembler {
public:
    Nrf24ReassemblyStatus accept(const Nrf24Frame& frame, Packet& packet);
    void reset();

private:
    std::uint8_t buffer_[sizeof(Packet)]{};
    std::uint8_t transfer_id_{};
    std::uint8_t next_index_{};
    bool active_{false};
};
