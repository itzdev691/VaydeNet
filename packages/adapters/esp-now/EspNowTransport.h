#pragma once

#include "VaydeNet/transport/TransportInterface.h"

class EspNowTransport final : public TransportInterface {
public:
    TransportStatus initialize() override;

private:
    bool initialized_{false};
};
