#include "Nrf24Frame.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
Packet patterned(std::uint8_t seed) {
    Packet packet{};
    auto* bytes = reinterpret_cast<std::uint8_t*>(&packet);
    for (std::size_t i = 0; i < sizeof(packet); ++i) {
        bytes[i] = static_cast<std::uint8_t>(seed + i * 7);
    }
    return packet;
}

std::array<Nrf24Frame, kNrf24FramesPerPacket> frames(const Packet& packet,
                                                     std::uint8_t id) {
    std::array<Nrf24Frame, kNrf24FramesPerPacket> result{};
    for (std::uint8_t i = 0; i < kNrf24FramesPerPacket; ++i) {
        encodeNrf24Frame(packet, id, i, result[i]);
    }
    return result;
}

bool same(const Packet& a, const Packet& b) {
    return std::memcmp(&a, &b, sizeof(Packet)) == 0;
}

// Feeds frames in order; returns true only if the final frame completes.
bool deliver(Nrf24Reassembler& reassembler,
             const std::array<Nrf24Frame, kNrf24FramesPerPacket>& sequence,
             Packet& out) {
    for (std::uint8_t i = 0; i + 1 < kNrf24FramesPerPacket; ++i) {
        if (reassembler.accept(sequence[i], out) != Nrf24ReassemblyStatus::Incomplete) {
            return false;
        }
    }
    return reassembler.accept(sequence.back(), out) == Nrf24ReassemblyStatus::Complete;
}
}

int main() {
    const Packet packet = patterned(3);
    const auto sequence = frames(packet, 42);

    // Headers, data slices and zero padding after the 220th byte.
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&packet);
    for (std::uint8_t i = 0; i < kNrf24FramesPerPacket; ++i) {
        const Nrf24Frame& frame = sequence[i];
        assert(frame.format == kNrf24FrameFormat && frame.transfer_id == 42);
        assert(frame.index == i && frame.count == kNrf24FramesPerPacket);
        const std::size_t offset = i * kNrf24FrameDataSize;
        const std::size_t length = i + 1 < kNrf24FramesPerPacket ? 28 : 220 - 196;
        assert(std::memcmp(frame.data, bytes + offset, length) == 0);
        for (std::size_t j = length; j < kNrf24FrameDataSize; ++j) assert(frame.data[j] == 0);
    }

    // Round trip, then a second Packet through the same reassembler.
    Nrf24Reassembler reassembler;
    Packet out{};
    assert(deliver(reassembler, sequence, out) && same(out, packet));
    const Packet second = patterned(99);
    assert(deliver(reassembler, frames(second, 43), out) && same(out, second));

    // Malformed headers are rejected without disturbing the partial Packet.
    Nrf24Frame bad = sequence[1];
    for (int field = 0; field < 3; ++field) {
        bad = sequence[1];
        if (field == 0) bad.format = 0x01; // A raw Packet version byte.
        if (field == 1) bad.count = 7;
        if (field == 2) bad.index = kNrf24FramesPerPacket;
        reassembler.reset();
        assert(reassembler.accept(sequence[0], out) == Nrf24ReassemblyStatus::Incomplete);
        assert(reassembler.accept(bad, out) == Nrf24ReassemblyStatus::Rejected);
        for (std::uint8_t i = 1; i + 1 < kNrf24FramesPerPacket; ++i) {
            assert(reassembler.accept(sequence[i], out) == Nrf24ReassemblyStatus::Incomplete);
        }
        assert(reassembler.accept(sequence.back(), out) == Nrf24ReassemblyStatus::Complete);
    }

    // Gaps, duplicates, foreign transfers and orphan frames discard the partial Packet.
    const auto other = frames(second, 7);
    for (int scenario = 0; scenario < 4; ++scenario) {
        reassembler.reset();
        out = Packet{};
        if (scenario == 3) {
            assert(reassembler.accept(sequence[2], out) == Nrf24ReassemblyStatus::Rejected);
            continue;
        }
        assert(reassembler.accept(sequence[0], out) == Nrf24ReassemblyStatus::Incomplete);
        assert(reassembler.accept(sequence[1], out) == Nrf24ReassemblyStatus::Incomplete);
        const Nrf24Frame& wrong = scenario == 0 ? sequence[3]
                                : scenario == 1 ? sequence[1]
                                                : other[2];
        assert(reassembler.accept(wrong, out) == Nrf24ReassemblyStatus::Rejected);
        for (std::uint8_t i = 2; i < kNrf24FramesPerPacket; ++i) {
            assert(reassembler.accept(sequence[i], out) == Nrf24ReassemblyStatus::Rejected);
        }
        assert(same(out, Packet{}));
    }

    // A new index 0 restarts reassembly, so an interrupted sender recovers.
    reassembler.reset();
    for (std::uint8_t i = 0; i < 5; ++i) {
        assert(reassembler.accept(other[i], out) == Nrf24ReassemblyStatus::Incomplete);
    }
    assert(deliver(reassembler, sequence, out) && same(out, packet));

    std::printf("PASS: nRF24 frame encoding and reassembly\n");
}
