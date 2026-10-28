#include "Nrf24Frame.h"

#include <cstring>

namespace {
std::size_t sliceLength(std::uint8_t index) {
    const std::size_t offset = index * kNrf24FrameDataSize;
    const std::size_t remaining = sizeof(Packet) - offset;
    return remaining < kNrf24FrameDataSize ? remaining : kNrf24FrameDataSize;
}
}

void encodeNrf24Frame(
    const Packet& packet,
    std::uint8_t transfer_id,
    std::uint8_t index,
    Nrf24Frame& frame
) {
    frame = Nrf24Frame{};
    frame.format = kNrf24FrameFormat;
    frame.transfer_id = transfer_id;
    frame.index = index;
    frame.count = kNrf24FramesPerPacket;
    if (index >= kNrf24FramesPerPacket) {
        return;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&packet);
    std::memcpy(frame.data, bytes + index * kNrf24FrameDataSize, sliceLength(index));
}

Nrf24ReassemblyStatus Nrf24Reassembler::accept(const Nrf24Frame& frame, Packet& packet) {
    if (frame.format != kNrf24FrameFormat || frame.count != kNrf24FramesPerPacket ||
        frame.index >= kNrf24FramesPerPacket) {
        return Nrf24ReassemblyStatus::Rejected;
    }

    if (frame.index == 0) {
        active_ = true;
        transfer_id_ = frame.transfer_id;
        next_index_ = 0;
    } else if (!active_ || frame.transfer_id != transfer_id_ ||
               frame.index != next_index_) {
        reset();
        return Nrf24ReassemblyStatus::Rejected;
    }

    std::memcpy(buffer_ + frame.index * kNrf24FrameDataSize, frame.data,
                sliceLength(frame.index));
    if (++next_index_ < kNrf24FramesPerPacket) {
        return Nrf24ReassemblyStatus::Incomplete;
    }

    std::memcpy(&packet, buffer_, sizeof(Packet));
    reset();
    return Nrf24ReassemblyStatus::Complete;
}

void Nrf24Reassembler::reset() {
    std::memset(buffer_, 0, sizeof(buffer_));
    transfer_id_ = 0;
    next_index_ = 0;
    active_ = false;
}
