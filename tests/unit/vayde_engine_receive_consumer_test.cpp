#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/message/MessageSink.h"
#include "VaydeNet/startup/EngineStartupContext.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/transport/TransportInterface.h"

namespace {

class FakeTransport final : public TransportInterface {
public:
    TransportStatus initialize() override {
        return TransportStatus::Ok;
    }

    TransportReceiveStatus tryReceive(Packet& packet) override {
        ++receive_attempts;

        if (receive_status == TransportReceiveStatus::Received) {
            packet = queued_packet;
        }

        return receive_status;
    }

    TransportReceiveStatus receive_status{
        TransportReceiveStatus::Empty
    };
    Packet queued_packet{};
    std::uint32_t receive_attempts{};
};

class FakeMessageSink final : public MessageSink {
public:
    MessageSinkStatus deliver(const Message& message) override {
        ++delivery_attempts;
        delivered_message = message;
        return MessageSinkStatus::Delivered;
    }

    Message delivered_message{};
    std::uint32_t delivery_attempts{};
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

Packet makeValidPacket() {
    Packet packet{};
    packet.version = 1;
    packet.type = 1;
    packet.ttl = 1;
    packet.crc = computePacketCrc(packet);
    return packet;
}

}  // namespace

int main() {
    FakeTransport transport;
    FakeMessageSink message_sink;
    VaydeEngine engine;

    const EngineProcessResult before_start = engine.processNextPacket();
    expect(
        before_start.status == EngineProcessStatus::NotStarted,
        "engine processed a packet before startup"
    );
    expect(
        before_start.validation == PacketValidationStatus::NotChecked,
        "packet was validated before engine startup"
    );
    expect(
        transport.receive_attempts == 0,
        "engine contacted transport before startup"
    );
    expect(
        message_sink.delivery_attempts == 0,
        "engine contacted message sink before startup"
    );

    HardwareIdentity identity{};
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

    const EngineProcessResult empty_result = engine.processNextPacket();
    expect(
        empty_result.status == EngineProcessStatus::QueueEmpty,
        "empty transport did not map to QueueEmpty"
    );
    expect(
        empty_result.validation == PacketValidationStatus::NotChecked,
        "empty transport produced a validation result"
    );

    transport.receive_status = TransportReceiveStatus::NotInitialized;
    const EngineProcessResult unavailable_result =
        engine.processNextPacket();
    expect(
        unavailable_result.status == EngineProcessStatus::TransportNotReady,
        "uninitialized transport did not map to TransportNotReady"
    );
    expect(
        unavailable_result.validation == PacketValidationStatus::NotChecked,
        "uninitialized transport produced a validation result"
    );

    transport.queued_packet = makeValidPacket();
    transport.queued_packet.sequenceNumber = 42;
    transport.queued_packet.crc =
        computePacketCrc(transport.queued_packet);
    transport.receive_status = TransportReceiveStatus::Received;

    const EngineProcessResult delivered_result =
        engine.processNextPacket();
    expect(
        delivered_result.status == EngineProcessStatus::MessageDelivered,
        "valid received packet was not delivered"
    );
    expect(
        delivered_result.validation == PacketValidationStatus::Valid,
        "delivered packet did not preserve the valid result"
    );
    expect(
        message_sink.delivery_attempts == 1,
        "valid packet was not delivered exactly once"
    );
    expect(
        message_sink.delivered_message.sequence_number == 42,
        "decoded sequence number did not reach the sink"
    );

    transport.queued_packet = makeValidPacket();
    transport.queued_packet.length = 1;
    transport.queued_packet.payload[0] = 0x42U;
    transport.queued_packet.crc =
        computePacketCrc(transport.queued_packet);
    transport.queued_packet.payload[0] ^= 0xFFU;

    const EngineProcessResult rejected_result =
        engine.processNextPacket();
    expect(
        rejected_result.status == EngineProcessStatus::PacketRejected,
        "invalid received packet was not rejected"
    );
    expect(
        rejected_result.validation == PacketValidationStatus::CrcMismatch,
        "rejected packet did not preserve its validation reason"
    );
    expect(
        message_sink.delivery_attempts == 1,
        "rejected packet reached the message sink"
    );
    expect(
        transport.receive_attempts == 4,
        "engine did not make exactly one receive attempt per poll"
    );

    std::puts("PASS: VaydeEngine validates and delivers one packet per poll");
    return EXIT_SUCCESS;
}
