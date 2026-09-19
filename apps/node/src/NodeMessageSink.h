#pragma once

#include "VaydeNet/message/MessageSink.h"

class NodeMessageSink final : public MessageSink {
public:
    MessageSinkStatus deliver(
        const Message& message
    ) override;
};
