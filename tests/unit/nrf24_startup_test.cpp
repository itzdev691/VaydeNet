#include "Nrf24Transport.h"
#include "Nrf24Packet.h"
#include "driver/gpio.h"
#include "freertos/task.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
struct Radio {
    std::array<std::array<uint8_t, 5>, 32> registers{};
    std::vector<uint8_t> commands;
    int fail_transfer{}, corrupt_read{}, transfers{}, reads{};
    int gpio_calls{}, fail_gpio{}, bus_inits{}, adds{}, removes{}, frees{};
    bool fail_bus{}, fail_add{}, owned{}, registered{}, passed{}, por_waited{};
    int forced_read{-1};
    unsigned ce{}, oscillator_waits{};
    Radio() {
        // A warm radio with old IRQs, full/reused FIFOs and dynamic payloads enabled.
        for (auto& reg : registers) reg.fill(0xFF);
        registers[0x07][0] = 0x7F;
        registers[0x17][0] = 0x63;
    }
} radio;
void reset() { radio = Radio{}; }
void unavailable(Nrf24Transport& transport) {
    Packet packet{};
    assert(transport.tryReceive(packet) == TransportReceiveStatus::NotInitialized);
    assert(transport.tryTransmit(packet) == TransportTransmitStatus::NotInitialized);
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::NotInitialized);
}
}

void testEspLog(const char*, const char* format, ...) {
    if (std::strstr(format, "Register and FIFO checks passed")) radio.passed = true;
}
esp_err_t gpio_set_direction(gpio_num_t, gpio_mode_t) {
    return ++radio.gpio_calls == radio.fail_gpio ? ESP_FAIL : ESP_OK;
}
esp_err_t gpio_set_level(gpio_num_t pin, unsigned level) {
    if (++radio.gpio_calls == radio.fail_gpio) return ESP_FAIL;
    if (pin == GPIO_NUM_5) { assert(level == 0); radio.ce = level; }
    return ESP_OK;
}
esp_err_t spi_bus_initialize(int host, const spi_bus_config_t* c, int dma) {
    ++radio.bus_inits;
    assert(host == SPI2_HOST && dma == SPI_DMA_DISABLED);
    assert(c->mosi_io_num == 1 && c->miso_io_num == 6 && c->sclk_io_num == 7);
    assert(c->quadwp_io_num == -1 && c->quadhd_io_num == -1);
    assert(c->data4_io_num == -1 && c->data5_io_num == -1);
    assert(c->data6_io_num == -1 && c->data7_io_num == -1);
    if (radio.fail_bus) return ESP_FAIL;
    assert(!radio.owned); radio.owned = true;
    return ESP_OK;
}
esp_err_t spi_bus_add_device(int, const spi_device_interface_config_t* c,
                             spi_device_handle_t* device) {
    ++radio.adds;
    assert(c->clock_speed_hz == 1000000 && c->mode == 0);
    assert(c->spics_io_num == 4 && c->queue_size == 1);
    if (radio.fail_add) return ESP_FAIL;
    *device = &radio; radio.registered = true;
    return ESP_OK;
}
esp_err_t spi_bus_remove_device(spi_device_handle_t device) {
    assert(device == &radio && radio.registered);
    radio.registered = false; ++radio.removes;
    return ESP_OK;
}
esp_err_t spi_bus_free(int) {
    assert(radio.owned && !radio.registered);
    radio.owned = false; ++radio.frees;
    return ESP_OK;
}
void vTaskDelay(TickType_t ticks) { assert(ticks >= 120); radio.por_waited = true; }
void esp_rom_delay_us(std::uint32_t us) {
    assert(us == 4 || us == 1500);
    if (us == 1500) {
        assert(radio.registers[0][0] == 0x0E);
        ++radio.oscillator_waits;
    }
}
esp_err_t spi_device_transmit(spi_device_handle_t device, spi_transaction_t* t) {
    assert(device == &radio && radio.registered && radio.por_waited && radio.ce == 0);
    const auto* tx = static_cast<const uint8_t*>(t->tx_buffer);
    assert(tx && t->length % 8 == 0);
    radio.commands.push_back(tx[0]);
    if (++radio.transfers == radio.fail_transfer) return ESP_FAIL;
    const auto size = t->length / 8;
    if (tx[0] == 0xE1) {
        assert(size == 1); radio.registers[0x17][0] &= ~0x60;
        radio.registers[0x17][0] |= 0x10; radio.registers[0x07][0] &= ~0x01;
    } else if (tx[0] == 0xE2) {
        assert(size == 1); radio.registers[0x17][0] &= ~0x02;
        radio.registers[0x17][0] |= 0x01;
        radio.registers[0x07][0] = (radio.registers[0x07][0] & ~0x0E) | 0x0E;
    } else if ((tx[0] & 0xE0) == 0x20) {
        assert(size >= 2 && size <= 6);
        const auto reg = tx[0] & 0x1F;
        if (reg == 7) radio.registers[reg][0] &= ~(tx[1] & 0x70);
        else std::memcpy(radio.registers[reg].data(), tx + 1, size - 1);
    } else {
        assert(tx[0] <= 0x1F && size >= 2 && size <= 6 && t->rxlength == t->length);
        for (std::size_t i = 1; i < size; ++i) assert(tx[i] == 0xFF);
        auto* rx = static_cast<uint8_t*>(t->rx_buffer);
        assert(rx); rx[0] = radio.registers[7][0];
        std::memcpy(rx + 1, radio.registers[tx[0]].data(), size - 1);
        if (radio.forced_read >= 0) std::memset(rx, radio.forced_read, size);
        if (++radio.reads == radio.corrupt_read) rx[size - 1] ^= 1;
    }
    return ESP_OK;
}

int main() {
    reset();
    int transfers{}, reads{}, gpio_calls{};
    {
        Nrf24Transport transport;
        unavailable(transport);
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(radio.passed && radio.oscillator_waits == 1);
        const auto& r = radio.registers;
        assert(r[0][0] == 0x0E && r[1][0] == 1 && r[2][0] == 1);
        assert(r[3][0] == 3 && r[4][0] == 0x3F && r[5][0] == 76 && r[6][0] == 6);
        assert(r[0x11][0] == 32 && r[0x1C][0] == 0 && r[0x1D][0] == 0);
        for (int reg = 0x12; reg <= 0x16; ++reg) assert(r[reg][0] == 0);
        const std::array<uint8_t,5> address{0xD2,0xF0,0xF0,0xF0,0xF0};
        assert(r[0x10] == address && r[0x0A] == address);
        assert(r[7][0] == 0x0E && r[0x17][0] == 0x11);
        transfers = radio.transfers; reads = radio.reads; gpio_calls = radio.gpio_calls;
        unavailable(transport);
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(radio.bus_inits == 1 && radio.adds == 1 && radio.oscillator_waits == 2);
    }
    assert(radio.removes == 1 && radio.frees == 1);
    // Every SPI error must stop startup; a later explicit retry must recover.
    for (int fault = 1; fault <= transfers; ++fault) {
        reset(); radio.fail_transfer = fault;
        {
            Nrf24Transport transport;
            assert(transport.initialize() == TransportStatus::InitializationFailed);
            assert(!radio.passed && radio.transfers == fault);
            unavailable(transport);
            radio.fail_transfer = 0;
            assert(transport.initialize() == TransportStatus::InitializationFailed);
            assert(radio.passed && radio.bus_inits == 1 && radio.adds == 1);
        }
        assert(radio.removes == 1 && radio.frees == 1);
    }
    // Corrupt the last byte of each read, including both five-byte addresses.
    for (int fault = 1; fault <= reads; ++fault) {
        reset(); radio.corrupt_read = fault;
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(!radio.passed && radio.reads == fault);
    }
    // Disconnected/stuck MISO must never produce a successful startup.
    for (int value : {0x00, 0xFF}) {
        reset(); radio.forced_read = value;
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(!radio.passed && radio.reads == 1);
    }
    for (int fault = 1; fault <= gpio_calls; ++fault) {
        reset(); radio.fail_gpio = fault;
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(!radio.passed && radio.transfers == 0);
        radio.fail_gpio = 0;
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(radio.passed && radio.bus_inits == 1 && radio.adds == 1);
    }
    for (bool fail_add : {false, true}) {
        reset(); radio.fail_bus = !fail_add; radio.fail_add = fail_add;
        {
            Nrf24Transport transport;
            assert(transport.initialize() == TransportStatus::InitializationFailed);
            assert(!radio.passed && radio.transfers == 0);
            assert(radio.frees == (fail_add ? 1 : 0));
            radio.fail_bus = radio.fail_add = false;
            assert(transport.initialize() == TransportStatus::InitializationFailed);
            assert(radio.passed);
        }
        assert(radio.removes == 1 && radio.frees == (fail_add ? 2 : 1));
    }
    std::printf("PASS: nRF24 startup, %d SPI failures, %d read-back mismatches, GPIO/resource failures and retries\n",
                transfers, reads);
}
