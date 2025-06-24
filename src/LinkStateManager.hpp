#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>

struct NeighborInfo
{
    std::string ip;
    std::string hostname;
    std::chrono::steady_clock::time_point lastSeen;
    int stabilityCounter = 0; // Nouveau: compteur de stabilité
    int helloInterval = 5;    // Nouveau: intervalle Hello adaptatif
};

class LinkStateManager
{
public:
    bool updateNeighbor(const std::string &neighborIp, const std::string &neighborHostname);
    std::vector<std::string> getActiveNeighborHostnames() const;
    std::vector<std::string> getActiveNeighbors() const;
    void purgeInactiveNeighbors();

    // Nouvelles méthodes pour l'optimisation
    int getAdaptiveHelloInterval(const std::string &neighborIp);
    void updateNeighborStability(const std::string &neighborIp);
    bool isNeighborStable(const std::string &neighborIp);

private:
    std::unordered_map<std::string, NeighborInfo> neighbors;
    static constexpr int STABILITY_THRESHOLD = 10;
    static constexpr int MAX_HELLO_INTERVAL = 30;
    static constexpr int MIN_HELLO_INTERVAL = 5;
};