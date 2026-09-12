#pragma once

#include <cstdint>

#include "esp_err.h"

struct esp_now_recv_info_t {
    const std::uint8_t* src_addr;
};

using esp_now_recv_cb_t = void (*)(
    const esp_now_recv_info_t* receive_info,
    const std::uint8_t* data,
    int data_length
);

esp_err_t esp_now_init();
esp_err_t esp_now_deinit();
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback);
