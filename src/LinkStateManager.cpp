// LinkStateManager.cpp
#include "LinkStateManager.hpp"
#include <chrono>

bool LinkStateManager::updateNeighbor(const std::string& neighborIp)
{
    auto now = std::chrono::steady_clock::now();
    auto it = neighbors.find(neighborIp);
    bool isNew = (it == neighbors.end());
    neighbors[neighborIp] = NeighborInfo{neighborIp, now};
    return isNew;
}

std::vector<std::string> LinkStateManager::getActiveNeighbors() const {
    std::vector<std::string> active;
    auto now = std::chrono::steady_clock::now();
    for (const auto& [ip, info] : neighbors) {
        if (now - info.lastSeen < std::chrono::seconds(10)) {
            active.push_back(ip);
        }
    }
    return active;
}

void LinkStateManager::purgeInactiveNeighbors() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = neighbors.begin(); it != neighbors.end(); ) {
        if (now - it->second.lastSeen > std::chrono::seconds(30)) {
            it = neighbors.erase(it);
        } else {
            ++it;
        }
    }
}
