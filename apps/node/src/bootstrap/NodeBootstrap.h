#pragma once

#include <cstdint>

#include "Esp32BoardInfo.h"
#include "EspNowTransport.h"
#include "Esp32RgbLed.h"
#include "NodeMessageSink.h"
#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/transport/TransportInterface.h"

enum class NodeBootstrapStatus : std::uint8_t {
    Ready,
    ReadyAfterProvisioning,
    HardwareIdentityFailed,
    SettingsReadFailed,
    SettingsWriteFailed,
    UnsupportedTransport,
    InvalidTransportConfiguration,
    TransportInitializationFailed,
    EngineStartupFailed
};

class NodeBootstrap {
public:
    NodeBootstrapStatus run();
    EngineProcessResult processNextPacket();
    EngineTransmitStatus tryTransmit(
        const TransmitRequest& request
    );
    EngineTransmitCompletionStatus pollTransmitCompletion();

private:
    HardwareIdentity hardware_identity_{};
    NodeSettings node_settings_{};
    static void indicatePacketReceived(void* context);

    Esp32RgbLed packet_activity_led_{};
    EspNowTransport esp_now_transport_{};
    NodeMessageSink node_message_sink_{};
    TransportInterface* selected_transport_{nullptr};

    VaydeEngine vayde_engine_{};
};
