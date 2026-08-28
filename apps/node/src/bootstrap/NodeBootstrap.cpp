#include "bootstrap/NodeBootstrap.h"
#include "NodeSettingsLoader.h"

void NodeBootstrap::run() {
    // Startup dependencies will be assembled here.
    const BoardInfoStatus status =
    retrieveBoardInformation(board_information_);

    if (status != BoardInfoStatus::Ok) {
        return;
    }

    const NodeSettingsLoadStatus settings_status =
    loadNodeSettings(board_information_, node_settings_);

    switch (settings_status) {
        case NodeSettingsLoadStatus::Ok:
            break;
        case NodeSettingsLoadStatus::NotConfigured:
            return;

        case NodeSettingsLoadStatus::ReadFailed:
            return;
    }
}
