#pragma once
#include "esp_err.h"
enum gpio_num_t { GPIO_NUM_1=1, GPIO_NUM_4=4, GPIO_NUM_5=5, GPIO_NUM_6=6, GPIO_NUM_7=7 };
enum gpio_mode_t { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT };
esp_err_t gpio_set_direction(gpio_num_t pin, gpio_mode_t mode);
esp_err_t gpio_set_level(gpio_num_t pin, unsigned level);
