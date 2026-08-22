#include "bootstrap/NodeBootstrap.h"

void NodeBootstrap::run() {
    // Startup dependencies will be assembled here.
    const BoardInfoStatus status =
    retrieveBoardInformation(board_information_);

    if (status != BoardInfoStatus::Ok) {
        return;
    }
}
