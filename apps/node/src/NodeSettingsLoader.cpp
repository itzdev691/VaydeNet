#include "NodeSettingsLoader.h"


NodeSettingsLoadStatus loadNodeSettings(
    const BoardInformation&,
    NodeSettings&
) {
    // Loading behaviour should be added here
    return NodeSettingsLoadStatus::NotConfigured;
}
