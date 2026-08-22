#include "bootstrap/NodeBootstrap.h"

extern "C" void app_main() {
    static NodeBootstrap bootstrap;
    bootstrap.run();
}
