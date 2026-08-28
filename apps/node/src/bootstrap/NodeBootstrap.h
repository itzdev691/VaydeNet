#pragma once

#include "Esp32BoardInfo.h"
#include "VaydeNet/config/NodeSettings.h"
#include "EspNowTransport.h"
#include "VaydeNet/transport/TransportInterface.h"

class NodeBootstrap {
public:
    void run();


private:
    BoardInformation board_information_{};
    NodeSettings node_settings_{};

    EspNowTransport esp_now_transport_{};
    TransportInterface* selected_transport_{nullptr};
};
