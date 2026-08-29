#pragma once

#include <cstdint>

#include "Esp32BoardInfo.h"
#include "VaydeNet/config/NodeSettings.h"
#include "EspNowTransport.h"
#include "VaydeNet/transport/TransportInterface.h"

enum class NodeBootstrapStatus : std::uint8_t {
    Ready,
    BoardInformationFailed,
    NotConfigured,
    SettingsReadFailed,
    UnsupportedTransport,
    InvalidTransportConfiguration,
    TransportInitializationFailed
};

class NodeBootstrap {
public:
    NodeBootstrapStatus run();


private:
    BoardInformation board_information_{};
    NodeSettings node_settings_{};

    EspNowTransport esp_now_transport_{};
    TransportInterface* selected_transport_{nullptr};
};
