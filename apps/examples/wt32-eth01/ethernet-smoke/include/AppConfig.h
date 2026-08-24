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
inline constexpr uint32_t kPortProbeIntervalMs = 2000;
inline constexpr int32_t kPortProbeConnectTimeoutMs = 250;
inline constexpr uint8_t kEspNowChannel = 1;
inline constexpr uint16_t kDashboardPort = VAYDENET_DASHBOARD_PORT;
inline constexpr uint16_t kProbePorts[] = {8080, 42691, 9443, 8081};
inline constexpr size_t kProbePortCount =
    sizeof(kProbePorts) / sizeof(kProbePorts[0]);
inline constexpr char kHostname[] = "vaydenet-wt32";

static_assert(
    kEspNowChannel >= 1 && kEspNowChannel <= 14,
    "ESP-NOW channel must be between 1 and 14");
static_assert(
    kProbePortCount >= 3 && kProbePortCount <= 5,
    "Configure between three and five TCP probe ports");

}  // namespace AppConfig
