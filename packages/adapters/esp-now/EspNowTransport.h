#pragma once

#include <cstdint>

#include "VaydeNet/transport/TransportInterface.h"

class EspNowTransport final : public TransportInterface {
public:
    bool configureChannel(std::uint16_t channel);
    TransportStatus initialize() override;

private:
    std::uint8_t channel_{};
    bool initialized_{false};
};
