#pragma once
#include <string>
#include <vector>
#include <atomic>
#include "LinkStateManager.hpp"
#include "TopologyDatabase.hpp"

class PacketManager
{
public:
    void sendHello(const std::string &destIp, int port = 5000,
                   const std::string &hostname = "",
                   const std::vector<std::string> &interfaces = {});

    void receivePackets(int port, LinkStateManager &lsm, std::atomic<bool> &running, const std::string &hostname, TopologyDatabase &topoDb);
    
    void sendLSA(const std::string &destIp, int port,
                 const std::string &hostname,
                 const std::vector<std::string> &interfaces,
                 const std::vector<std::string> &neighbors);
};
