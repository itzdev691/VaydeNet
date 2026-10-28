#include "Nrf24Transport.h"
#include "esp_log.h"

extern "C" void app_main() {
    // Static lifetime retains the configured SPI device after this one-shot check.
    static Nrf24Transport transport;
    ESP_LOGI("nrf24-check", "Starting one-shot register checks; nothing is transmitted");
    const auto result = transport.initialize();
    if (result == TransportStatus::Ok) {
        ESP_LOGI("nrf24-check", "Checks passed; radio left listening on pipe 0");
    } else {
        ESP_LOGW("nrf24-check", "Transport unavailable; inspect the register-check log above");
    }
}
