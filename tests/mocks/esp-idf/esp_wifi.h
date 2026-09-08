#pragma once

#include <cstdint>

#include "esp_err.h"

struct wifi_init_config_t {};

constexpr int WIFI_STORAGE_RAM = 0;
constexpr int WIFI_MODE_STA = 1;
constexpr int WIFI_SECOND_CHAN_NONE = 0;

#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

esp_err_t esp_wifi_init(const wifi_init_config_t* configuration);
esp_err_t esp_wifi_set_storage(int storage);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_start();
esp_err_t esp_wifi_set_channel(
    std::uint8_t primary,
    int secondary
);
