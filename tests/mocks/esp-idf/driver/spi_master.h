#pragma once
#include <cstddef>
#include "esp_err.h"
using spi_device_handle_t = void*;
constexpr int SPI2_HOST = 2;
constexpr int SPI_DMA_DISABLED = 0;
struct spi_bus_config_t {
    int mosi_io_num{}, miso_io_num{}, sclk_io_num{}, quadwp_io_num{}, quadhd_io_num{};
    int data4_io_num{}, data5_io_num{}, data6_io_num{}, data7_io_num{};
};
struct spi_device_interface_config_t {
    int clock_speed_hz{}, mode{}, spics_io_num{}, queue_size{};
};
struct spi_transaction_t {
    std::size_t length{}, rxlength{};
    const void* tx_buffer{};
    void* rx_buffer{};
};
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t* config, int dma);
esp_err_t spi_bus_add_device(int host, const spi_device_interface_config_t* config,
                             spi_device_handle_t* device);
esp_err_t spi_bus_remove_device(spi_device_handle_t device);
esp_err_t spi_bus_free(int host);
esp_err_t spi_device_transmit(spi_device_handle_t device, spi_transaction_t* transaction);
