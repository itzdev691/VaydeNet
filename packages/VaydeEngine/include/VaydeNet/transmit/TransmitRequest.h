#pragma once

#include <cstdint>

#include "VaydeNet/message/OutboundMessage.h"

enum class TransmitDestination : std::uint8_t {
    Broadcast
};

struct TransmitRequest {
    OutboundMessage message{};
    TransmitDestination destination{TransmitDestination::Broadcast};
};
