#include "EspHomeLogCompat.h"

// Compile the upstream implementation exactly once, with its logging macro.
// Including the snapshot here preserves its original bytes and license.
#include "../../third_party/esp-now-hosted/host/esp_now_hosted.cpp"
