#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include "utils.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include "TopologyDatabase.hpp"

std::string calculateBroadcastAddress(const std::string &ip)
{
    size_t lastDot = ip.find_last_of('.');
    if (lastDot == std::string::npos)
        return ip;

    return ip.substr(0, lastDot + 1) + "255";
}

int main(int argc, char *argv[])
{
    std::string routerId = "R_1";

    TopologyDatabase topoDb;

    if (argc > 1)
    {
        routerId = argv[1];
    }

    std::cout << "Starting router: " << routerId << std::endl;

    const std::string configFile = "config/router.conf";
    RouterConfig config = getRouterConfig(routerId, configFile);

    if (config.hostname.empty() || config.interfaces.empty())
    {
        std::cerr << "Invalid configuration for router: " << routerId << std::endl;
        return 1;
    }

    const std::string hostname = config.hostname;
    std::vector<std::string> interfaces = config.interfaces;
    int port = config.port;

    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Interfaces:" << std::endl;
    for (const auto &iface : interfaces)
    {
        std::cout << "  " << iface << std::endl;
    }
    std::cout << "Port: " << port << std::endl;

    LinkStateManager lsm;
    PacketManager pm;

    std::atomic<bool> running = true;
    std::thread receiverThread([&pm, &lsm, &running, port, hostname, &topoDb]()
                               {
        std::cout << "Starting receiver thread on port " << port << std::endl;
        pm.receivePackets(port, lsm, running, hostname, topoDb); });

    while (true)
    {
        topoDb.print();

        for (const auto &iface : interfaces)
        {
            std::string broadcastAddr = calculateBroadcastAddress(iface);
            std::cout << "Sending HELLO to broadcast address: " << broadcastAddr << std::endl;
            pm.sendHello(broadcastAddr, port, hostname, interfaces);
        }

        auto activeNeighbors = lsm.getActiveNeighbors();
        for (const auto &neighbor : activeNeighbors)
        {
            pm.sendHello(neighbor, port, hostname, interfaces);
            pm.sendLSA(neighbor, port, hostname, interfaces,  activeNeighbors);
        }

        lsm.purgeInactiveNeighbors();

        std::cout << "Active neighbors: ";
        for (const auto &neighbor : lsm.getActiveNeighbors())
        {
            std::cout << neighbor << " ";
        }
        std::cout << std::endl;

        auto routingTable = topoDb.computeRoutingTable(hostname);
        routingTable.print();

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    running = false;
    if (receiverThread.joinable())
    {
        receiverThread.join();
    }

    return 0;
}
