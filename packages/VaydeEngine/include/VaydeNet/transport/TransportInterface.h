#pragma once

#include <cstdint>

enum class TransportStatus : std::uint8_t {
    Ok,
    InitializationFailed
};

class TransportInterface {
public:
    virtual ~TransportInterface() = default;

    virtual TransportStatus initialize() = 0;
};