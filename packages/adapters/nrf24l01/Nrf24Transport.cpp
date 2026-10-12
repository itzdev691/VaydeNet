#include "Nrf24Transport.h"
#include "Nrf24Packet.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"

#define CE_PIN   GPIO_NUM_5
#define CSN_PIN  GPIO_NUM_4
#define MOSI_PIN GPIO_NUM_1
#define MISO_PIN GPIO_NUM_6
#define SCK_PIN  GPIO_NUM_7

Nrf24Transport::~Nrf24Transport() {
    if (spi_device_ != nullptr) {
        (void)spi_bus_remove_device(spi_device_);
        (void)spi_bus_free(SPI2_HOST);
    }
}

TransportStatus Nrf24Transport::initialize() {
    // Preserve the registered device if initialization is retried.
    if (spi_device_ == nullptr) {
        if (
            gpio_set_direction(CE_PIN, GPIO_MODE_OUTPUT) != ESP_OK ||
            gpio_set_direction(CSN_PIN, GPIO_MODE_OUTPUT) != ESP_OK ||
            gpio_set_direction(MOSI_PIN, GPIO_MODE_OUTPUT) != ESP_OK ||
            gpio_set_direction(MISO_PIN, GPIO_MODE_INPUT) != ESP_OK ||
            gpio_set_direction(SCK_PIN, GPIO_MODE_OUTPUT) != ESP_OK ||
            gpio_set_level(CE_PIN, 0) != ESP_OK ||
            gpio_set_level(CSN_PIN, 1) != ESP_OK
        ) {
            return TransportStatus::InitializationFailed;
        }

        spi_bus_config_t bus_config{};
        bus_config.mosi_io_num = MOSI_PIN;
        bus_config.miso_io_num = MISO_PIN;
        bus_config.sclk_io_num = SCK_PIN;
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.data4_io_num = -1;
        bus_config.data5_io_num = -1;
        bus_config.data6_io_num = -1;
        bus_config.data7_io_num = -1;

        if (spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_DISABLED) != ESP_OK) {
            return TransportStatus::InitializationFailed;
        }

        spi_device_interface_config_t interface_config{};
        interface_config.clock_speed_hz = 1000000; // 1 MHz
        interface_config.mode = 0;                // SPI mode 0
        interface_config.spics_io_num = CSN_PIN;
        interface_config.queue_size = 1;

        if (spi_bus_add_device(SPI2_HOST, &interface_config, &spi_device_) != ESP_OK) {
            (void)spi_bus_free(SPI2_HOST);
            return TransportStatus::InitializationFailed;
        }
    }

    constexpr uint8_t RF_CH = 0x05;
    constexpr uint8_t channel = 76;

    const uint8_t write_bytes[2] = {
        0x20 | RF_CH, // Write-register command + register address
        channel      // Value to store
    };

    spi_transaction_t write_transaction{};
    write_transaction.length = 16; // Two bytes, measured in bits
    write_transaction.tx_buffer = write_bytes;

    if (spi_device_transmit(spi_device_, &write_transaction) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }
    const uint8_t read_bytes[2] = {
        RF_CH, // Read-register command is 0x00 + register address
        0xFF   // Dummy byte supplies clock pulses for the response
    };

    uint8_t response[2]{};

    spi_transaction_t read_transaction{};
    read_transaction.length = 16;
    read_transaction.rxlength = 16;
    read_transaction.tx_buffer = read_bytes;
    read_transaction.rx_buffer = response;

    if (spi_device_transmit(spi_device_, &read_transaction) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    const uint8_t actual_channel = response[1];

    if (actual_channel != channel) {
        return TransportStatus::InitializationFailed;
    }

    constexpr uint8_t CONFIG = 0x00;
    constexpr uint8_t EN_CRC = 1U << 3;
    constexpr uint8_t CRCO = 1U << 2;
    constexpr uint8_t PWR_UP = 1U << 1;
    // Two-byte radio CRC, powered up, PRIM_RX cleared. CE stays low for standby.
    constexpr uint8_t config = EN_CRC | CRCO | PWR_UP;

    const uint8_t config_write_bytes[2] = {0x20 | CONFIG, config};
    spi_transaction_t config_write_transaction{};
    config_write_transaction.length = 16;
    config_write_transaction.tx_buffer = config_write_bytes;

    if (spi_device_transmit(spi_device_, &config_write_transaction) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    // Allow the internal crystal to settle after PWR_UP before further startup.
    esp_rom_delay_us(1500);

    const uint8_t config_read_bytes[2] = {CONFIG, 0xFF};
    uint8_t config_response[2]{};
    spi_transaction_t config_read_transaction{};
    config_read_transaction.length = 16;
    config_read_transaction.rxlength = 16;
    config_read_transaction.tx_buffer = config_read_bytes;
    config_read_transaction.rx_buffer = config_response;

    if (spi_device_transmit(spi_device_, &config_read_transaction) != ESP_OK ||
        config_response[1] != config) {
        return TransportStatus::InitializationFailed;
    }
    constexpr uint8_t TX_ADDR = 0x10;

    // Example address, in SPI transfer order.
    const uint8_t bytes[6] = {
        0x20 | TX_ADDR,
        0xD2, 0xF0, 0xF0, 0xF0, 0xF0
    };

    spi_transaction_t transaction{};
    transaction.length = sizeof(bytes) * 8;
    transaction.tx_buffer = bytes;

    if (spi_device_transmit(spi_device_, &transaction) != ESP_OK) {
        return TransportStatus::InitializationFailed;
    }

    // TODO: Complete address setup, FIFOs and RX/TX before reporting readiness.
    return TransportStatus::InitializationFailed;
}

TransportReceiveStatus Nrf24Transport::tryReceive(Packet&) {
    // TODO: Read a received frame after the nRF24 framing contract is defined.
    return TransportReceiveStatus::NotInitialized;
}

TransportTransmitStatus Nrf24Transport::tryTransmit(const Packet&) {
    // TODO: Encode and submit a frame without waiting for transmission completion.
    return TransportTransmitStatus::NotInitialized;
}

TransportTransmitCompletionStatus Nrf24Transport::pollTransmitCompletion() {
    // TODO: Check radio status and report pending, sent, or failed transmission.
    return TransportTransmitCompletionStatus::NotInitialized;
}
