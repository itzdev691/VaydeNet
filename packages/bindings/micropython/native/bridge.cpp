#include "bridge.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/message/MessageSink.h"
#include "VaydeNet/startup/EngineStartupContext.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/transmit/TransmitRequest.h"
#include "VaydeNet/transport/TransportInterface.h"

namespace {

// Called only by the MicroPython VM task. No Python objects are retained here.
class PacketBridge final : public TransportInterface {
public:
    bool ready{false};
    std::array<Packet, 4> incoming{};
    size_t head{0}, count{0};
    Packet outgoing{};
    bool pending{false}, available{false}, completed{false}, success{false};

    TransportStatus initialize() override {
        ready = true;
        return TransportStatus::Ok;
    }
    TransportReceiveStatus tryReceive(Packet& packet) override {
        if (!ready) return TransportReceiveStatus::NotInitialized;
        if (!count) return TransportReceiveStatus::Empty;
        packet = incoming[head];
        head = (head + 1) % incoming.size();
        --count;
        return TransportReceiveStatus::Received;
    }
    TransportTransmitStatus tryTransmit(const Packet& packet) override {
        if (!ready) return TransportTransmitStatus::NotInitialized;
        if (pending) return TransportTransmitStatus::Busy;
        outgoing = packet;
        pending = available = true;
        completed = false;
        return TransportTransmitStatus::Queued;
    }
    TransportTransmitCompletionStatus pollTransmitCompletion() override {
        if (!ready) return TransportTransmitCompletionStatus::NotInitialized;
        if (!pending) return TransportTransmitCompletionStatus::Empty;
        if (!completed) return TransportTransmitCompletionStatus::Pending;
        pending = completed = false;
        return success ? TransportTransmitCompletionStatus::Sent
                       : TransportTransmitCompletionStatus::Failed;
    }
};

class Inbox final : public MessageSink {
public:
    Message message{};
    MessageSinkStatus deliver(const Message& value) override {
        message = value;
        return MessageSinkStatus::Delivered;
    }
};

struct Runtime {
    HardwareIdentity identity{};
    NodeSettings settings{};
    PacketBridge transport{};
    Inbox inbox{};
    VaydeEngine engine{};
};

Runtime& runtime() {
    static Runtime instance;
    return instance;
}

} // namespace

extern "C" void vn_stop(void) {
    auto& r = runtime();
    r.engine = VaydeEngine{};
    r.transport = PacketBridge{};
    r.inbox = Inbox{};
}

extern "C" bool vn_init(const uint8_t mac[6]) {
    vn_stop();
    auto& r = runtime();
    std::copy_n(mac, r.identity.device_uid.size(), r.identity.device_uid.begin());
    r.identity.board_model = "MicroPython";
    r.settings.transport = TransportType::EspNow;
    r.transport.initialize();
    const EngineStartupContext context{r.identity, r.settings, r.transport, r.inbox};
    return r.engine.start(context) == EngineStartStatus::Ok;
}

extern "C" const char *vn_send(const uint8_t *payload, size_t length, uint8_t ttl, uint8_t flags) {
    if (length > kMaximumOutboundPayloadSize) return "InvalidMessageLength";
    TransmitRequest request{};
    request.message.type = 1;
    request.message.ttl = ttl;
    request.message.flags = flags;
    request.message.payload_length = static_cast<uint16_t>(length);
    if (length) std::copy_n(payload, length, request.message.payload.begin());
    switch (runtime().engine.tryTransmit(request)) {
        case EngineTransmitStatus::Queued: return "Queued";
        case EngineTransmitStatus::Busy: return "Busy";
        case EngineTransmitStatus::InvalidMessageType: return "InvalidMessageType";
        case EngineTransmitStatus::InvalidMessageTtl: return "InvalidMessageTtl";
        case EngineTransmitStatus::InvalidMessageLength: return "InvalidMessageLength";
        case EngineTransmitStatus::NotStarted: return "NotStarted";
        case EngineTransmitStatus::TransportNotReady: return "TransportNotReady";
        case EngineTransmitStatus::Unavailable: return "Unavailable";
        case EngineTransmitStatus::Failed: return "Failed";
    }
    return "Failed";
}

extern "C" bool vn_take_tx(uint8_t packet[220]) {
    auto& t = runtime().transport;
    if (!t.available) return false;
    static_assert(sizeof(Packet) == 220);
    std::memcpy(packet, &t.outgoing, sizeof(Packet));
    t.available = false;
    return true;
}

extern "C" bool vn_complete_tx(bool success) {
    auto& t = runtime().transport;
    if (!t.pending || t.available || t.completed) return false;
    t.completed = true;
    t.success = success;
    return true;
}

extern "C" const char *vn_poll_tx(void) {
    switch (runtime().engine.pollTransmitCompletion()) {
        case EngineTransmitCompletionStatus::Sent: return "Sent";
        case EngineTransmitCompletionStatus::Failed: return "Failed";
        case EngineTransmitCompletionStatus::Pending: return "Pending";
        case EngineTransmitCompletionStatus::Empty: return "Empty";
        case EngineTransmitCompletionStatus::NotStarted: return "NotStarted";
        case EngineTransmitCompletionStatus::TransportNotReady: return "TransportNotReady";
        case EngineTransmitCompletionStatus::Unavailable: return "Unavailable";
    }
    return "Failed";
}

extern "C" bool vn_feed(const uint8_t packet[220]) {
    auto& t = runtime().transport;
    if (!t.ready || t.count == t.incoming.size()) return false;
    std::memcpy(&t.incoming[(t.head + t.count) % t.incoming.size()], packet, sizeof(Packet));
    ++t.count;
    return true;
}

extern "C" const char *vn_receive(vn_message *message, uint8_t *validation) {
    auto& r = runtime();
    const auto result = r.engine.processNextPacket();
    *validation = static_cast<uint8_t>(result.validation);
    switch (result.status) {
        case EngineProcessStatus::MessageDelivered: {
            const auto& m = r.inbox.message;
            *message = {};
            message->version = m.protocol_version;
            message->type = m.type;
            message->flags = m.flags;
            message->ttl = m.ttl;
            message->source = m.source_node_id;
            message->sequence = m.sequence_number;
            message->length = m.payload_length;
            std::copy_n(m.payload.begin(), m.payload_length, message->payload);
            return "MessageDelivered";
        }
        case EngineProcessStatus::QueueEmpty: return "QueueEmpty";
        case EngineProcessStatus::PacketRejected: return "PacketRejected";
        case EngineProcessStatus::UnsupportedMessageType: return "UnsupportedMessageType";
        case EngineProcessStatus::InvalidMessageLength: return "InvalidMessageLength";
        case EngineProcessStatus::MessageRejected: return "MessageRejected";
        case EngineProcessStatus::NotStarted: return "NotStarted";
        case EngineProcessStatus::TransportNotReady: return "TransportNotReady";
        case EngineProcessStatus::MessageSinkUnavailable: return "MessageSinkUnavailable";
    }
    return "PacketRejected";
}
