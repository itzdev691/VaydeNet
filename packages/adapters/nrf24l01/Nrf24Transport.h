#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/spi_master.h"
#include "VaydeNet/transport/TransportInterface.h"

// Startup register/FIFO checks are implemented; packet RX/TX is not implemented.
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
    // Read 1-5 register bytes in SPI transfer order, excluding the STATUS byte.
    esp_err_t readRegister(uint8_t reg, uint8_t* data, std::size_t length);

    esp_err_t writeRegister(uint8_t reg, const uint8_t* data, std::size_t length);
    bool writeAndVerify(uint8_t reg, const uint8_t* data, std::size_t length);
    esp_err_t sendCommand(uint8_t command);

    spi_device_handle_t spi_device_{nullptr};
};
