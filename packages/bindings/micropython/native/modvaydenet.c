#include <string.h>
#include "py/runtime.h"
#include "bridge.h"

static mp_obj_t status_string(const char *value) {
    // Engine state may already have changed. Returning an interned status must
    // not allocate and leave a queued request stranded on MemoryError.
    static const struct {
        const char *name;
        qstr value;
    } statuses[] = {
        {"Queued", MP_QSTR_Queued},
        {"Busy", MP_QSTR_Busy},
        {"InvalidMessageType", MP_QSTR_InvalidMessageType},
        {"InvalidMessageTtl", MP_QSTR_InvalidMessageTtl},
        {"InvalidMessageLength", MP_QSTR_InvalidMessageLength},
        {"NotStarted", MP_QSTR_NotStarted},
        {"TransportNotReady", MP_QSTR_TransportNotReady},
        {"Unavailable", MP_QSTR_Unavailable},
        {"Failed", MP_QSTR_Failed},
        {"Sent", MP_QSTR_Sent},
        {"Pending", MP_QSTR_Pending},
        {"Empty", MP_QSTR_Empty},
        {"MessageDelivered", MP_QSTR_MessageDelivered},
        {"QueueEmpty", MP_QSTR_QueueEmpty},
        {"PacketRejected", MP_QSTR_PacketRejected},
        {"UnsupportedMessageType", MP_QSTR_UnsupportedMessageType},
        {"MessageRejected", MP_QSTR_MessageRejected},
        {"MessageSinkUnavailable", MP_QSTR_MessageSinkUnavailable},
    };
    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        if (strcmp(value, statuses[i].name) == 0) {
            return MP_OBJ_NEW_QSTR(statuses[i].value);
        }
    }
    return MP_OBJ_NEW_QSTR(MP_QSTR_Failed);
}

static mp_obj_t module_init(mp_obj_t mac) {
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(mac, &buffer, MP_BUFFER_READ);
    if (buffer.len != 6) mp_raise_ValueError(MP_ERROR_TEXT("MAC must contain 6 bytes"));
    return mp_obj_new_bool(vn_init(buffer.buf));
}
static MP_DEFINE_CONST_FUN_OBJ_1(init_obj, module_init);

static mp_obj_t module_stop(void) {
    vn_stop();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(stop_obj, module_stop);

static mp_obj_t module_send(size_t argc, const mp_obj_t *argv) {
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(argv[0], &buffer, MP_BUFFER_READ);
    mp_int_t ttl = argc > 1 ? mp_obj_get_int(argv[1]) : 1;
    mp_int_t flags = argc > 2 ? mp_obj_get_int(argv[2]) : 0;
    if (ttl < 1 || ttl > 255) mp_raise_ValueError(MP_ERROR_TEXT("TTL must be 1..255"));
    if (flags < 0 || flags > 255) mp_raise_ValueError(MP_ERROR_TEXT("flags must be 0..255"));
    return status_string(vn_send(buffer.buf, buffer.len, ttl, flags));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(send_obj, 1, 3, module_send);

static mp_obj_t module_take_tx(void) {
    uint8_t packet[220];
    if (!vn_take_tx(packet)) return mp_const_none;
    return mp_obj_new_bytes(packet, sizeof(packet));
}
static MP_DEFINE_CONST_FUN_OBJ_0(take_tx_obj, module_take_tx);

static mp_obj_t module_complete_tx(mp_obj_t success) {
    return mp_obj_new_bool(vn_complete_tx(mp_obj_is_true(success)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(complete_tx_obj, module_complete_tx);

static mp_obj_t module_poll_tx(void) {
    return status_string(vn_poll_tx());
}
static MP_DEFINE_CONST_FUN_OBJ_0(poll_tx_obj, module_poll_tx);

static mp_obj_t module_feed(mp_obj_t packet) {
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(packet, &buffer, MP_BUFFER_READ);
    if (buffer.len != 220) mp_raise_ValueError(MP_ERROR_TEXT("packet must contain 220 bytes"));
    return mp_obj_new_bool(vn_feed(buffer.buf));
}
static MP_DEFINE_CONST_FUN_OBJ_1(feed_obj, module_feed);

static mp_obj_t module_recv(void) {
    vn_message message;
    uint8_t validation;
    const char *status = vn_receive(&message, &validation);
    mp_obj_t result[3] = {status_string(status), MP_OBJ_NEW_SMALL_INT(validation), mp_const_none};
    if (strcmp(status, "MessageDelivered") == 0) {
        mp_obj_t dict = mp_obj_new_dict(7);
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_version), MP_OBJ_NEW_SMALL_INT(message.version));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_type), MP_OBJ_NEW_SMALL_INT(message.type));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_flags), MP_OBJ_NEW_SMALL_INT(message.flags));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_ttl), MP_OBJ_NEW_SMALL_INT(message.ttl));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_source), mp_obj_new_int_from_ull(message.source));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_sequence), mp_obj_new_int_from_uint(message.sequence));
        mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_payload), mp_obj_new_bytes(message.payload, message.length));
        result[2] = dict;
    }
    return mp_obj_new_tuple(3, result);
}
static MP_DEFINE_CONST_FUN_OBJ_0(recv_obj, module_recv);

static const mp_rom_map_elem_t module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__vaydenet)},
    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&init_obj)},
    {MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&stop_obj)},
    {MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&send_obj)},
    {MP_ROM_QSTR(MP_QSTR_take_tx), MP_ROM_PTR(&take_tx_obj)},
    {MP_ROM_QSTR(MP_QSTR_complete_tx), MP_ROM_PTR(&complete_tx_obj)},
    {MP_ROM_QSTR(MP_QSTR_poll_tx), MP_ROM_PTR(&poll_tx_obj)},
    {MP_ROM_QSTR(MP_QSTR_feed), MP_ROM_PTR(&feed_obj)},
    {MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&recv_obj)},
};
static MP_DEFINE_CONST_DICT(module_globals, module_globals_table);
const mp_obj_module_t vaydenet_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&module_globals,
};
MP_REGISTER_MODULE(MP_QSTR__vaydenet, vaydenet_module);
