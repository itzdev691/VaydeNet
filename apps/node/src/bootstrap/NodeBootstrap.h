#pragma once

#include "Esp32BoardInfo.h"
#include "VaydeNet/config/NodeSettings.h"

class NodeBootstrap {
public:
    void run();


private:
    BoardInformation board_information_{};
    NodeSettings node_settings_{};
};
