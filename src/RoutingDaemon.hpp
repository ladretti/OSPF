#pragma once
#include "LinkStateManager.hpp"
#include "PacketManager.hpp"
#include "TopologyDatabase.hpp"
#include <atomic>
#include <thread>
#include <memory>

class RoutingDaemon
{
public:
    RoutingDaemon(const std::string &configFile);
    ~RoutingDaemon();

    bool start();
    void stop();
    bool isRunning() const;

    std::vector<std::string> getActiveNeighbors() const;
    std::vector<std::string> getActiveNeighborHostnames() const;
    void getStatus() const;

    bool pingHost(const std::string &target, int count = 4) const;
    void showPingResults(const std::string &target, int count = 4) const;
    void requestNeighborsFrom(const std::string &targetIp) const;

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