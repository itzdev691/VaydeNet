#include "NodeMessageSink.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "esp_log.h"

namespace {

constexpr char kLogTag[] = "NodeMessageSink";

bool isPrintableAscii(std::uint8_t byte) {
    return byte >= 0x20U && byte <= 0x7EU;
}

}  // namespace

MessageSinkStatus NodeMessageSink::deliver(
    const Message& message
) {
    if (
        message.payload_length >
        kMaximumMessagePayloadSize
    ) {
        ESP_LOGW(
            kLogTag,
            "Rejected logical message invalid length: %u",
            static_cast<unsigned>(message.payload_length)
        );

        return MessageSinkStatus::Rejected;
    }

    std::array<
        char,
        kMaximumMessagePayloadSize + 1
    > rendered_payload{};

    for (
        std::size_t index = 0;
        index < message.payload_length;
        ++index
    ) {
        const std::uint8_t byte =
            message.payload[index];

        rendered_payload[index] =
            isPrintableAscii(byte)
                ? static_cast<char>(byte)
                : '.';
    }

    rendered_payload[message.payload_length] = '\0';

    ESP_LOGI(
        kLogTag,
        "Logical message delivered: "
        "version=%u type=%u length=%u "
        "source=%llu sequence=%lu payload=\"%s\"",
        static_cast<unsigned>(message.protocol_version),
        static_cast<unsigned>(message.type),
        static_cast<unsigned>(message.payload_length),
        static_cast<unsigned long long>(
            message.source_node_id
        ),
        static_cast<unsigned long>(
            message.sequence_number
        ),
        rendered_payload.data()
    );

    return MessageSinkStatus::Delivered;
}