#pragma once

#include <cstddef>
#include <cstdint>

#ifndef VAYDENET_DASHBOARD_PORT
#define VAYDENET_DASHBOARD_PORT 8080
#endif

namespace AppConfig {

static_assert(
    VAYDENET_DASHBOARD_PORT > 0 && VAYDENET_DASHBOARD_PORT <= 65535,
    "VAYDENET_DASHBOARD_PORT must be between 1 and 65535");

inline constexpr uint32_t kTransmitIntervalMs = 1000;
inline constexpr uint32_t kStatisticsIntervalMs = 10000;
inline constexpr size_t kEthernetHeaderSize = 14;
inline constexpr uint16_t kExperimentalEtherType = 0x88B5;
inline constexpr uint16_t kDashboardPort = VAYDENET_DASHBOARD_PORT;
inline constexpr char kHostname[] = "vaydenet-wt32";
inline constexpr char kPayloadMessage[] = "VaydeNet WT32-ETH01 Ethernet broadcast";

}  // namespace AppConfig
