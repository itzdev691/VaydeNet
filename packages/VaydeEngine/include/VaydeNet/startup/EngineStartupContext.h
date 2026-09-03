#pragma once

struct HardwareIdentity;
struct NodeSettings;
class TransportInterface;

struct EngineStartupContext {
    const HardwareIdentity& identity;
    const NodeSettings& settings;
    TransportInterface& transport;
};
