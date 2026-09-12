#include "Esp32BoardInfo.h"
#include "esp_err.h"
#include "esp_mac.h"

#ifndef VAYDENET_BOARD_MODEL
#error "VAYDENET_BOARD_MODEL must be defined by board configuration"
#endif

Esp32BoardInfoStatus readEsp32DeviceUid(
    std::array<std::uint8_t, 6>& output_uid
) {
    const esp_err_t result =
        esp_efuse_mac_get_default(output_uid.data());

    if (result != ESP_OK) {
        return Esp32BoardInfoStatus::UidReadFailed;
    }

    return Esp32BoardInfoStatus::Ok;
}

const char* getEsp32BoardModel() {
    return VAYDENET_BOARD_MODEL;
}

Esp32BoardInfoStatus retrieveEsp32HardwareIdentity(
    HardwareIdentity& output
) {
    const Esp32BoardInfoStatus uid_status =
        readEsp32DeviceUid(output.device_uid);

    if (uid_status != Esp32BoardInfoStatus::Ok) {
        return uid_status;
    }

    output.board_model = getEsp32BoardModel();

    if (output.board_model == nullptr || output.board_model[0] == '\0') {
        return Esp32BoardInfoStatus::MissingBoardModel;
    }

    return Esp32BoardInfoStatus::Ok;
}
