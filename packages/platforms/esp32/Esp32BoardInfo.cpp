#include "Esp32BoardInfo.h"
#include "esp_err.h"
#include "esp_mac.h"

#ifndef VAYDENET_BOARD_MODEL
#error "VAYDENET_BOARD_MODEL must be defined by board configuration"
#endif

BoardInfoStatus readDeviceUid(
    std::array<std::uint8_t, 6>& outputUid
) {
    const esp_err_t result =
        esp_efuse_mac_get_default(outputUid.data());

    if (result != ESP_OK) {
        return BoardInfoStatus::UidReadFailed;
    }

    return BoardInfoStatus::Ok;
}

const char* getBoardModel() {
    return VAYDENET_BOARD_MODEL;
}

BoardInfoStatus retrieveBoardInformation(
    BoardInformation& output
) {
    const BoardInfoStatus uidStatus = readDeviceUid(output.device_uid);

    if (uidStatus != BoardInfoStatus::Ok) {
        return uidStatus;
    }

    output.board_model = getBoardModel();

    if (output.board_model == nullptr || output.board_model[0] == '\0') {
        return BoardInfoStatus::MissingBoardModel;
    }

    return BoardInfoStatus::Ok;
}
