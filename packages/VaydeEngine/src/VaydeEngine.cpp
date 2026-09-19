#include "VaydeNet/VaydeEngine.h"

#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/message/Message.h"
#include "VaydeNet/message/MessageSink.h"
#include "VaydeNet/message/PacketMessageDecoder.h"
#include "VaydeNet/packet/Packet.h"
#include "VaydeNet/packet/PacketValidation.h"
#include "VaydeNet/startup/EngineStartupContext.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/transport/TransportInterface.h"

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
    message_sink_ = &context.message_sink;
    started_ = true;

    return EngineStartStatus::Ok;
}

EngineProcessResult VaydeEngine::processNextPacket() {
    if (!started_) {
        return {
            EngineProcessStatus::NotStarted,
            PacketValidationStatus::NotChecked
        };
    }

    if (transport_ == nullptr) {
        return {
            EngineProcessStatus::TransportNotReady,
            PacketValidationStatus::NotChecked
        };
    }

    if (message_sink_ == nullptr) {
        return {
            EngineProcessStatus::MessageSinkUnavailable,
            PacketValidationStatus::NotChecked
        };
    }

    Packet packet{};

    switch (transport_->tryReceive(packet)) {
        case TransportReceiveStatus::Received: {
            const PacketValidationStatus validation =
                validatePacket(packet);

            if (validation != PacketValidationStatus::Valid) {
                return {
                    EngineProcessStatus::PacketRejected,
                    validation
                };
            }

            Message message{};

            switch (decodeValidatedPacket(packet, message)) {
                case PacketMessageDecodeStatus::Decoded:
                    break;

                case PacketMessageDecodeStatus::UnsupportedType:
                    return {
                        EngineProcessStatus::UnsupportedMessageType,
                        validation
                    };

                case PacketMessageDecodeStatus::InvalidLength:
                    return {
                        EngineProcessStatus::InvalidMessageLength,
                        validation
                    };
            }

            if (
                message_sink_->deliver(message) ==
                MessageSinkStatus::Delivered
            ) {
                return {
                    EngineProcessStatus::MessageDelivered,
                    validation
                };
            }

            return {
                EngineProcessStatus::MessageRejected,
                validation
            };
        }

        case TransportReceiveStatus::Empty:
            return {
                EngineProcessStatus::QueueEmpty,
                PacketValidationStatus::NotChecked
            };

        case TransportReceiveStatus::NotInitialized:
            return {
                EngineProcessStatus::TransportNotReady,
                PacketValidationStatus::NotChecked
            };
    }

    return {
        EngineProcessStatus::TransportNotReady,
        PacketValidationStatus::NotChecked
    };
}
