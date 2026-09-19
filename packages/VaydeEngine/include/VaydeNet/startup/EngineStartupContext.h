#pragma once

struct HardwareIdentity;
struct NodeSettings;

class MessageSink;
class TransportInterface;

struct EngineStartupContext {
    const HardwareIdentity& identity;
    const NodeSettings& settings;
    TransportInterface& transport;
    MessageSink& message_sink;
};
