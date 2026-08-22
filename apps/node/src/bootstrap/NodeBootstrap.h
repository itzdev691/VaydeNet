#pragma once

#include "Esp32BoardInfo.h"

class NodeBootstrap {
public:
    void run();


private:
    BoardInformation board_information_{};
};
