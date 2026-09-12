#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
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

bool packetsMatch(const Packet& left, const Packet& right) {
    return std::memcmp(&left, &right, sizeof(Packet)) == 0;
}

bool isZeroPacket(const Packet& packet) {
    const Packet zero_packet{};
    return packetsMatch(packet, zero_packet);
}

}  // namespace

int main() {
    FakeTransport transport;
    VaydeEngine engine;

    const EngineReceiveResult before_start = engine.consumeNextPacket();
    expect(
        before_start.status == EngineReceiveStatus::NotStarted,
        "engine consumed before startup"
    );
    expect(
        before_start.validation == PacketValidationStatus::NotChecked,
        "packet was validated before engine startup"
    );
    expect(
        isZeroPacket(before_start.packet),
        "pre-start result exposed packet data"
    );
    expect(
        transport.receive_attempts == 0,
        "engine contacted transport before startup"
    );

    HardwareIdentity identity{};
    identity.board_model = "Host-Test-Board";

    NodeSettings settings{};
    settings.transport = TransportType::EspNow;

    const EngineStartupContext context{
        identity,
        settings,
        transport
    };

    expect(
        engine.start(context) == EngineStartStatus::Ok,
        "valid engine startup failed"
    );
    const EngineReceiveResult empty_result = engine.consumeNextPacket();
    expect(
        empty_result.status == EngineReceiveStatus::QueueEmpty,
        "empty transport did not map to QueueEmpty"
    );
    expect(
        empty_result.validation == PacketValidationStatus::NotChecked,
        "empty transport produced a validation result"
    );
    expect(
        isZeroPacket(empty_result.packet),
        "empty transport exposed packet data"
    );

    transport.receive_status = TransportReceiveStatus::NotInitialized;
    const EngineReceiveResult unavailable_result =
        engine.consumeNextPacket();
    expect(
        unavailable_result.status == EngineReceiveStatus::TransportNotReady,
        "uninitialized transport did not map to TransportNotReady"
    );
    expect(
        unavailable_result.validation == PacketValidationStatus::NotChecked,
        "uninitialized transport produced a validation result"
    );
    expect(
        isZeroPacket(unavailable_result.packet),
        "uninitialized transport exposed packet data"
    );

    transport.queued_packet = makeValidPacket();
    transport.queued_packet.sequenceNumber = 42;
    transport.queued_packet.crc =
        computePacketCrc(transport.queued_packet);
    transport.receive_status = TransportReceiveStatus::Received;
    const EngineReceiveResult accepted_result = engine.consumeNextPacket();
    expect(
        accepted_result.status == EngineReceiveStatus::PacketAccepted,
        "valid received packet was not accepted"
    );
    expect(
        accepted_result.validation == PacketValidationStatus::Valid,
        "accepted packet did not preserve the valid result"
    );
    expect(
        packetsMatch(accepted_result.packet, transport.queued_packet),
        "accepted result did not return the unchanged dequeued packet"
    );

    transport.queued_packet = makeValidPacket();
    transport.queued_packet.length = 1;
    transport.queued_packet.payload[0] = 0x42U;
    transport.queued_packet.crc =
        computePacketCrc(transport.queued_packet);
    transport.queued_packet.payload[0] ^= 0xFFU;

    const EngineReceiveResult rejected_result = engine.consumeNextPacket();
    expect(
        rejected_result.status == EngineReceiveStatus::PacketRejected,
        "invalid received packet was not rejected"
    );
    expect(
        rejected_result.validation == PacketValidationStatus::CrcMismatch,
        "rejected packet did not preserve its validation reason"
    );
    expect(
        isZeroPacket(rejected_result.packet),
        "rejected result exposed untrusted packet data"
    );
    expect(
        transport.receive_attempts == 4,
        "engine did not make exactly one receive attempt per poll"
    );

    std::puts("PASS: VaydeEngine validates one queued packet per poll");
    return EXIT_SUCCESS;
}
