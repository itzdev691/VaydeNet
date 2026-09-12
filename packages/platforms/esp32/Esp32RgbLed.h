#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

class Esp32RgbLed {
public:
    bool initialize(int gpio_number);
    void flash();

private:
    static void workerEntry(void* context);
    void run();

    led_strip_handle_t led_strip_{nullptr};
    TaskHandle_t worker_task_{nullptr};
    int gpio_number_{-1};
};
