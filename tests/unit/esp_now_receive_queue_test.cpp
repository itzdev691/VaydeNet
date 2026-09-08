#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

#include "EspNowTransport.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace {

struct FakeQueue {
    UBaseType_t capacity;
    UBaseType_t item_size;
    std::deque<std::vector<std::uint8_t>> items;
};

esp_now_recv_cb_t registered_receive_callback = nullptr;
int receive_activity_count = 0;

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, const char* message) {
    if (!condition) {
        fail(message);
    }
}

Packet makePacket(std::uint32_t sequence_number) {
    Packet packet{};
    packet.version = 1;
    packet.type = 2;
    packet.flags = 3;
    packet.ttl = 4;
    packet.length = sizeof(packet.payload);
    packet.senderID = 0x0102030405060708ULL;
    packet.sequenceNumber = sequence_number;
    std::memset(
        packet.payload,
        static_cast<int>(sequence_number),
        sizeof(packet.payload)
    );
    packet.crc = static_cast<std::uint16_t>(0xA000U + sequence_number);
    return packet;
}

void recordReceiveActivity(void*) {
    ++receive_activity_count;
}

void deliverFrame(const Packet& packet) {
    const std::uint8_t sender_address[] = {
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01
    };
    const esp_now_recv_info_t receive_info{sender_address};

    expect(
        registered_receive_callback != nullptr,
        "ESP-NOW receive callback was not registered"
    );
    registered_receive_callback(
        &receive_info,
        reinterpret_cast<const std::uint8_t*>(&packet),
        sizeof(packet)
    );
}

}  // namespace

void testEspLog(const char*, const char*, ...) {}

esp_err_t nvs_flash_init() {
    return ESP_OK;
}

esp_err_t esp_netif_init() {
    return ESP_OK;
}

esp_err_t esp_event_loop_create_default() {
    return ESP_OK;
}

esp_err_t esp_wifi_init(const wifi_init_config_t*) {
    return ESP_OK;
}

esp_err_t esp_wifi_set_storage(int) {
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(int) {
    return ESP_OK;
}

esp_err_t esp_wifi_start() {
    return ESP_OK;
}

esp_err_t esp_wifi_set_channel(std::uint8_t, int) {
    return ESP_OK;
}

esp_err_t esp_now_init() {
    return ESP_OK;
}

esp_err_t esp_now_deinit() {
    return ESP_OK;
}

esp_err_t esp_now_register_recv_cb(esp_now_recv_cb_t callback) {
    registered_receive_callback = callback;
    return ESP_OK;
}

QueueHandle_t xQueueCreate(
    UBaseType_t queue_length,
    UBaseType_t item_size
) {
    return new FakeQueue{queue_length, item_size, {}};
}

BaseType_t xQueueSend(
    QueueHandle_t queue,
    const void* item,
    TickType_t ticks_to_wait
) {
    expect(ticks_to_wait == 0, "receive callback attempted to block");

    auto* fake_queue = static_cast<FakeQueue*>(queue);
    if (fake_queue->items.size() >= fake_queue->capacity) {
        return pdFALSE;
    }

    const auto* item_bytes = static_cast<const std::uint8_t*>(item);
    fake_queue->items.emplace_back(
        item_bytes,
        item_bytes + fake_queue->item_size
    );
    return pdTRUE;
}

BaseType_t xQueueReceive(
    QueueHandle_t queue,
    void* item,
    TickType_t ticks_to_wait
) {
    expect(ticks_to_wait == 0, "tryReceive attempted to block");

    auto* fake_queue = static_cast<FakeQueue*>(queue);
    if (fake_queue->items.empty()) {
        return pdFALSE;
    }

    std::memcpy(
        item,
        fake_queue->items.front().data(),
        fake_queue->item_size
    );
    fake_queue->items.pop_front();
    return pdTRUE;
}

void vQueueDelete(QueueHandle_t queue) {
    delete static_cast<FakeQueue*>(queue);
}

int main() {
    EspNowTransport transport;
    Packet received_packet{};

    expect(
        transport.tryReceive(received_packet) ==
            TransportReceiveStatus::NotInitialized,
        "uninitialized transport did not reject dequeue"
    );
    expect(
        transport.configureChannel(1),
        "valid ESP-NOW channel was rejected"
    );

    transport.setReceiveActivityCallback(recordReceiveActivity, nullptr);
    expect(
        transport.initialize() == TransportStatus::Ok,
        "transport initialization failed"
    );
    expect(
        transport.tryReceive(received_packet) ==
            TransportReceiveStatus::Empty,
        "new receive queue was not empty"
    );

    Packet copied_packet = makePacket(1);
    const Packet original_packet = copied_packet;
    deliverFrame(copied_packet);
    std::memset(&copied_packet, 0, sizeof(copied_packet));

    expect(
        transport.tryReceive(received_packet) ==
            TransportReceiveStatus::Received,
        "exact-sized ESP-NOW frame was not queued"
    );
    expect(
        std::memcmp(
            &received_packet,
            &original_packet,
            sizeof(received_packet)
        ) == 0,
        "receive queue did not own a complete frame copy"
    );

    Packet invalid_packet = makePacket(2);
    const std::uint8_t sender_address[] = {
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01
    };
    const esp_now_recv_info_t receive_info{sender_address};
    registered_receive_callback(
        &receive_info,
        reinterpret_cast<const std::uint8_t*>(&invalid_packet),
        sizeof(invalid_packet) - 1
    );
    expect(
        transport.tryReceive(received_packet) ==
            TransportReceiveStatus::Empty,
        "unexpected-sized ESP-NOW frame entered the queue"
    );
    expect(
        receive_activity_count == 1,
        "unexpected-sized frame triggered receive activity"
    );

    for (std::uint32_t sequence_number = 10;
         sequence_number < 15;
         ++sequence_number) {
        deliverFrame(makePacket(sequence_number));
    }

    for (std::uint32_t expected_sequence = 10;
         expected_sequence < 14;
         ++expected_sequence) {
        expect(
            transport.tryReceive(received_packet) ==
                TransportReceiveStatus::Received,
            "queued ESP-NOW frame was missing"
        );
        expect(
            received_packet.sequenceNumber == expected_sequence,
            "receive queue did not preserve FIFO order"
        );
    }

    expect(
        transport.tryReceive(received_packet) ==
            TransportReceiveStatus::Empty,
        "full queue did not drop the fifth frame"
    );
    expect(
        receive_activity_count == 6,
        "receive activity count did not match exact-sized frames"
    );

    std::puts("PASS: ESP-NOW frames load into the bounded FreeRTOS queue");
    return EXIT_SUCCESS;
}
