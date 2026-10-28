#include "Nrf24Transport.h"
#include "Nrf24Frame.h"
#include "driver/gpio.h"
#include "freertos/task.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

namespace {
struct Radio {
    std::array<std::array<uint8_t, 5>, 32> registers{};
    std::vector<uint8_t> commands;
    std::deque<Nrf24Frame> tx_fifo, rx_fifo;
    std::vector<Nrf24Frame> air;
    int fail_transfer{}, corrupt_read{}, transfers{}, reads{};
    int gpio_calls{}, fail_gpio{}, bus_inits{}, adds{}, removes{}, frees{};
    bool fail_bus{}, fail_add{}, owned{}, registered{}, passed{}, ready{}, por_waited{};
    int forced_read{-1};
    unsigned ce{}, oscillator_waits{};
    TickType_t ticks{};
    Radio() {
        // A warm radio with old IRQs, full/reused FIFOs and dynamic payloads enabled.
        for (auto& reg : registers) reg.fill(0xFF);
        registers[0x07][0] = 0x7F;
        registers[0x17][0] = 0x63;
    }
} radio;
void reset() { radio = Radio{}; }

// Derive FIFO_STATUS and STATUS RX_P_NO/TX_FULL from the modeled FIFOs.
void sync() {
    auto& fifo = radio.registers[0x17][0];
    fifo = (radio.tx_fifo.empty() ? 0x10 : 0) | (radio.tx_fifo.size() == 3 ? 0x20 : 0) |
           (radio.rx_fifo.size() == 3 ? 0x02 : 0) | (radio.rx_fifo.empty() ? 0x01 : 0);
    auto& status = radio.registers[0x07][0];
    status = (status & 0x70) | (radio.rx_fifo.empty() ? 0x0E : 0x00) |
             (radio.tx_fifo.size() == 3 ? 0x01 : 0);
}
bool transmitting() { return radio.registers[0][0] == 0x0E && radio.ce == 1; }
bool listening() { return radio.registers[0][0] == 0x0F && radio.ce == 1; }

// Over-the-air step: acknowledged frames leave the TX FIFO; a lost ack sets MAX_RT.
void transmit(bool acknowledged = true) {
    assert(transmitting());
    if (radio.tx_fifo.empty()) return;
    if (!acknowledged) { radio.registers[7][0] |= 0x10; return; }
    while (!radio.tx_fifo.empty()) {
        radio.air.push_back(radio.tx_fifo.front());
        radio.tx_fifo.pop_front();
    }
    radio.registers[7][0] |= 0x20;
    sync();
}
void receive(const Nrf24Frame& frame) {
    assert(listening() && radio.rx_fifo.size() < 3);
    radio.rx_fifo.push_back(frame);
    radio.registers[7][0] |= 0x40;
    sync();
}
void unavailable(Nrf24Transport& transport) {
    Packet packet{};
    assert(transport.tryReceive(packet) == TransportReceiveStatus::NotInitialized);
    assert(transport.tryTransmit(packet) == TransportTransmitStatus::NotInitialized);
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::NotInitialized);
}
Packet patterned(uint8_t seed) {
    Packet packet{};
    auto* bytes = reinterpret_cast<uint8_t*>(&packet);
    for (std::size_t i = 0; i < sizeof(packet); ++i) bytes[i] = static_cast<uint8_t>(seed ^ i);
    return packet;
}
bool same(const Packet& a, const Packet& b) { return std::memcmp(&a, &b, sizeof(Packet)) == 0; }
// Drives one submitted Packet to completion, delivering each loaded FIFO batch.
TransportTransmitCompletionStatus complete(Nrf24Transport& transport) {
    for (int i = 0; i < 16; ++i) {
        if (transmitting()) transmit();
        const auto result = transport.pollTransmitCompletion();
        if (result != TransportTransmitCompletionStatus::Pending) return result;
    }
    assert(false && "transmission did not finish");
    return TransportTransmitCompletionStatus::Failed;
}
}

void testEspLog(const char*, const char* format, ...) {
    if (std::strstr(format, "Register and FIFO checks passed")) radio.passed = true;
    if (std::strstr(format, "transport ready")) radio.ready = true;
}
esp_err_t gpio_set_direction(gpio_num_t, gpio_mode_t) {
    return ++radio.gpio_calls == radio.fail_gpio ? ESP_FAIL : ESP_OK;
}
esp_err_t gpio_set_level(gpio_num_t pin, unsigned level) {
    if (++radio.gpio_calls == radio.fail_gpio) return ESP_FAIL;
    if (pin == GPIO_NUM_5) radio.ce = level;
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
TickType_t xTaskGetTickCount() { return radio.ticks; }
void esp_rom_delay_us(std::uint32_t us) {
    assert(us == 4 || us == 1500);
    if (us == 1500) {
        assert(radio.registers[0][0] == 0x0E && radio.ce == 0);
        ++radio.oscillator_waits;
    }
}
esp_err_t spi_device_transmit(spi_device_handle_t device, spi_transaction_t* t) {
    assert(device == &radio && radio.registered && radio.por_waited);
    const auto* tx = static_cast<const uint8_t*>(t->tx_buffer);
    assert(tx && t->length % 8 == 0);
    radio.commands.push_back(tx[0]);
    if (++radio.transfers == radio.fail_transfer) return ESP_FAIL;
    const auto size = t->length / 8;
    if (tx[0] == 0xE1) {
        assert(size == 1); radio.tx_fifo.clear(); sync();
    } else if (tx[0] == 0xE2) {
        assert(size == 1); radio.rx_fifo.clear(); sync();
    } else if (tx[0] == 0xA0) {
        assert(size == 33 && radio.tx_fifo.size() < 3);
        Nrf24Frame frame{}; std::memcpy(&frame, tx + 1, sizeof(frame));
        radio.tx_fifo.push_back(frame); sync();
    } else if (tx[0] == 0x61) {
        assert(size == 33 && t->rxlength == t->length && !radio.rx_fifo.empty());
        auto* rx = static_cast<uint8_t*>(t->rx_buffer);
        assert(rx); rx[0] = radio.registers[7][0];
        std::memcpy(rx + 1, &radio.rx_fifo.front(), sizeof(Nrf24Frame));
        radio.rx_fifo.pop_front(); sync();
    } else if ((tx[0] & 0xE0) == 0x20) {
        assert(size >= 2 && size <= 6);
        const auto reg = tx[0] & 0x1F;
        // Only write-one-to-clear STATUS may change while CE is high.
        if (reg == 7) radio.registers[reg][0] &= ~(tx[1] & 0x70);
        else { assert(radio.ce == 0); std::memcpy(radio.registers[reg].data(), tx + 1, size - 1); }
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

namespace {
void testStartup() {
    reset();
    int transfers{}, reads{}, gpio_calls{};
    {
        Nrf24Transport transport;
        unavailable(transport);
        assert(transport.initialize() == TransportStatus::Ok);
        assert(radio.passed && radio.ready && radio.oscillator_waits == 1);
        const auto& r = radio.registers;
        assert(r[1][0] == 1 && r[2][0] == 1);
        assert(r[3][0] == 3 && r[4][0] == 0x3F && r[5][0] == 76 && r[6][0] == 6);
        assert(r[0x11][0] == 32 && r[0x1C][0] == 0 && r[0x1D][0] == 0);
        for (int reg = 0x12; reg <= 0x16; ++reg) assert(r[reg][0] == 0);
        const std::array<uint8_t,5> address{0xD2,0xF0,0xF0,0xF0,0xF0};
        assert(r[0x10] == address && r[0x0A] == address);
        assert(r[7][0] == 0x0E && r[0x17][0] == 0x11 && listening());
        transfers = radio.transfers; reads = radio.reads; gpio_calls = radio.gpio_calls;
        Packet packet{};
        assert(transport.tryReceive(packet) == TransportReceiveStatus::Empty);
        assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Empty);
        // Re-initialization from receive mode reuses the SPI device.
        assert(transport.initialize() == TransportStatus::Ok);
        assert(radio.bus_inits == 1 && radio.adds == 1 && radio.oscillator_waits == 2);
    }
    assert(radio.removes == 1 && radio.frees == 1 && radio.ce == 0);
    // Every SPI error must stop startup; a later explicit retry must recover.
    for (int fault = 1; fault <= transfers; ++fault) {
        reset(); radio.fail_transfer = fault;
        {
            Nrf24Transport transport;
            assert(transport.initialize() == TransportStatus::InitializationFailed);
            assert(!radio.ready && radio.transfers == fault);
            unavailable(transport);
            radio.fail_transfer = 0;
            assert(transport.initialize() == TransportStatus::Ok);
            assert(radio.ready && radio.bus_inits == 1 && radio.adds == 1);
        }
        assert(radio.removes == 1 && radio.frees == 1);
    }
    // Corrupt the last byte of each read, including both five-byte addresses.
    for (int fault = 1; fault <= reads; ++fault) {
        reset(); radio.corrupt_read = fault;
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::InitializationFailed);
        assert(!radio.ready && radio.reads == fault);
        unavailable(transport);
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
        assert(!radio.ready);
        unavailable(transport);
        radio.fail_gpio = 0;
        assert(transport.initialize() == TransportStatus::Ok);
        assert(radio.ready && radio.bus_inits == 1 && radio.adds == 1);
    }
    for (bool fail_add : {false, true}) {
        reset(); radio.fail_bus = !fail_add; radio.fail_add = fail_add;
        {
            Nrf24Transport transport;
            assert(transport.initialize() == TransportStatus::InitializationFailed);
            assert(!radio.passed && radio.transfers == 0);
            assert(radio.frees == (fail_add ? 1 : 0));
            radio.fail_bus = radio.fail_add = false;
            assert(transport.initialize() == TransportStatus::Ok);
            assert(radio.ready);
        }
        assert(radio.removes == 1 && radio.frees == (fail_add ? 2 : 1));
    }
    std::printf("PASS: nRF24 startup, %d SPI failures, %d read-back mismatches, GPIO/resource failures and retries\n",
                transfers, reads);
}

void testTransmitAndReceive() {
    reset();
    Nrf24Transport transport;
    assert(transport.initialize() == TransportStatus::Ok);
    const Packet sent = patterned(0x5A);

    // Submission fills the three-entry TX FIFO and starts PTX without waiting.
    assert(transport.tryTransmit(sent) == TransportTransmitStatus::Queued);
    assert(transmitting() && radio.tx_fifo.size() == 3);
    assert(transport.tryTransmit(sent) == TransportTransmitStatus::Busy);
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Pending);
    for (std::size_t expected : {6u, 8u}) {
        transmit();
        assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Pending);
        assert(radio.air.size() + radio.tx_fifo.size() == expected);
    }
    transmit();
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Sent);
    assert(listening() && radio.air.size() == kNrf24FramesPerPacket);
    assert((radio.registers[7][0] & 0x30) == 0);
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Empty);

    // Feeding the transmitted frames back reproduces the Packet byte for byte.
    Packet received{};
    std::size_t next = 0;
    while (next < kNrf24FramesPerPacket) {
        for (int i = 0; i < 3 && next < kNrf24FramesPerPacket; ++i) receive(radio.air[next++]);
        const auto status = transport.tryReceive(received);
        assert(status == (next == kNrf24FramesPerPacket ? TransportReceiveStatus::Received
                                                       : TransportReceiveStatus::Empty));
    }
    assert(same(received, sent) && radio.rx_fifo.empty());
    assert((radio.registers[7][0] & 0x40) == 0);
    assert(transport.tryReceive(received) == TransportReceiveStatus::Empty);

    // Each Packet uses a fresh transfer ID.
    const uint8_t first_id = radio.air[0].transfer_id;
    radio.air.clear();
    assert(transport.tryTransmit(patterned(1)) == TransportTransmitStatus::Queued);
    assert(complete(transport) == TransportTransmitCompletionStatus::Sent);
    assert(radio.air[0].transfer_id == static_cast<uint8_t>(first_id + 1));

    // Foreign/malformed frames are drained without producing a Packet.
    Nrf24Frame junk{};
    receive(junk); receive(junk);
    assert(transport.tryReceive(received) == TransportReceiveStatus::Empty);
    assert(radio.rx_fifo.empty());

    // A missing ack fails the Packet, flushes TX and resumes listening.
    assert(transport.tryTransmit(sent) == TransportTransmitStatus::Queued);
    transmit(false);
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Failed);
    assert(listening() && radio.tx_fifo.empty() && (radio.registers[7][0] & 0x30) == 0);

    // A radio that never completes is bounded by the transmit timeout.
    radio.ticks = 1000;
    assert(transport.tryTransmit(sent) == TransportTransmitStatus::Queued);
    radio.ticks += 499;
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Pending);
    radio.ticks += 1;
    assert(transport.pollTransmitCompletion() == TransportTransmitCompletionStatus::Failed);
    assert(listening() && radio.tx_fifo.empty());

    radio.air.clear();
    assert(transport.tryTransmit(sent) == TransportTransmitStatus::Queued);
    assert(complete(transport) == TransportTransmitCompletionStatus::Sent);
    std::printf("PASS: nRF24 eight-frame transmit, receive, ack failure and timeout\n");
}

void testTransferFaults() {
    // Count transfers in one full send cycle, then fail each one in turn.
    reset();
    int cycle{};
    {
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::Ok);
        const int before = radio.transfers;
        assert(transport.tryTransmit(patterned(2)) == TransportTransmitStatus::Queued);
        assert(complete(transport) == TransportTransmitCompletionStatus::Sent);
        cycle = radio.transfers - before;
    }
    for (int fault = 1; fault <= cycle; ++fault) {
        reset();
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::Ok);
        radio.fail_transfer = radio.transfers + fault;
        if (transport.tryTransmit(patterned(3)) == TransportTransmitStatus::Queued) {
            (void)complete(transport);
        }
        // Either the radio was restored to RX or the transport demands reinitialization.
        Packet probe{};
        if (transport.tryReceive(probe) == TransportReceiveStatus::NotInitialized) {
            unavailable(transport);
            assert(transport.initialize() == TransportStatus::Ok);
        }
        assert(listening() && radio.tx_fifo.empty());
        radio.air.clear();
        const Packet sent = patterned(4);
        assert(transport.tryTransmit(sent) == TransportTransmitStatus::Queued);
        assert(complete(transport) == TransportTransmitCompletionStatus::Sent);
        Nrf24Reassembler reassembler;
        Packet out{};
        Nrf24ReassemblyStatus status{};
        for (const auto& frame : radio.air) status = reassembler.accept(frame, out);
        assert(status == Nrf24ReassemblyStatus::Complete && same(out, sent));
    }
    // A failed receive transfer drops the partial Packet; the next one still arrives.
    for (int fault = 1; fault <= 3; ++fault) {
        reset();
        Nrf24Transport transport;
        assert(transport.initialize() == TransportStatus::Ok);
        const Packet sent = patterned(5);
        std::array<Nrf24Frame, kNrf24FramesPerPacket> frames{};
        for (uint8_t i = 0; i < kNrf24FramesPerPacket; ++i) encodeNrf24Frame(sent, 9, i, frames[i]);
        receive(frames[0]);
        radio.fail_transfer = radio.transfers + fault;
        Packet out{};
        assert(transport.tryReceive(out) == TransportReceiveStatus::Empty);
        radio.rx_fifo.clear(); sync();
        std::size_t next = 0;
        TransportReceiveStatus status{};
        while (next < kNrf24FramesPerPacket) {
            for (int i = 0; i < 3 && next < kNrf24FramesPerPacket; ++i) receive(frames[next++]);
            status = transport.tryReceive(out);
        }
        assert(status == TransportReceiveStatus::Received && same(out, sent));
    }
    std::printf("PASS: nRF24 %d transmit-cycle SPI failures and receive SPI failures\n", cycle);
}
}

int main() {
    testStartup();
    testTransmitAndReceive();
    testTransferFaults();
}
