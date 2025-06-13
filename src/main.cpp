#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include "utils.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

int main(int argc, char* argv[])
{
    std::string routerId = "R_1";
    if (argc > 1) {
        routerId = argv[1];
    }
    
    std::cout << "Starting router: " << routerId << std::endl;
    
    const std::string configFile = "config/router.conf";
    RouterConfig config = getRouterConfig(routerId, configFile);
    
    if (config.hostname.empty() || config.interfaces.empty()) {
        std::cerr << "Invalid configuration for router: " << routerId << std::endl;
        return 1;
    }
    
    const std::string hostname = config.hostname;
    std::vector<std::string> interfaces = config.interfaces;
    int port = config.port;
    
    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Interfaces:" << std::endl;
    for (const auto& iface : interfaces) {
        std::cout << "  " << iface << std::endl;
    }
    std::cout << "Port: " << port << std::endl;
    
    LinkStateManager lsm;
    PacketManager pm;
    
    std::atomic<bool> running = true;
    std::thread receiverThread([&pm, &lsm, &running, port]() {
        std::cout << "Starting receiver thread on port " << port << std::endl;
        pm.receivePackets(port, lsm, running);
    });
    
    while (true)
    {
        pm.sendHello("255.255.255.255", port, hostname, interfaces);
        
        auto activeNeighbors = lsm.getActiveNeighbors();
        for (const auto& neighbor : activeNeighbors) {
            pm.sendHello(neighbor, port, hostname, interfaces);
        }
        
        lsm.purgeInactiveNeighbors();
        
        std::cout << "Active neighbors: ";
        for (const auto& neighbor : lsm.getActiveNeighbors()) {
            std::cout << neighbor << " ";
        }
        std::cout << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    running = false;
    if (receiverThread.joinable()) {
        receiverThread.join();
    }
    
    return 0;
}