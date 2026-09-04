#include "VaydeNet/VaydeEngine.h"
#include "VaydeNet/config/NodeSettings.h"
#include "VaydeNet/startup/HardwareIdentity.h"
#include "VaydeNet/startup/EngineStartupContext.h"

namespace {

constexpr std::uint16_t kSupportedProtocolVersion = 1;
constexpr std::uint16_t kSupportedSettingsVersion = 1;

}  // namespace

EngineStartStatus VaydeEngine::start(
    const EngineStartupContext& context
) {
    if (started_) {
        return EngineStartStatus::StartupFailed;
    }

    if (
        context.identity.board_model == nullptr ||
        context.identity.board_model[0] == '\0'
    ) {
        return EngineStartStatus::StartupFailed;
    }

    if (
        context.settings.protocol_version !=
            kSupportedProtocolVersion ||
        context.settings.settings_version !=
            kSupportedSettingsVersion
    ) {
        return EngineStartStatus::StartupFailed;
    }

    if (
        context.settings.transport ==
        TransportType::Unspecified
    ) {
        return EngineStartStatus::StartupFailed;
    }

    identity_ = &context.identity;
    settings_ = &context.settings;
    transport_ = &context.transport;
    started_ = true;

    return EngineStartStatus::Ok;
}
