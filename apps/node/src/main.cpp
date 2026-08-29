#include "bootstrap/NodeBootstrap.h"

#include "esp_log.h"

namespace {

constexpr char kLogTag[] = "NodeBootstrap";

}  // namespace

extern "C" void app_main() {
    static NodeBootstrap bootstrap;
    const NodeBootstrapStatus status = bootstrap.run();

    switch (status) {
        case NodeBootstrapStatus::Ready:
            ESP_LOGI(kLogTag, "Bootstrap ready");
            return;

        case NodeBootstrapStatus::NotConfigured:
            ESP_LOGW(kLogTag, "Node settings are not configured");
            return;

        case NodeBootstrapStatus::BoardInformationFailed:
            ESP_LOGE(kLogTag, "Board information retrieval failed");
            return;

        case NodeBootstrapStatus::SettingsReadFailed:
            ESP_LOGE(kLogTag, "Node settings read failed");
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
