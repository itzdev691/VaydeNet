#include "VaydeNet/VaydeEngine.h"

#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/message/Message.h"
#include "VaydeNet/message/MessageSink.h"
#include "VaydeNet/message/PacketMessageDecoder.h"
#include "VaydeNet/message/PacketMessageEncoder.h"
#include "VaydeNet/transmit/TransmitRequest.h"
#include "VaydeNet/packet/Packet.h"
#include "VaydeNet/packet/PacketValidation.h"
#include "VaydeNet/startup/EngineStartupContext.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/transport/TransportInterface.h"

namespace {

constexpr std::uint16_t kSupportedProtocolVersion = 1;
constexpr std::uint16_t kSupportedSettingsVersion = 1;

std::uint64_t makeSenderId(const HardwareIdentity& identity) {
    std::uint64_t sender_id = 0;

    for (const std::uint8_t byte : identity.device_uid) {
        sender_id = (sender_id << 8U) | byte;
    }

    return sender_id;
}
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

EngineTransmitStatus VaydeEngine::tryTransmit(
    const TransmitRequest& request
) {
    if (!started_ || identity_ == nullptr) {
        return EngineTransmitStatus::NotStarted;
    }

    if (transport_ == nullptr) {
        return EngineTransmitStatus::TransportNotReady;
    }

    if (
        request.destination !=
        TransmitDestination::Broadcast
    ) {
        return EngineTransmitStatus::Unavailable;
    }

    Packet packet{};

    switch (
        encodeOutboundMessage(
            request.message,
            makeSenderId(*identity_),
            next_sequence_number_,
            packet
        )
    ) {
        case PacketMessageEncodeStatus::Encoded:
            break;

        case PacketMessageEncodeStatus::InvalidType:
            return EngineTransmitStatus::InvalidMessageType;

        case PacketMessageEncodeStatus::InvalidTtl:
            return EngineTransmitStatus::InvalidMessageTtl;

        case PacketMessageEncodeStatus::InvalidLength:
            return EngineTransmitStatus::InvalidMessageLength;
    }

    switch (transport_->tryTransmit(packet)) {
        case TransportTransmitStatus::Queued:
            ++next_sequence_number_;
            return EngineTransmitStatus::Queued;

        case TransportTransmitStatus::Busy:
            return EngineTransmitStatus::Busy;

        case TransportTransmitStatus::NotInitialized:
            return EngineTransmitStatus::TransportNotReady;

        case TransportTransmitStatus::Unavailable:
            return EngineTransmitStatus::Unavailable;

        case TransportTransmitStatus::Failed:
            return EngineTransmitStatus::Failed;
    }

    return EngineTransmitStatus::Failed;
}

EngineTransmitCompletionStatus
VaydeEngine::pollTransmitCompletion() {
    if (!started_) {
        return EngineTransmitCompletionStatus::NotStarted;
    }

    if (transport_ == nullptr) {
        return EngineTransmitCompletionStatus::TransportNotReady;
    }

    switch (transport_->pollTransmitCompletion()) {
        case TransportTransmitCompletionStatus::Sent:
            return EngineTransmitCompletionStatus::Sent;

        case TransportTransmitCompletionStatus::Failed:
            return EngineTransmitCompletionStatus::Failed;

        case TransportTransmitCompletionStatus::Pending:
            return EngineTransmitCompletionStatus::Pending;

        case TransportTransmitCompletionStatus::Empty:
            return EngineTransmitCompletionStatus::Empty;

        case TransportTransmitCompletionStatus::NotInitialized:
            return EngineTransmitCompletionStatus::TransportNotReady;

        case TransportTransmitCompletionStatus::Unavailable:
            return EngineTransmitCompletionStatus::Unavailable;
    }

    return EngineTransmitCompletionStatus::Failed;
}
