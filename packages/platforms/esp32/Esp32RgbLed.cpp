#include "Esp32RgbLed.h"

#include "esp_log.h"

#if defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
#include "driver/gpio.h"
#else
#include "led_strip_rmt.h"
#endif

#if defined(VAYDENET_ACTIVITY_LED_ADDRESSABLE) && \
    defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
#error "Only one activity LED backend may be selected"
#endif

#if !defined(VAYDENET_ACTIVITY_LED_ADDRESSABLE) && \
    !defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
#error "An activity LED backend must be selected"
#endif

namespace {

constexpr char kLogTag[] = "Esp32RgbLed";
constexpr std::uint32_t kFlashDurationMs = 60;
constexpr std::uint32_t kWorkerStackSize = 2048;

#if defined(VAYDENET_ACTIVITY_LED_ADDRESSABLE)
constexpr std::uint32_t kGreenBrightness = 16;
#endif

}  // namespace

bool Esp32RgbLed::initialize(int gpio_number) {
    if (worker_task_ != nullptr) {
        return true;
    }

#if defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
    gpio_number_ = gpio_number;

    const gpio_num_t gpio = static_cast<gpio_num_t>(gpio_number_);

    if (
        gpio_reset_pin(gpio) != ESP_OK ||
        gpio_set_direction(gpio, GPIO_MODE_OUTPUT) != ESP_OK ||
        gpio_set_level(gpio, 1) != ESP_OK
    ) {
        ESP_LOGE(kLogTag, "Failed to initialize active-low LED");
        gpio_number_ = -1;
        return false;
    }
#else
    led_strip_config_t strip_configuration{};
    strip_configuration.strip_gpio_num = gpio_number;
    strip_configuration.max_leds = 1;
    strip_configuration.led_model = LED_MODEL_WS2812;
    strip_configuration.led_pixel_format = LED_PIXEL_FORMAT_GRB;
    strip_configuration.flags.invert_out = false;

    led_strip_rmt_config_t rmt_configuration{};
    rmt_configuration.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_configuration.resolution_hz = 10 * 1000 * 1000;
    rmt_configuration.mem_block_symbols = 0;
    rmt_configuration.flags.with_dma = false;

    if (
        led_strip_new_rmt_device(
            &strip_configuration,
            &rmt_configuration,
            &led_strip_
        ) != ESP_OK
    ) {
        ESP_LOGE(kLogTag, "Failed to initialize RGB LED");
        return false;
    }

    if (led_strip_clear(led_strip_) != ESP_OK) {
        ESP_LOGE(kLogTag, "Failed to clear RGB LED");
        (void)led_strip_del(led_strip_);
        led_strip_ = nullptr;
        return false;
    }
#endif

    if (
        xTaskCreate(
            Esp32RgbLed::workerEntry,
            "packet_led",
            kWorkerStackSize,
            this,
            tskIDLE_PRIORITY + 1,
            &worker_task_
        ) != pdPASS
    ) {
        ESP_LOGE(kLogTag, "Failed to create RGB LED task");

#if defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
        (void)gpio_set_level(
            static_cast<gpio_num_t>(gpio_number_),
            1
        );
        gpio_number_ = -1;
#else
        (void)led_strip_del(led_strip_);
        led_strip_ = nullptr;
#endif

        return false;
    }

    return true;
}

void Esp32RgbLed::flash() {
    if (worker_task_ != nullptr) {
        xTaskNotifyGive(worker_task_);
    }
}

void Esp32RgbLed::workerEntry(void* context) {
    static_cast<Esp32RgbLed*>(context)->run();
}

void Esp32RgbLed::run() {
    while (true) {
        // Clear all accumulated notifications so packet bursts do not
        // create several seconds of delayed flashing.
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

#if defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
        (void)gpio_set_level(
            static_cast<gpio_num_t>(gpio_number_),
            0
        );
#else
        if (
            led_strip_set_pixel(
                led_strip_,
                0,
                0,
                kGreenBrightness,
                0
            ) == ESP_OK
        ) {
            (void)led_strip_refresh(led_strip_);
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(kFlashDurationMs));

#if defined(VAYDENET_ACTIVITY_LED_ACTIVE_LOW_GPIO)
        (void)gpio_set_level(
            static_cast<gpio_num_t>(gpio_number_),
            1
        );
#else
        (void)led_strip_clear(led_strip_);
#endif
    }
}
