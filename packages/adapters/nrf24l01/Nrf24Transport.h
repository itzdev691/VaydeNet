#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "Nrf24Frame.h"
#include "VaydeNet/transport/TransportInterface.h"

// Carries each 220-byte Packet as eight auto-acknowledged 32-byte frames.
// The radio listens between transmissions; one Packet transmission may be pending.
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
    esp_err_t writePayload(const Nrf24Frame& frame);
    esp_err_t readPayload(Nrf24Frame& frame);

    bool startListening();
    bool loadTransmitFrames();
    TransportTransmitCompletionStatus finishTransmit(
        TransportTransmitCompletionStatus result
    );

    spi_device_handle_t spi_device_{nullptr};
    Nrf24Reassembler reassembler_{};
    Packet transmit_packet_{};
    TickType_t transmit_started_{};
    std::uint8_t transfer_id_{};
    std::uint8_t frames_loaded_{};
    bool transmit_pending_{false};
    bool initialized_{false};
};
