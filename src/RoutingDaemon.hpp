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
    void showRoutingMetrics() const;
    void showRoutingTable() const;
    void showTrafficOptimizationStats() const;
    int getAdaptiveSleepTime() const;
    void resetOptimizationStats();

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

    std::vector<double> getLinkCapabilities() const;
    std::vector<bool> getLinkStates() const;

    std::chrono::steady_clock::time_point networkStartTime;
    std::chrono::steady_clock::time_point lastConvergenceTime;
    std::chrono::steady_clock::time_point lastTopologyChangeTime;
    bool hasConverged = false;
    int convergenceCount = 0;
    std::vector<std::chrono::milliseconds> convergenceTimes;

    // Méthodes pour les métriques de convergence
    void recordTopologyChange();
    void checkConvergence();
    double getAverageConvergenceTime() const;
};