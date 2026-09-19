#pragma once

#include <cstdint>

#include "VaydeNet/message/Message.h"

enum class MessageSinkStatus : std::uint8_t {
    Delivered,
    Rejected
};

class MessageSink {
public:
    virtual ~MessageSink() = default;

    virtual MessageSinkStatus deliver(
        const Message& message
    ) = 0;
};
