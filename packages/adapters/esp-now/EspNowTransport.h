#pragma once

#include <cstdint>

#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "VaydeNet/packet/Packet.h"
#include "VaydeNet/transport/TransportInterface.h"

enum class TransportReceiveStatus : std::uint8_t {
    Received,
    Empty,
    NotInitialized
};

class EspNowTransport final : public TransportInterface {
public:
    bool configureChannel(std::uint16_t channel);
    TransportStatus initialize() override;
    TransportReceiveStatus tryReceive(Packet& packet);

private:
    static void onDataReceived(
        const esp_now_recv_info_t* receive_info,
        const std::uint8_t* data,
        int data_length
    );

    void enqueueReceivedData(
        const std::uint8_t* data,
        int data_length
    );

    static EspNowTransport* active_instance_;

    QueueHandle_t receive_queue_{nullptr};
    std::uint8_t channel_{};
    bool initialized_{false};
};
