#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/message/MessageSink.h"
#include "VaydeNet/packet/PacketValidation.h"
#include "VaydeNet/startup/EngineStartupContext.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/transmit/TransmitRequest.h"
#include "VaydeNet/transport/TransportInterface.h"

namespace {

class FakeTransport final : public TransportInterface {
public:
    TransportStatus initialize() override {
        return TransportStatus::Ok;
    }

    TransportReceiveStatus tryReceive(Packet&) override {
        return TransportReceiveStatus::Empty;
    }

    TransportTransmitStatus tryTransmit(const Packet& packet) override {
        ++transmit_attempts;
        transmitted_packet = packet;
        return transmit_status;
    }

    TransportTransmitCompletionStatus pollTransmitCompletion() override {
        ++completion_polls;
        return completion_status;
    }

    TransportTransmitStatus transmit_status{
        TransportTransmitStatus::Queued
    };
    TransportTransmitCompletionStatus completion_status{
        TransportTransmitCompletionStatus::Empty
    };
    Packet transmitted_packet{};
    std::uint32_t transmit_attempts{};
    std::uint32_t completion_polls{};
};

class FakeMessageSink final : public MessageSink {
public:
    MessageSinkStatus deliver(const Message&) override {
        return MessageSinkStatus::Delivered;
    }
};

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, const char* message) {
    if (!condition) {
        fail(message);
    }
}

TransmitRequest makeValidRequest() {
    TransmitRequest request{};
    request.destination = TransmitDestination::Broadcast;
    request.message.type = 1;
    request.message.flags = 3;
    request.message.ttl = 2;
    request.message.payload_length = 5;
    request.message.payload[0] = 'h';
    request.message.payload[1] = 'e';
    request.message.payload[2] = 'l';
    request.message.payload[3] = 'l';
    request.message.payload[4] = 'o';
    return request;
}

void expectCompletionMapping(
    VaydeEngine& engine,
    FakeTransport& transport,
    TransportTransmitCompletionStatus transport_status,
    EngineTransmitCompletionStatus expected_engine_status,
    const char* failure_message
) {
    transport.completion_status = transport_status;
    expect(
        engine.pollTransmitCompletion() == expected_engine_status,
        failure_message
    );
}

}  // namespace

int main() {
    FakeTransport transport;
    FakeMessageSink message_sink;
    VaydeEngine engine;
    const TransmitRequest valid_request = makeValidRequest();

    expect(
        engine.tryTransmit(valid_request) ==
            EngineTransmitStatus::NotStarted,
        "engine transmitted before startup"
    );
    expect(
        engine.pollTransmitCompletion() ==
            EngineTransmitCompletionStatus::NotStarted,
        "engine polled transmit completion before startup"
    );
    expect(
        transport.transmit_attempts == 0 &&
            transport.completion_polls == 0,
        "engine contacted transport before startup"
    );

    HardwareIdentity identity{};
    identity.device_uid = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    identity.board_model = "Host-Test-Board";

    NodeSettings settings{};
    settings.transport = TransportType::EspNow;

    const EngineStartupContext context{
        identity,
        settings,
        transport,
        message_sink
    };

    expect(
        engine.start(context) == EngineStartStatus::Ok,
        "valid engine startup failed"
    );

    TransmitRequest invalid_request = valid_request;
    invalid_request.destination =
        static_cast<TransmitDestination>(0xFFU);
    expect(
        engine.tryTransmit(invalid_request) ==
            EngineTransmitStatus::Unavailable,
        "unsupported destination was not rejected"
    );

    invalid_request = valid_request;
    invalid_request.message.type = 2;
    expect(
        engine.tryTransmit(invalid_request) ==
            EngineTransmitStatus::InvalidMessageType,
        "invalid outbound message type was not preserved"
    );

    invalid_request = valid_request;
    invalid_request.message.ttl = 0;
    expect(
        engine.tryTransmit(invalid_request) ==
            EngineTransmitStatus::InvalidMessageTtl,
        "invalid outbound message TTL was not preserved"
    );

    invalid_request = valid_request;
    invalid_request.message.payload_length = 201;
    expect(
        engine.tryTransmit(invalid_request) ==
            EngineTransmitStatus::InvalidMessageLength,
        "invalid outbound message length was not preserved"
    );
    expect(
        transport.transmit_attempts == 0,
        "invalid transmit request reached the transport"
    );

    transport.transmit_status = TransportTransmitStatus::Busy;
    expect(
        engine.tryTransmit(valid_request) == EngineTransmitStatus::Busy,
        "transport Busy result was not preserved"
    );
    expect(
        transport.transmitted_packet.sequenceNumber == 0,
        "Busy submission used the wrong initial sequence"
    );

    transport.transmit_status = TransportTransmitStatus::Failed;
    expect(
        engine.tryTransmit(valid_request) == EngineTransmitStatus::Failed,
        "transport Failed result was not preserved"
    );
    expect(
        transport.transmitted_packet.sequenceNumber == 0,
        "failed submission consumed a sequence number"
    );

    transport.transmit_status = TransportTransmitStatus::NotInitialized;
    expect(
        engine.tryTransmit(valid_request) ==
            EngineTransmitStatus::TransportNotReady,
        "uninitialized transport was not mapped to TransportNotReady"
    );

    transport.transmit_status = TransportTransmitStatus::Unavailable;
    expect(
        engine.tryTransmit(valid_request) ==
            EngineTransmitStatus::Unavailable,
        "unavailable transport result was not preserved"
    );

    transport.transmit_status = TransportTransmitStatus::Queued;
    expect(
        engine.tryTransmit(valid_request) == EngineTransmitStatus::Queued,
        "valid outbound message was not queued"
    );

    const Packet& first_packet = transport.transmitted_packet;
    expect(
        first_packet.version == 1 &&
            first_packet.type == valid_request.message.type &&
            first_packet.flags == valid_request.message.flags &&
            first_packet.ttl == valid_request.message.ttl,
        "engine encoded incorrect packet metadata"
    );
    expect(
        first_packet.length == valid_request.message.payload_length &&
            first_packet.senderID == 0x0123456789ABULL &&
            first_packet.sequenceNumber == 0,
        "engine encoded incorrect packet identity"
    );
    expect(
        first_packet.payload[0] == 'h' &&
            first_packet.payload[1] == 'e' &&
            first_packet.payload[2] == 'l' &&
            first_packet.payload[3] == 'l' &&
            first_packet.payload[4] == 'o' &&
            first_packet.payload[5] == 0,
        "engine encoded incorrect packet payload"
    );
    expect(
        validatePacket(first_packet) == PacketValidationStatus::Valid,
        "engine produced an invalid packet"
    );

    expect(
        engine.tryTransmit(valid_request) == EngineTransmitStatus::Queued,
        "second valid outbound message was not queued"
    );
    expect(
        transport.transmitted_packet.sequenceNumber == 1,
        "queued submission did not advance the sequence number"
    );

    expectCompletionMapping(
        engine,
        transport,
        TransportTransmitCompletionStatus::Sent,
        EngineTransmitCompletionStatus::Sent,
        "transport Sent completion was not preserved"
    );
    expectCompletionMapping(
        engine,
        transport,
        TransportTransmitCompletionStatus::Failed,
        EngineTransmitCompletionStatus::Failed,
        "transport Failed completion was not preserved"
    );
    expectCompletionMapping(
        engine,
        transport,
        TransportTransmitCompletionStatus::Pending,
        EngineTransmitCompletionStatus::Pending,
        "transport Pending completion was not preserved"
    );
    expectCompletionMapping(
        engine,
        transport,
        TransportTransmitCompletionStatus::Empty,
        EngineTransmitCompletionStatus::Empty,
        "transport Empty completion was not preserved"
    );
    expectCompletionMapping(
        engine,
        transport,
        TransportTransmitCompletionStatus::NotInitialized,
        EngineTransmitCompletionStatus::TransportNotReady,
        "uninitialized completion was not mapped to TransportNotReady"
    );
    expectCompletionMapping(
        engine,
        transport,
        TransportTransmitCompletionStatus::Unavailable,
        EngineTransmitCompletionStatus::Unavailable,
        "unavailable completion was not preserved"
    );
    expect(
        transport.completion_polls == 6,
        "engine did not make exactly one transport poll per completion call"
    );

    std::puts("PASS: VaydeEngine encodes and submits outbound messages");
    return EXIT_SUCCESS;
}
