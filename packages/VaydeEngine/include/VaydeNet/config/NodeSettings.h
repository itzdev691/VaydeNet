#pragma once

#include <cstdint>
#include <string>


enum class TransportType : std::uint8_t {
    Unspecified,
    EspNow,
    Nrf24,
    Ethernet
};


enum NodeCapability : std::uint32_t {
    CapabilityNone = 0,
    CapabilitySensor = 1,
    CapabilityDisplay = 2,
    CapabilityRelay = 4
};

enum class SecurityMode : std::uint8_t {
    None,
    PreSharedKey
};


struct NodeSettings {
    // Identity
    std::string node_id{};
    std::string network_id{};

    // Format compatibility
    std::uint16_t protocol_version{1};
    std::uint16_t settings_version{1};

    // Communication
    TransportType transport{TransportType::Unspecified};
    std::uint16_t channel{};

    // Behaviour and security
    std::uint32_t capabilities{CapabilityNone};
    SecurityMode security_mode{SecurityMode::None};

};