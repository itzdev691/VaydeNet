#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t version, type, flags, ttl;
    uint64_t source;
    uint32_t sequence;
    uint16_t length;
    uint8_t payload[200];
} vn_message;

bool vn_init(const uint8_t mac[6]);
void vn_stop(void);
const char *vn_send(const uint8_t *payload, size_t length, uint8_t ttl, uint8_t flags);
bool vn_take_tx(uint8_t packet[220]);
bool vn_complete_tx(bool success);
const char *vn_poll_tx(void);
bool vn_feed(const uint8_t packet[220]);
const char *vn_receive(vn_message *message, uint8_t *validation);

#ifdef __cplusplus
}
#endif
