#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/message/MessageSink.h"
#include "VaydeNet/message/PacketMessageDecoder.h"
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
        packet = queued_packet;
        return TransportReceiveStatus::Received;
    }

    TransportTransmitStatus tryTransmit(const Packet&) override {
        return TransportTransmitStatus::Unavailable;
    }

    TransportTransmitCompletionStatus pollTransmitCompletion() override {
        return TransportTransmitCompletionStatus::Unavailable;
    }

    Packet queued_packet{};
    std::uint32_t receive_attempts{};
};

class RecordingMessageSink final : public MessageSink {
public:
    MessageSinkStatus deliver(const Message& message) override {
        ++delivery_attempts;
        delivered_message = message;
        return result;
    }

    MessageSinkStatus result{MessageSinkStatus::Delivered};
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

Packet makePacket(std::uint8_t type) {
    Packet packet{};
    packet.version = 1;
    packet.type = type;
    packet.flags = 3;
    packet.ttl = 2;
    packet.length = 3;
    packet.senderID = 0x1122334455667788ULL;
    packet.sequenceNumber = 73;
    packet.payload[0] = 0x10U;
    packet.payload[1] = 0x20U;
    packet.payload[2] = 0x30U;
    packet.crc = computePacketCrc(packet);
    return packet;
}

}  // namespace

int main() {
    FakeTransport transport;
    RecordingMessageSink message_sink;
    VaydeEngine engine;

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

    transport.queued_packet = makePacket(1);
    const EngineProcessResult delivered = engine.processNextPacket();

    expect(
        delivered.status == EngineProcessStatus::MessageDelivered,
        "supported packet was not delivered"
    );
    expect(
        delivered.validation == PacketValidationStatus::Valid,
        "delivered packet did not retain valid status"
    );
    expect(
        message_sink.delivery_attempts == 1,
        "supported packet was not delivered exactly once"
    );
    expect(
        message_sink.delivered_message.protocol_version == 1 &&
            message_sink.delivered_message.type == 1 &&
            message_sink.delivered_message.flags == 3 &&
            message_sink.delivered_message.ttl == 2,
        "logical message metadata was decoded incorrectly"
    );
    expect(
        message_sink.delivered_message.source_node_id ==
            0x1122334455667788ULL &&
            message_sink.delivered_message.sequence_number == 73,
        "logical message identity was decoded incorrectly"
    );
    expect(
        message_sink.delivered_message.payload_length == 3 &&
            message_sink.delivered_message.payload[0] == 0x10U &&
            message_sink.delivered_message.payload[1] == 0x20U &&
            message_sink.delivered_message.payload[2] == 0x30U,
        "logical message payload was decoded incorrectly"
    );

    transport.queued_packet = makePacket(2);
    const EngineProcessResult unsupported = engine.processNextPacket();
    expect(
        unsupported.status == EngineProcessStatus::UnsupportedMessageType,
        "unsupported packet type was not rejected by the decoder"
    );
    expect(
        unsupported.validation == PacketValidationStatus::Valid,
        "unsupported logical type did not first pass packet validation"
    );
    expect(
        message_sink.delivery_attempts == 1,
        "unsupported packet type reached the message sink"
    );

    message_sink.result = MessageSinkStatus::Rejected;
    transport.queued_packet = makePacket(1);
    const EngineProcessResult sink_rejected = engine.processNextPacket();
    expect(
        sink_rejected.status == EngineProcessStatus::MessageRejected,
        "message sink rejection was not preserved"
    );
    expect(
        message_sink.delivery_attempts == 2,
        "sink rejection did not follow one delivery attempt"
    );

    Packet oversized_packet = makePacket(1);
    oversized_packet.length = 201;
    Message stale_message{};
    stale_message.type = 9;
    stale_message.payload_length = 9;

    expect(
        decodeValidatedPacket(oversized_packet, stale_message) ==
            PacketMessageDecodeStatus::InvalidLength,
        "decoder accepted an oversized payload"
    );
    expect(
        stale_message.type == 0 && stale_message.payload_length == 0,
        "decoder did not clear output before rejecting the packet"
    );
    expect(
        transport.receive_attempts == 3,
        "engine did not make exactly one receive attempt per process call"
    );

    std::puts("PASS: VaydeEngine decodes and dispatches logical messages");
    return EXIT_SUCCESS;
}
