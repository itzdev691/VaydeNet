#include "bootstrap/NodeBootstrap.h"
#include "NodeSettingsLoader.h"

NodeBootstrapStatus NodeBootstrap::run() {
    // Startup dependencies will be assembled here.
    const BoardInfoStatus status =
        retrieveBoardInformation(board_information_);

    if (status != BoardInfoStatus::Ok) {
        return NodeBootstrapStatus::BoardInformationFailed;
    }

    const NodeSettingsLoadStatus settings_status =
        loadNodeSettings(board_information_, node_settings_);

    const bool settings_were_provisioned =
        settings_status == NodeSettingsLoadStatus::Provisioned;

    if (settings_status == NodeSettingsLoadStatus::ReadFailed) {
        return NodeBootstrapStatus::SettingsReadFailed;
    }

    if (settings_status == NodeSettingsLoadStatus::WriteFailed) {
        return NodeBootstrapStatus::SettingsWriteFailed;
    }

    switch (node_settings_.transport) {
        case TransportType::EspNow:
            if (!esp_now_transport_.configureChannel(
                    node_settings_.channel
                )) {
                return NodeBootstrapStatus::InvalidTransportConfiguration;
            }

            selected_transport_ = &esp_now_transport_;
            break;

        case TransportType::Nrf24:
        case TransportType::Ethernet:
        case TransportType::Unspecified:
            return NodeBootstrapStatus::UnsupportedTransport;
    }

    const TransportStatus transport_status =
        selected_transport_->initialize();

    if (transport_status != TransportStatus::Ok) {
        return NodeBootstrapStatus::TransportInitializationFailed;
    }

    // ESP-NOW should be initialized
    // VaydeEngine handoff will be here.

    if (settings_were_provisioned) {
        return NodeBootstrapStatus::ReadyAfterProvisioning;
    }

    return NodeBootstrapStatus::Ready;
}
