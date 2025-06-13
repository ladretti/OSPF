#pragma once
#include <string>

class PacketManager {
public:
    void sendHello(const std::string& destIp, int port = 5000);
    void receivePackets(int port = 5000);
};
