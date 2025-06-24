#pragma once
#include "LinkStateManager.hpp"
#include "PacketManager.hpp"
#include "TopologyDatabase.hpp"
#include <atomic>
#include <thread>
#include <memory>

class RoutingDaemon {
public:
    RoutingDaemon(const std::string& configFile);
    ~RoutingDaemon();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    std::vector<std::string> getActiveNeighbors() const;
    void getStatus() const;
    
private:
    void runDaemon();
    void mainLoop();
    
    std::string hostname;
    std::vector<std::string> interfaces;
    int port;
    
    std::unique_ptr<LinkStateManager> lsm;
    std::unique_ptr<PacketManager> pm;
    std::unique_ptr<TopologyDatabase> topoDb;
    
    std::atomic<bool> running;
    std::thread daemonThread;
    std::thread receiverThread;
};