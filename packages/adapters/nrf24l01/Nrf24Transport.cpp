#include "Nrf24Transport.h"
#include "Nrf24Packet.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

#define CE_PIN   GPIO_NUM_5
#define CSN_PIN  GPIO_NUM_4
#define MOSI_PIN GPIO_NUM_1
#define MISO_PIN GPIO_NUM_6
#define SCK_PIN  GPIO_NUM_7

namespace {
constexpr char tag[] = "Nrf24Transport";
constexpr uint8_t CONFIG = 0x00;
constexpr uint8_t STATUS = 0x07;
constexpr uint8_t FIFO_STATUS = 0x17;
constexpr uint8_t irq_flags = 0x70;
}

Nrf24Transport::~Nrf24Transport() {
    if (spi_device_ != nullptr) {
        (void)gpio_set_level(CE_PIN, 0);
        (void)spi_bus_remove_device(spi_device_);
        (void)spi_bus_free(SPI2_HOST);
    }
}

esp_err_t Nrf24Transport::readRegister(uint8_t reg, uint8_t* data, std::size_t length) {
    constexpr std::size_t max_register_bytes = 5;
    if (data == nullptr || length == 0 || length > max_register_bytes || reg > 0x1F) {
        return ESP_ERR_INVALID_ARG;
    }
    if (spi_device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t request[1 + max_register_bytes]{};
    uint8_t response[1 + max_register_bytes]{};
    request[0] = reg; // R_REGISTER (0x00) | register address.
    std::memset(request + 1, 0xFF, length); // Dummy bytes clock out register data.

    spi_transaction_t transaction{};
    transaction.length = (1 + length) * 8;
    transaction.rxlength = transaction.length;
    transaction.tx_buffer = request;
    transaction.rx_buffer = response;

    const esp_err_t result = spi_device_transmit(spi_device_, &transaction);
    esp_rom_delay_us(4); // Conservative CSN-high spacing between commands.
    if (result != ESP_OK) {
        return result;
    }

    // response[0] is STATUS; the remaining bytes contain the register value.
    std::memcpy(data, response + 1, length);
    return ESP_OK;
}

esp_err_t Nrf24Transport::writeRegister(uint8_t reg, const uint8_t* data,
                                     std::size_t length) {
    if (data == nullptr || length == 0 || length > 5 || reg > 0x1F) {
        return ESP_ERR_INVALID_ARG;
    }
    if (spi_device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t request[6]{};
    request[0] = 0x20 | reg;
    std::memcpy(request + 1, data, length);
    spi_transaction_t transaction{};
    transaction.length = (1 + length) * 8;
    transaction.tx_buffer = request;
    const esp_err_t result = spi_device_transmit(spi_device_, &transaction);
    esp_rom_delay_us(4);
    return result;
}

bool Nrf24Transport::writeAndVerify(uint8_t reg, const uint8_t* data,
                                   std::size_t length) {
    uint8_t actual[5]{};
    if (writeRegister(reg, data, length) != ESP_OK ||
        readRegister(reg, actual, length) != ESP_OK) {
        ESP_LOGW(tag, "Register 0x%02x SPI transfer failed", reg);
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        if (actual[i] != data[i]) {
            ESP_LOGW(tag, "Register 0x%02x byte %u: expected 0x%02x, read 0x%02x",
                     reg, static_cast<unsigned>(i), data[i], actual[i]);
            return false;
        }
    }
    return true;
}

esp_err_t Nrf24Transport::sendCommand(uint8_t command) {
    if (spi_device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    spi_transaction_t transaction{};
    transaction.length = 8;
    transaction.tx_buffer = &command;
    const esp_err_t result = spi_device_transmit(spi_device_, &transaction);
    esp_rom_delay_us(4);
    return result;
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
            ESP_LOGW(tag, "GPIO setup failed");
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
            ESP_LOGW(tag, "SPI bus initialization failed");
            return TransportStatus::InitializationFailed;
        }

        spi_device_interface_config_t interface_config{};
        interface_config.clock_speed_hz = 1000000; // 1 MHz
        interface_config.mode = 0;                // SPI mode 0
        interface_config.spics_io_num = CSN_PIN;
        interface_config.queue_size = 1;

        if (spi_bus_add_device(SPI2_HOST, &interface_config, &spi_device_) != ESP_OK) {
            (void)spi_bus_free(SPI2_HOST);
            ESP_LOGW(tag, "SPI device registration failed");
            return TransportStatus::InitializationFailed;
        }
    }

    // Reassert standby on every attempt, including retries after partial setup.
    if (gpio_set_level(CE_PIN, 0) != ESP_OK) {
        ESP_LOGW(tag, "CE standby level failed");
        return TransportStatus::InitializationFailed;
    }
    // Allow the specified 100 ms supply ramp plus 10.3 ms power-on reset.
    vTaskDelay(pdMS_TO_TICKS(120) + 1);

    // Power down first so a warm MCU restart can safely replace old radio settings.
    constexpr uint8_t crc_config = 0x0C; // EN_CRC | CRCO; PRIM_RX and PWR_UP clear.
    if (!writeAndVerify(CONFIG, &crc_config, 1)) {
        return TransportStatus::InitializationFailed;
    }

    struct RegisterSetting { uint8_t reg; uint8_t value; };
    constexpr RegisterSetting settings[] = {
        {0x05, 76},   // RF_CH: channel 76.
        {0x03, 0x03}, // SETUP_AW: five-byte addresses.
        {0x02, 0x01}, // EN_RXADDR: pipe 0 only.
        {0x01, 0x01}, // EN_AA: auto-ack on pipe 0 only.
        {0x04, 0x3F}, // SETUP_RETR: 1000 us delay, at most 15 retransmissions.
        {0x06, 0x06}, // RF_SETUP: 1 Mbps, 0 dBm, test modes disabled.
        {0x1C, 0x00}, // DYNPD: static payloads on all pipes.
        {0x1D, 0x00}, // FEATURE: no dynamic payloads, ACK payloads or NO_ACK command.
        {0x11, sizeof(Nrf24Packet)}, // RX_PW_P0: exactly 32 bytes.
        {0x12, 0}, {0x13, 0}, {0x14, 0}, {0x15, 0}, {0x16, 0}, // Unused pipes.
    };
    for (const auto& setting : settings) {
        if (!writeAndVerify(setting.reg, &setting.value, 1)) {
            return TransportStatus::InitializationFailed;
        }
    }

    // SPI transfer order. PTX pipe 0 must match TX_ADDR to receive auto-acks.
    constexpr uint8_t address[] = {0xD2, 0xF0, 0xF0, 0xF0, 0xF0};
    if (!writeAndVerify(0x10, address, sizeof(address)) ||
        !writeAndVerify(0x0A, address, sizeof(address))) {
        return TransportStatus::InitializationFailed;
    }

    // CE is low and the radio is powered down: no active packet can be interrupted.
    if (sendCommand(0xE1) != ESP_OK || sendCommand(0xE2) != ESP_OK) {
        ESP_LOGW(tag, "FIFO flush command failed");
        return TransportStatus::InitializationFailed;
    }
    // STATUS IRQ bits are write-one-to-clear, not ordinary writable bits.
    if (writeRegister(STATUS, &irq_flags, 1) != ESP_OK) {
        ESP_LOGW(tag, "IRQ clear SPI transfer failed");
        return TransportStatus::InitializationFailed;
    }
    uint8_t status{};
    uint8_t fifo_status{};
    if (readRegister(STATUS, &status, 1) != ESP_OK || status != 0x0E ||
        readRegister(FIFO_STATUS, &fifo_status, 1) != ESP_OK || fifo_status != 0x11) {
        ESP_LOGW(tag, "Startup state mismatch: STATUS=0x%02x FIFO_STATUS=0x%02x",
                 status, fifo_status);
        return TransportStatus::InitializationFailed;
    }

    constexpr uint8_t standby_config = crc_config | 0x02; // PWR_UP; CE stays low.
    if (!writeAndVerify(CONFIG, &standby_config, 1)) {
        return TransportStatus::InitializationFailed;
    }
    esp_rom_delay_us(1500); // Power-down to standby-I oscillator startup.

    ESP_LOGI(tag, "Register and FIFO checks passed; CE low, powered standby");
    // A configured radio is not yet a usable TransportInterface implementation.
    ESP_LOGW(tag, "Transport unavailable: packet encoding and RX/TX are unfinished");
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
