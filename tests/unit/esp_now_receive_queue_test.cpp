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
esp_now_send_cb_t registered_send_callback = nullptr;
bool broadcast_peer_registered = false;
esp_err_t next_send_result = ESP_OK;
std::uint8_t last_send_address[ESP_NOW_ETH_ALEN]{};
std::vector<std::uint8_t> last_sent_data;
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

void completeSend(esp_now_send_status_t status) {
    expect(
        registered_send_callback != nullptr,
        "ESP-NOW send callback was not registered"
    );

    const esp_now_send_info_t send_info{};
    registered_send_callback(&send_info, status);
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
esp_err_t esp_now_unregister_recv_cb() {
    registered_receive_callback = nullptr;
    return ESP_OK;
}

esp_err_t esp_now_register_send_cb(esp_now_send_cb_t callback) {
    registered_send_callback = callback;
    return ESP_OK;
}

esp_err_t esp_now_unregister_send_cb() {
    registered_send_callback = nullptr;
    return ESP_OK;
}

esp_err_t esp_now_add_peer(const esp_now_peer_info_t* peer) {
    expect(peer != nullptr, "null ESP-NOW peer was added");

    const std::uint8_t expected_address[ESP_NOW_ETH_ALEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    expect(
        std::memcmp(
            peer->peer_addr,
            expected_address,
            sizeof(expected_address)
        ) == 0,
        "ESP-NOW peer was not the broadcast address"
    );
    expect(peer->channel == 1, "ESP-NOW peer used the wrong channel");
    expect(peer->ifidx == WIFI_IF_STA, "ESP-NOW peer used the wrong interface");
    expect(!peer->encrypt, "ESP-NOW broadcast peer enabled encryption");

    broadcast_peer_registered = true;
    return ESP_OK;
}

esp_err_t esp_now_del_peer(const std::uint8_t* peer_address) {
    expect(peer_address != nullptr, "null ESP-NOW peer was deleted");
    broadcast_peer_registered = false;
    return ESP_OK;
}

esp_err_t esp_now_send(
    const std::uint8_t* peer_address,
    const std::uint8_t* data,
    std::size_t data_length
) {
    expect(peer_address != nullptr, "ESP-NOW send omitted its destination");
    expect(data != nullptr, "ESP-NOW send omitted its packet data");

    std::memcpy(
        last_send_address,
        peer_address,
        sizeof(last_send_address)
    );
    last_sent_data.assign(data, data + data_length);
    return next_send_result;
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
    const Packet transmit_packet = makePacket(0);

    expect(
        transport.tryTransmit(transmit_packet) ==
            TransportTransmitStatus::NotInitialized,
        "uninitialized transport accepted transmission"
    );
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::NotInitialized,
        "uninitialized transport exposed send completion"
    );

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
        broadcast_peer_registered,
        "ESP-NOW broadcast peer was not registered"
    );
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::Empty,
        "new transmit completion queue was not empty"
    );
    expect(
        transport.tryReceive(received_packet) ==
            TransportReceiveStatus::Empty,
        "new receive queue was not empty"
    );

    expect(
        transport.tryTransmit(transmit_packet) ==
            TransportTransmitStatus::Queued,
        "valid ESP-NOW packet was not queued for transmission"
    );
    expect(
        last_sent_data.size() == sizeof(transmit_packet) &&
            std::memcmp(
                last_sent_data.data(),
                &transmit_packet,
                sizeof(transmit_packet)
            ) == 0,
        "ESP-NOW send did not receive the complete packet"
    );
    const std::uint8_t expected_broadcast_address[ESP_NOW_ETH_ALEN] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };
    expect(
        std::memcmp(
            last_send_address,
            expected_broadcast_address,
            sizeof(expected_broadcast_address)
        ) == 0,
        "ESP-NOW packet was not sent to the broadcast address"
    );
    expect(
        transport.tryTransmit(transmit_packet) ==
            TransportTransmitStatus::Busy,
        "second transmission was accepted while one was pending"
    );
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::Pending,
        "pending transmission was not reported"
    );

    completeSend(ESP_NOW_SEND_SUCCESS);
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::Sent,
        "successful send callback was not reported"
    );
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::Empty,
        "consumed completion remained queued"
    );

    expect(
        transport.tryTransmit(transmit_packet) ==
            TransportTransmitStatus::Queued,
        "transmission was not accepted after completion"
    );
    completeSend(ESP_NOW_SEND_FAIL);
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::Failed,
        "failed send callback was not reported"
    );

    next_send_result = ESP_FAIL;
    expect(
        transport.tryTransmit(transmit_packet) ==
            TransportTransmitStatus::Failed,
        "immediate ESP-NOW send failure was not reported"
    );
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::Empty,
        "failed submission left a transmission pending"
    );
    next_send_result = ESP_OK;

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

    transport.shutdown();
    expect(
        registered_receive_callback == nullptr &&
            registered_send_callback == nullptr,
        "ESP-NOW callbacks remained registered after shutdown"
    );
    expect(
        !broadcast_peer_registered,
        "ESP-NOW broadcast peer remained registered after shutdown"
    );
    expect(
        transport.pollTransmitCompletion() ==
            TransportTransmitCompletionStatus::NotInitialized,
        "shutdown transport still exposed completion state"
    );

    std::puts("PASS: ESP-NOW receive and transmit queues");
    return EXIT_SUCCESS;
}
