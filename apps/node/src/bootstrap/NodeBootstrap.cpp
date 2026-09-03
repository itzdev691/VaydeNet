#include "bootstrap/NodeBootstrap.h"
#include "NodeSettingsLoader.h"
#ifndef VAYDENET_ACTIVITY_LED_GPIO
#error "VAYDENET_ACTIVITY_LED_GPIO must be defined"
#endif

void NodeBootstrap::indicatePacketReceived(void* context) {
    if (context != nullptr) {
        static_cast<Esp32RgbLed*>(context)->flash();
    }
}

NodeBootstrapStatus NodeBootstrap::run() {
    // Startup dependencies will be assembled here.
    const Esp32BoardInfoStatus status =
        retrieveEsp32HardwareIdentity(hardware_identity_);

    if (status != Esp32BoardInfoStatus::Ok) {
        return NodeBootstrapStatus::HardwareIdentityFailed;
    }

    const NodeSettingsLoadStatus settings_status =
        loadNodeSettings(hardware_identity_, node_settings_);

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

    if (
        packet_activity_led_.initialize(
            VAYDENET_ACTIVITY_LED_GPIO
        )
    ) {
        esp_now_transport_.setReceiveActivityCallback(
            NodeBootstrap::indicatePacketReceived,
            &packet_activity_led_
        );
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
