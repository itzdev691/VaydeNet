#include "bootstrap/NodeBootstrap.h"
#include "NodeSettingsLoader.h"

void NodeBootstrap::run() {
    // Startup dependencies will be assembled here.
    const BoardInfoStatus status =
    retrieveBoardInformation(board_information_);

    if (status != BoardInfoStatus::Ok) {
        return;
    }

    const NodeSettingsLoadStatus settings_status =
    loadNodeSettings(board_information_, node_settings_);

    if (settings_status != NodeSettingsLoadStatus::Ok) {
        return;
    }

    switch (node_settings_.transport) {
        case TransportType::EspNow:
            selected_transport_ = &esp_now_transport_;
            break;

        case TransportType::Nrf24:
        case TransportType::Ethernet:
        case TransportType::Unspecified:
            return;
    }

    const TransportStatus transport_status =
        selected_transport_->initialize();

    if (transport_status != TransportStatus::Ok) {
        return;
    }

    // ESP-NOW should be initialized
    // VaydeEngine handoff will be here hopefully

}
