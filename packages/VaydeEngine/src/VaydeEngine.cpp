#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/packet/Packet.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/startup/EngineStartupContext.h"
#include "VaydeNet/transport/TransportInterface.h"
#include "VaydeNet/packet/PacketValidation.h"

namespace {

constexpr std::uint16_t kSupportedProtocolVersion = 1;
constexpr std::uint16_t kSupportedSettingsVersion = 1;

}  // namespace

EngineStartStatus VaydeEngine::start(
    const EngineStartupContext& context
) {
    if (started_) {
        return EngineStartStatus::StartupFailed;
    }

    if (
        context.identity.board_model == nullptr ||
        context.identity.board_model[0] == '\0'
    ) {
        return EngineStartStatus::StartupFailed;
    }

    if (
        context.settings.protocol_version !=
            kSupportedProtocolVersion ||
        context.settings.settings_version !=
            kSupportedSettingsVersion
    ) {
        return EngineStartStatus::StartupFailed;
    }

    if (
        context.settings.transport ==
        TransportType::Unspecified
    ) {
        return EngineStartStatus::StartupFailed;
    }

    identity_ = &context.identity;
    settings_ = &context.settings;
    transport_ = &context.transport;
    started_ = true;

    return EngineStartStatus::Ok;
}

EngineReceiveResult VaydeEngine::consumeNextPacket() {
    if (!started_) {
        return {
            EngineReceiveStatus::NotStarted,
            PacketValidationStatus::NotChecked,
            Packet{}
        };
    }

    if (transport_ == nullptr) {
        return {
            EngineReceiveStatus::TransportNotReady,
            PacketValidationStatus::NotChecked,
            Packet{}
        };
    }

    Packet packet{};

    switch (transport_->tryReceive(packet)) {
        case TransportReceiveStatus::Received: {
            const PacketValidationStatus validation =
                validatePacket(packet);

            if (validation == PacketValidationStatus::Valid) {
                return {
                    EngineReceiveStatus::PacketAccepted,
                    validation,
                    packet
                };
            }

            return {
                EngineReceiveStatus::PacketRejected,
                validation,
                Packet{}
            };
        }

        case TransportReceiveStatus::Empty:
            return {
                EngineReceiveStatus::QueueEmpty,
                PacketValidationStatus::NotChecked,
                Packet{}
            };

        case TransportReceiveStatus::NotInitialized:
            return {
                EngineReceiveStatus::TransportNotReady,
                PacketValidationStatus::NotChecked,
                Packet{}
            };
    }

    return {
        EngineReceiveStatus::TransportNotReady,
        PacketValidationStatus::NotChecked,
        Packet{}
    };
}
