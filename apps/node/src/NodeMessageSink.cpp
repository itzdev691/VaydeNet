#include "NodeMessageSink.h"

#include "esp_log.h"

namespace {

constexpr char kLogTag[] = "NodeMessageSink";

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

    ESP_LOGI(
        kLogTag,
        "Logical message delivered: "
        "version=%u type=%u length=%u source=%llu sequence=%lu",
        static_cast<unsigned>(message.protocol_version),
        static_cast<unsigned>(message.type),
        static_cast<unsigned>(message.payload_length),
        static_cast<unsigned long long>(message.source_node_id),
        static_cast<unsigned long>(message.sequence_number)
    );

    return MessageSinkStatus::Delivered;
}
