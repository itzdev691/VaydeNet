#pragma once

#include <cstdint>

struct EngineStartupContext;
struct HardwareIdentity;
struct NodeSettings;
class TransportInterface;

enum class EngineStartStatus : std::uint8_t {
    Ok,
    StartupFailed
};

class VaydeEngine {
public:
    EngineStartStatus start(
        const EngineStartupContext& context
    );

private:
    const HardwareIdentity* identity_{nullptr};
    const NodeSettings* settings_{nullptr};
    TransportInterface* transport_{nullptr};
    bool started_{false};
};
