#pragma once
#include "FreeRTOS.h"
constexpr TickType_t pdMS_TO_TICKS(unsigned ms) { return ms; }
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount();
