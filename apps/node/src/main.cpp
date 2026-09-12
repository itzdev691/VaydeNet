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

void logAcceptedPacket(const Packet& packet) {
    ESP_LOGI(
        kLogTag,
        "VaydeEngine accepted packet: type=%u length=%u sequence=%lu",
        static_cast<unsigned>(packet.type),
        static_cast<unsigned>(packet.length),
        static_cast<unsigned long>(packet.sequenceNumber)
    );
}

void runReceiveConsumer(NodeBootstrap& bootstrap) {
    ESP_LOGI(kLogTag, "VaydeEngine receive consumer started");

    while (true) {
        const EngineReceiveResult receive_result =
            bootstrap.consumeNextPacket();

        switch (receive_result.status) {
            case EngineReceiveStatus::PacketAccepted:
                logAcceptedPacket(receive_result.packet);
                break;

            case EngineReceiveStatus::PacketRejected:
                ESP_LOGW(
                    kLogTag,
                    "VaydeEngine rejected packet: %s",
                    packetValidationStatusName(receive_result.validation)
                );
                break;

            case EngineReceiveStatus::QueueEmpty:
                break;

            case EngineReceiveStatus::NotStarted:
                ESP_LOGE(kLogTag, "VaydeEngine is not started");
                return;

            case EngineReceiveStatus::TransportNotReady:
                ESP_LOGE(
                    kLogTag,
                    "VaydeEngine receive transport is not ready"
                );
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

    runReceiveConsumer(bootstrap);
}
