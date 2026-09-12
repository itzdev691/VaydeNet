#include "packet.h"

#include <iostream>


bool sendMessage(const MessagePacket& message, Node& receiver)
{
    if (message.payload.empty())
    {
        return false;
    }

    receiver.inbox.push_back(message);
    return true;

}

int main() {
    Node receiver{200, {}};

    MessagePacket outgoingMessage{
        101,
        MessageType::Status,
        "Node online"
    };

    bool sent = sendMessage(outgoingMessage, receiver);

    if (sent)
    {
        std::cout << "Message delivered\n";
        std::cout << receiver.inbox[0].payload << "\n";
    }

    return 0;
}