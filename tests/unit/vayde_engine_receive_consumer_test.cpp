#include <cstdint>
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

}  // namespace

int main() {
    FakeTransport transport;
    VaydeEngine engine;

    expect(
        engine.consumeNextPacket() == EngineReceiveStatus::NotStarted,
        "engine consumed before startup"
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
    expect(
        engine.consumeNextPacket() == EngineReceiveStatus::QueueEmpty,
        "empty transport did not map to QueueEmpty"
    );

    transport.receive_status = TransportReceiveStatus::NotInitialized;
    expect(
        engine.consumeNextPacket() ==
            EngineReceiveStatus::TransportNotReady,
        "uninitialized transport did not map to TransportNotReady"
    );

    transport.queued_packet.sequenceNumber = 42;
    transport.receive_status = TransportReceiveStatus::Received;
    expect(
        engine.consumeNextPacket() ==
            EngineReceiveStatus::PacketDequeued,
        "received packet was not consumed"
    );
    expect(
        transport.receive_attempts == 3,
        "engine did not make exactly one receive attempt per poll"
    );

    std::puts("PASS: VaydeEngine consumes one queued packet per poll");
    return EXIT_SUCCESS;
}
