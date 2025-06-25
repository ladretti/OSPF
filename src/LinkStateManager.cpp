// LinkStateManager.cpp
#include "LinkStateManager.hpp"
#include <chrono>
#include <iostream>

bool LinkStateManager::updateNeighbor(const std::string &neighborIp, const std::string &neighborHostname)
{
    auto now = std::chrono::steady_clock::now();
    auto it = neighbors.find(neighborIp);

    if (it != neighbors.end())
    {
        it->second.lastSeen = now;
        updateNeighborStability(neighborIp);
        return false; // Pas nouveau
    }
    else
    {
        // Nouveau voisin
        NeighborInfo info;
        info.ip = neighborIp;
        info.hostname = neighborHostname;
        info.lastSeen = now;
        info.stabilityCounter = 0;
        info.helloInterval = MIN_HELLO_INTERVAL;

        neighbors[neighborIp] = info;
        return true; // Nouveau voisin
    }
}
std::vector<std::string> LinkStateManager::getActiveNeighborHostnames() const
{
    std::vector<std::string> hostnames;
    auto now = std::chrono::steady_clock::now();
    for (const auto &[ip, info] : neighbors)
    {
        // ✅ UTILISER LE MÊME TIMEOUT que purgeInactiveNeighbors()
        if (now - info.lastSeen < std::chrono::seconds(45) && !info.hostname.empty())
        {
            hostnames.push_back(info.hostname);
        }
    }
    return hostnames;
}

std::vector<std::string> LinkStateManager::getActiveNeighbors() const
{
    std::vector<std::string> active;
    auto now = std::chrono::steady_clock::now();
    for (const auto &[ip, info] : neighbors)
    {
        // ✅ UTILISER LE MÊME TIMEOUT que purgeInactiveNeighbors()
        if (now - info.lastSeen < std::chrono::seconds(45))
        {
            active.push_back(ip);
        }
    }
    return active;
}
void LinkStateManager::purgeInactiveNeighbors()
{
    auto now = std::chrono::steady_clock::now();
    auto it = neighbors.begin();

    while (it != neighbors.end())
    {
        auto timeSinceLastSeen = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastSeen);

        // TIMEOUT TRÈS GÉNÉREUX : 45 secondes au lieu de 10
        if (timeSinceLastSeen > std::chrono::seconds(45))
        {
            it = neighbors.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

int LinkStateManager::getAdaptiveHelloInterval(const std::string &neighborIp)
{
    auto it = neighbors.find(neighborIp);
    if (it == neighbors.end())
    {
        return MIN_HELLO_INTERVAL; // Défaut pour nouveaux voisins
    }

    // Plus le voisin est stable, plus l'intervalle peut être long
    int stability = it->second.stabilityCounter;
    if (stability > 20)
        return MAX_HELLO_INTERVAL; // Très stable
    else if (stability > 15)
        return 20; // Stable
    else if (stability > 10)
        return 15; // Moyennement stable
    else if (stability > 5)
        return 10; // Peu stable
    else
        return MIN_HELLO_INTERVAL; // Instable
}

void LinkStateManager::updateNeighborStability(const std::string &neighborIp)
{
    auto it = neighbors.find(neighborIp);
    if (it != neighbors.end())
    {
        // Augmenter le compteur de stabilité (max 25)
        it->second.stabilityCounter = std::min(25, it->second.stabilityCounter + 1);
        it->second.helloInterval = getAdaptiveHelloInterval(neighborIp);
    }
}

bool LinkStateManager::isNeighborStable(const std::string &neighborIp)
{
    auto it = neighbors.find(neighborIp);
    return (it != neighbors.end() && it->second.stabilityCounter >= STABILITY_THRESHOLD);
}