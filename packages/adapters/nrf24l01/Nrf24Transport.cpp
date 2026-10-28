#include "Nrf24Transport.h"
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
constexpr uint8_t rx_dr = 0x40;
constexpr uint8_t tx_flags = 0x30; // TX_DS | MAX_RT.
constexpr uint8_t max_rt = 0x10;
constexpr uint8_t rx_empty = 0x01;
constexpr uint8_t tx_empty = 0x10;
constexpr uint8_t tx_full = 0x20;
constexpr uint8_t R_RX_PAYLOAD = 0x61;
constexpr uint8_t W_TX_PAYLOAD = 0xA0;
constexpr uint8_t FLUSH_TX = 0xE1;
constexpr uint8_t FLUSH_RX = 0xE2;
constexpr uint8_t crc_config = 0x0C;                   // EN_CRC | CRCO.
constexpr uint8_t standby_config = crc_config | 0x02;  // PWR_UP; PRIM_RX clear.
constexpr uint8_t receive_config = standby_config | 0x01; // PRIM_RX.
constexpr std::size_t rx_fifo_depth = 3;
// Eight frames normally finish within tens of milliseconds; this bounds a wedged radio.
constexpr TickType_t transmit_timeout = pdMS_TO_TICKS(500);
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

esp_err_t Nrf24Transport::writePayload(const Nrf24Frame& frame) {
    if (spi_device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t request[1 + sizeof(Nrf24Frame)]{};
    request[0] = W_TX_PAYLOAD;
    std::memcpy(request + 1, &frame, sizeof(frame));
    spi_transaction_t transaction{};
    transaction.length = sizeof(request) * 8;
    transaction.tx_buffer = request;
    const esp_err_t result = spi_device_transmit(spi_device_, &transaction);
    esp_rom_delay_us(4);
    return result;
}

esp_err_t Nrf24Transport::readPayload(Nrf24Frame& frame) {
    if (spi_device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t request[1 + sizeof(Nrf24Frame)];
    uint8_t response[1 + sizeof(Nrf24Frame)]{};
    std::memset(request, 0xFF, sizeof(request));
    request[0] = R_RX_PAYLOAD;
    spi_transaction_t transaction{};
    transaction.length = sizeof(request) * 8;
    transaction.rxlength = transaction.length;
    transaction.tx_buffer = request;
    transaction.rx_buffer = response;
    const esp_err_t result = spi_device_transmit(spi_device_, &transaction);
    esp_rom_delay_us(4);
    if (result == ESP_OK) {
        std::memcpy(&frame, response + 1, sizeof(frame));
    }
    return result;
}

bool Nrf24Transport::startListening() {
    // Configuration changes happen with CE low; CE high then enters RX mode.
    if (gpio_set_level(CE_PIN, 0) != ESP_OK ||
        !writeAndVerify(CONFIG, &receive_config, 1) ||
        gpio_set_level(CE_PIN, 1) != ESP_OK) {
        ESP_LOGW(tag, "Receive mode entry failed");
        return false;
    }
    return true;
}

TransportStatus Nrf24Transport::initialize() {
    initialized_ = false;
    transmit_pending_ = false;
    reassembler_.reset();

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
        {0x11, sizeof(Nrf24Frame)}, // RX_PW_P0: exactly 32 bytes.
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
    if (sendCommand(FLUSH_TX) != ESP_OK || sendCommand(FLUSH_RX) != ESP_OK) {
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

    // PWR_UP with CE low enters standby-I.
    if (!writeAndVerify(CONFIG, &standby_config, 1)) {
        return TransportStatus::InitializationFailed;
    }
    esp_rom_delay_us(1500); // Power-down to standby-I oscillator startup.

    ESP_LOGI(tag, "Register and FIFO checks passed; CE low, powered standby");
    if (!startListening()) {
        return TransportStatus::InitializationFailed;
    }
    initialized_ = true;
    ESP_LOGI(tag, "Receive mode active; transport ready");
    return TransportStatus::Ok;
}

TransportReceiveStatus Nrf24Transport::tryReceive(Packet& packet) {
    if (!initialized_) {
        return TransportReceiveStatus::NotInitialized;
    }
    // Drain at most one FIFO's worth per poll so callers keep a bounded loop.
    for (std::size_t i = 0; i < rx_fifo_depth; ++i) {
        uint8_t fifo_status{};
        if (readRegister(FIFO_STATUS, &fifo_status, 1) != ESP_OK) {
            ESP_LOGW(tag, "Receive FIFO status read failed");
            return TransportReceiveStatus::Empty;
        }
        if ((fifo_status & rx_empty) != 0) {
            return TransportReceiveStatus::Empty;
        }
        Nrf24Frame frame{};
        if (readPayload(frame) != ESP_OK || writeRegister(STATUS, &rx_dr, 1) != ESP_OK) {
            ESP_LOGW(tag, "Receive payload SPI transfer failed");
            reassembler_.reset();
            return TransportReceiveStatus::Empty;
        }
        if (reassembler_.accept(frame, packet) == Nrf24ReassemblyStatus::Complete) {
            return TransportReceiveStatus::Received;
        }
    }
    return TransportReceiveStatus::Empty;
}

bool Nrf24Transport::loadTransmitFrames() {
    while (frames_loaded_ < kNrf24FramesPerPacket) {
        uint8_t fifo_status{};
        if (readRegister(FIFO_STATUS, &fifo_status, 1) != ESP_OK) {
            return false;
        }
        if ((fifo_status & tx_full) != 0) {
            return true;
        }
        Nrf24Frame frame{};
        encodeNrf24Frame(transmit_packet_, transfer_id_, frames_loaded_, frame);
        if (writePayload(frame) != ESP_OK) {
            return false;
        }
        ++frames_loaded_;
    }
    return true;
}

TransportTransmitStatus Nrf24Transport::tryTransmit(const Packet& packet) {
    if (!initialized_) {
        return TransportTransmitStatus::NotInitialized;
    }
    if (transmit_pending_) {
        return TransportTransmitStatus::Busy;
    }

    transmit_packet_ = packet;
    ++transfer_id_;
    frames_loaded_ = 0;
    // CE low leaves RX; with PRIM_RX clear, CE high sends the TX FIFO in order.
    if (gpio_set_level(CE_PIN, 0) != ESP_OK ||
        !writeAndVerify(CONFIG, &standby_config, 1) ||
        writeRegister(STATUS, &tx_flags, 1) != ESP_OK ||
        !loadTransmitFrames() ||
        gpio_set_level(CE_PIN, 1) != ESP_OK) {
        ESP_LOGW(tag, "Transmit submission failed");
        (void)finishTransmit(TransportTransmitCompletionStatus::Failed);
        return TransportTransmitStatus::Failed;
    }

    transmit_pending_ = true;
    transmit_started_ = xTaskGetTickCount();
    return TransportTransmitStatus::Queued;
}

TransportTransmitCompletionStatus Nrf24Transport::pollTransmitCompletion() {
    if (!initialized_) {
        return TransportTransmitCompletionStatus::NotInitialized;
    }
    if (!transmit_pending_) {
        return TransportTransmitCompletionStatus::Empty;
    }

    uint8_t status{};
    if (readRegister(STATUS, &status, 1) != ESP_OK) {
        ESP_LOGW(tag, "Transmit STATUS read failed");
        return finishTransmit(TransportTransmitCompletionStatus::Failed);
    }
    if ((status & max_rt) != 0) {
        ESP_LOGW(tag, "Frame %u of %u was not acknowledged",
                 static_cast<unsigned>(frames_loaded_),
                 static_cast<unsigned>(kNrf24FramesPerPacket));
        return finishTransmit(TransportTransmitCompletionStatus::Failed);
    }
    if (!loadTransmitFrames()) {
        ESP_LOGW(tag, "Transmit FIFO refill failed");
        return finishTransmit(TransportTransmitCompletionStatus::Failed);
    }
    // A payload leaves the TX FIFO only after its auto-ack arrives.
    uint8_t fifo_status{};
    if (readRegister(FIFO_STATUS, &fifo_status, 1) != ESP_OK) {
        ESP_LOGW(tag, "Transmit FIFO status read failed");
        return finishTransmit(TransportTransmitCompletionStatus::Failed);
    }
    if (frames_loaded_ == kNrf24FramesPerPacket && (fifo_status & tx_empty) != 0) {
        return finishTransmit(TransportTransmitCompletionStatus::Sent);
    }
    if (static_cast<TickType_t>(xTaskGetTickCount() - transmit_started_) >=
        transmit_timeout) {
        ESP_LOGW(tag, "Transmit timed out");
        return finishTransmit(TransportTransmitCompletionStatus::Failed);
    }
    return TransportTransmitCompletionStatus::Pending;
}

TransportTransmitCompletionStatus Nrf24Transport::finishTransmit(
    TransportTransmitCompletionStatus result
) {
    transmit_pending_ = false;
    if (gpio_set_level(CE_PIN, 0) != ESP_OK ||
        sendCommand(FLUSH_TX) != ESP_OK ||
        writeRegister(STATUS, &tx_flags, 1) != ESP_OK ||
        !startListening()) {
        // Radio mode is unknown; callers must reinitialize before further use.
        initialized_ = false;
        ESP_LOGW(tag, "Radio not restored to receive mode; reinitialize required");
    }
    return result;
}
