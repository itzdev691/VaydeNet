#include "bootstrap/NodeBootstrap.h"

#include <algorithm>
#include <cstdint>

#include "VaydeNet/transmit/TransmitRequest.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kLogTag[] = "NodeBootstrap";
constexpr std::uint32_t kNodePollIntervalMs = 10;
constexpr char kStartupPayload[] = "VaydeNet node online";

static_assert(
    sizeof(kStartupPayload) - 1 <= kMaximumOutboundPayloadSize,
    "Startup message exceeds outbound payload capacity"
);

TransmitRequest makeStartupTransmitRequest() {
    TransmitRequest request{};
    request.destination = TransmitDestination::Broadcast;
    request.message.type = 1;
    request.message.flags = 0;
    request.message.ttl = 1;
    request.message.payload_length =
        static_cast<std::uint16_t>(sizeof(kStartupPayload) - 1);

    std::copy_n(
        kStartupPayload,
        request.message.payload_length,
        request.message.payload.begin()
    );

    return request;
}

bool submitStartupMessage(NodeBootstrap& bootstrap) {
    const TransmitRequest request = makeStartupTransmitRequest();

    switch (bootstrap.tryTransmit(request)) {
        case EngineTransmitStatus::Queued:
            ESP_LOGI(kLogTag, "Startup message queued");
            return true;

        case EngineTransmitStatus::Busy:
            ESP_LOGW(kLogTag, "Transmit transport is busy");
            break;

        case EngineTransmitStatus::InvalidMessageType:
            ESP_LOGE(kLogTag, "Invalid startup message type");
            break;

        case EngineTransmitStatus::InvalidMessageTtl:
            ESP_LOGE(kLogTag, "Invalid startup message TTL");
            break;

        case EngineTransmitStatus::InvalidMessageLength:
            ESP_LOGE(kLogTag, "Invalid startup message length");
            break;

        case EngineTransmitStatus::NotStarted:
            ESP_LOGE(kLogTag, "VaydeEngine is not started");
            break;

        case EngineTransmitStatus::TransportNotReady:
            ESP_LOGE(kLogTag, "Transmit transport is not ready");
            break;

        case EngineTransmitStatus::Unavailable:
            ESP_LOGE(kLogTag, "Transmit capability is unavailable");
            break;

        case EngineTransmitStatus::Failed:
            ESP_LOGE(kLogTag, "Startup message submission failed");
            break;
    }

    return false;
}

const char* packetValidationStatusName(PacketValidationStatus status) {
    switch (status) {
        case PacketValidationStatus::NotChecked:
            return "not checked";

        case PacketValidationStatus::Valid:
            return "valid";

        case PacketValidationStatus::UnsupportedVersion:
            return "unsupported version";

        case PacketValidationStatus::InvalidType:
            return "invalid type";

        case PacketValidationStatus::InvalidTtl:
            return "invalid TTL";

        case PacketValidationStatus::InvalidLength:
            return "invalid length";

        case PacketValidationStatus::CrcMismatch:
            return "CRC mismatch";
    }

    return "unknown validation result";
}

void runNodeLoop(NodeBootstrap& bootstrap, bool transmit_pending) {
    ESP_LOGI(kLogTag, "VaydeEngine node loop started");

    while (true) {
        const EngineProcessResult process_result =
            bootstrap.processNextPacket();

        switch (process_result.status) {
            case EngineProcessStatus::MessageDelivered:
                break;

            case EngineProcessStatus::PacketRejected:
                ESP_LOGW(
                    kLogTag,
                    "VaydeEngine rejected packet: %s",
                    packetValidationStatusName(process_result.validation)
                );
                break;

            case EngineProcessStatus::UnsupportedMessageType:
                ESP_LOGW(kLogTag, "Unsupported logical message type");
                break;

            case EngineProcessStatus::InvalidMessageLength:
                ESP_LOGW(kLogTag, "Invalid logical message length");
                break;

            case EngineProcessStatus::MessageRejected:
                ESP_LOGW(kLogTag, "Logical message sink rejected message");
                break;

            case EngineProcessStatus::QueueEmpty:
                break;

            case EngineProcessStatus::NotStarted:
                ESP_LOGE(kLogTag, "VaydeEngine is not started");
                return;

            case EngineProcessStatus::TransportNotReady:
                ESP_LOGE(
                    kLogTag,
                    "VaydeEngine receive transport is not ready"
                );
                return;

            case EngineProcessStatus::MessageSinkUnavailable:
                ESP_LOGE(kLogTag, "VaydeEngine message sink is unavailable");
                return;
        }

        if (transmit_pending) {
            switch (bootstrap.pollTransmitCompletion()) {
                case EngineTransmitCompletionStatus::Sent:
                    ESP_LOGI(kLogTag, "Startup message sent");
                    transmit_pending = false;
                    break;

                case EngineTransmitCompletionStatus::Failed:
                    ESP_LOGE(kLogTag, "Startup message send failed");
                    transmit_pending = false;
                    break;

                case EngineTransmitCompletionStatus::Pending:
                    break;

                case EngineTransmitCompletionStatus::Empty:
                    ESP_LOGW(kLogTag, "Startup message completion is empty");
                    transmit_pending = false;
                    break;

                case EngineTransmitCompletionStatus::NotStarted:
                    ESP_LOGE(kLogTag, "VaydeEngine is not started");
                    transmit_pending = false;
                    break;

                case EngineTransmitCompletionStatus::TransportNotReady:
                    ESP_LOGE(kLogTag, "Transmit transport is not ready");
                    transmit_pending = false;
                    break;

                case EngineTransmitCompletionStatus::Unavailable:
                    ESP_LOGE(kLogTag, "Transmit completion is unavailable");
                    transmit_pending = false;
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kNodePollIntervalMs));
    }
}

}  // namespace

extern "C" void app_main() {
    // Give the native USB Serial/JTAG monitor time to reconnect after reset.
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(kLogTag, "Node firmware starting");

    static NodeBootstrap bootstrap;
    const NodeBootstrapStatus status = bootstrap.run();

    switch (status) {
        case NodeBootstrapStatus::Ready:
            ESP_LOGI(kLogTag, "Bootstrap ready");
            break;

        case NodeBootstrapStatus::ReadyAfterProvisioning:
            ESP_LOGI(
                kLogTag,
                "Bootstrap ready; default node settings written to NVS"
            );
            break;

        case NodeBootstrapStatus::HardwareIdentityFailed:
            ESP_LOGE(kLogTag, "Hardware identity retrieval failed");
            return;

        case NodeBootstrapStatus::SettingsReadFailed:
            ESP_LOGE(kLogTag, "Node settings read failed");
            return;

        case NodeBootstrapStatus::SettingsWriteFailed:
            ESP_LOGE(kLogTag, "Node settings write failed");
            return;

        case NodeBootstrapStatus::UnsupportedTransport:
            ESP_LOGE(kLogTag, "Configured transport is not supported");
            return;

        case NodeBootstrapStatus::InvalidTransportConfiguration:
            ESP_LOGE(kLogTag, "Transport configuration is invalid");
            return;

        case NodeBootstrapStatus::TransportInitializationFailed:
            ESP_LOGE(kLogTag, "Transport initialization failed");
            return;

        case NodeBootstrapStatus::EngineStartupFailed:
            ESP_LOGE(kLogTag, "VaydeEngine startup failed");
            return;

        default:
            ESP_LOGE(kLogTag, "Bootstrap returned an unknown status");
            return;
    }

    const bool transmit_pending = submitStartupMessage(bootstrap);
    runNodeLoop(bootstrap, transmit_pending);
}
