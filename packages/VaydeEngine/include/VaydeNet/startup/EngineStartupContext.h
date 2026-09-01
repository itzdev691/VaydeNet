#pragma once

struct BoardInformation;
struct NodeSettings;
class TransportInterface;

struct EngineStartupContext {
    const BoardInformation& identity;
    const NodeSettings& settings;
    TransportInterface& transport;
};
