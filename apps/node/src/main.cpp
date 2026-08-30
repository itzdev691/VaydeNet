#include "bootstrap/NodeBootstrap.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kLogTag[] = "NodeBootstrap";

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
            return;

        case NodeBootstrapStatus::ReadyAfterProvisioning:
            ESP_LOGI(
                kLogTag,
                "Bootstrap ready; default node settings written to NVS"
            );
            return;

        case NodeBootstrapStatus::BoardInformationFailed:
            ESP_LOGE(kLogTag, "Board information retrieval failed");
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
    }

    ESP_LOGE(kLogTag, "Bootstrap returned an unknown status");
}
