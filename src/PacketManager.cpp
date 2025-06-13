// PacketManager.cpp
#include "PacketManager.h"
#include <iostream>

void PacketManager::sendHello(const std::string& ip) {
    std::cout << "Sending HELLO to " << ip << std::endl;
}

void PacketManager::receivePackets() {
    std::cout << "Receiving packets..." << std::endl;
}
