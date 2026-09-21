#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Transport-independent content for a locally originated message.
// Destination and transport options belong to the transmit request.
inline constexpr std::size_t kMaximumOutboundPayloadSize = 200;

struct OutboundMessage {
    std::uint8_t type{};
    std::uint8_t flags{};
    std::uint8_t ttl{1};
    std::uint16_t payload_length{};
    std::array<std::uint8_t, kMaximumOutboundPayloadSize> payload{};
};
