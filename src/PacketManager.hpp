#pragma once
#include <string>
#include <vector>

class PacketManager {
public:
    void sendHello(const std::string& destIp, int port = 5000,
                   const std::string& hostname = "",
                   const std::vector<std::string>& interfaces = {});
    void receivePackets(int port = 5000);
};
