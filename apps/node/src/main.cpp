#include "bootstrap/NodeBootstrap.h"

#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kLogTag[] = "NodeBootstrap";
constexpr std::uint32_t kReceivePollIntervalMs = 10;

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

void runPacketProcessor(NodeBootstrap& bootstrap) {
    ESP_LOGI(kLogTag, "VaydeEngine packet processor started");

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

        vTaskDelay(pdMS_TO_TICKS(kReceivePollIntervalMs));
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

    runPacketProcessor(bootstrap);
}
