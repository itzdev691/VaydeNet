#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Transport-independent in-memory message representation.
// This is not a packed or serialized wire format.
inline constexpr std::size_t kMaximumMessagePayloadSize = 200;

struct Message {
    std::uint8_t protocol_version{};
    std::uint8_t type{};
    std::uint8_t flags{};
    std::uint8_t ttl{};
    std::uint64_t source_node_id{};
    std::uint32_t sequence_number{};
    std::uint16_t payload_length{};
    std::array<
        std::uint8_t,
        kMaximumMessagePayloadSize
    > payload{};
};
