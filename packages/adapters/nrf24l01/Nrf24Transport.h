#pragma once

#include "driver/spi_master.h"
#include "VaydeNet/transport/TransportInterface.h"

// SPI setup is implemented; radio behavior is not implemented.
class Nrf24Transport final : public TransportInterface {
public:
    Nrf24Transport() = default;
    ~Nrf24Transport() override;

    Nrf24Transport(const Nrf24Transport&) = delete;
    Nrf24Transport& operator=(const Nrf24Transport&) = delete;

    TransportStatus initialize() override;
    TransportReceiveStatus tryReceive(Packet& packet) override;
    TransportTransmitStatus tryTransmit(const Packet& packet) override;
    TransportTransmitCompletionStatus pollTransmitCompletion() override;

private:
    spi_device_handle_t spi_device_{nullptr};
};
