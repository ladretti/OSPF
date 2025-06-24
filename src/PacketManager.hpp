#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include "LinkStateManager.hpp"
#include "TopologyDatabase.hpp"

class PacketManager
{
private:
    // Nouveau: Cache pour optimisation
    std::unordered_map<std::string, std::string> lastSentLSAHash;                         // neighbor -> hash
    std::unordered_map<std::string, nlohmann::json> lastSentLSA;                          // neighbor -> LSA
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastHelloSent; // neighbor -> timestamp

public:
    void sendHello(const std::string &destIp, int port = 5000,
                   const std::string &hostname = "",
                   const std::vector<std::string> &interfaces = {});

    void receivePackets(int port, LinkStateManager &lsm, std::atomic<bool> &running,
                        const std::string &hostname, TopologyDatabase &topoDb);

    void sendLSA(const std::string &destIp, int port, const nlohmann::json &lsaMsg);
    void sendNeighborRequest(const std::string &destIp, int port, const std::string &hostname);
    void sendNeighborResponse(const std::string &destIp, int port, const std::string &hostname,
                              const std::vector<std::string> &neighbors);

    // Nouvelles méthodes pour l'optimisation
    void sendOptimizedLSA(const std::string &destIp, int port,
                          const nlohmann::json &currentLSA, const std::string &hostname);
    bool shouldSendHello(const std::string &neighborIp, int adaptiveInterval = 5);
    nlohmann::json createDifferentialLSA(const nlohmann::json &oldLSA,
                                         const nlohmann::json &newLSA, const std::string &hostname);
    std::string compressData(const std::string &data);
    std::string decompressData(const std::string &compressedData);
    void resetOptimizationCache();

    // Statistiques de trafic
    struct TrafficStats
    {
        size_t totalBytesSent = 0;
        size_t totalBytesReceived = 0;
        size_t compressedMessages = 0;
        size_t differentialMessages = 0;
        size_t fullMessages = 0;
    } stats;

    const TrafficStats &getTrafficStats() const { return stats; }
};