// PacketManager.h
#pragma once
#include <string>

class PacketManager {
public:
    void sendHello(const std::string& ip);
    void receivePackets();
};
