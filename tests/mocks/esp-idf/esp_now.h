#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

constexpr std::size_t ESP_NOW_ETH_ALEN = 6;

struct esp_now_recv_info_t {
    const std::uint8_t* src_addr;
};

struct esp_now_send_info_t {};

enum esp_now_send_status_t {
    ESP_NOW_SEND_SUCCESS,
    ESP_NOW_SEND_FAIL
};

struct esp_now_peer_info_t {
    std::uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    std::uint8_t channel;
    int ifidx;
    bool encrypt;
};

using esp_now_recv_cb_t = void (*)(
    const esp_now_recv_info_t* receive_info,
    const std::uint8_t* data,
    int data_length
);

using esp_now_send_cb_t = void (*)(
    const esp_now_send_info_t* send_info,
    esp_now_send_status_t status
);

esp_err_t esp_now_init();
esp_err_t esp_now_deinit();
esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback);
esp_err_t esp_now_unregister_recv_cb();
esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback);
esp_err_t esp_now_unregister_send_cb();
esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer);
esp_err_t esp_now_del_peer(const std::uint8_t* peer_address);
esp_err_t esp_now_send(
    const std::uint8_t* peer_address,
    const std::uint8_t* data,
    std::size_t data_length
);
