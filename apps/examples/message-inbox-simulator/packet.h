#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class MessageType {
    HeartBeat = 0,
    Status = 1,
    Text = 2,
    Command = 3
};

enum class TransportType {
    Unspecified = 0,
    EspNow = 1,
    LoRa = 2,
    Bluetooth = 3,
    Wifi = 4,
    Ethernet = 5
};


struct MessagePacket {

    std::uint32_t senderId;
    MessageType messageType;
    std::string payload;
    

};

struct Node {
    std::uint32_t nodeId;
    std::vector<MessagePacket> inbox;
};