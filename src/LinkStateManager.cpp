// LinkStateManager.cpp
#include "LinkStateManager.hpp"
#include <chrono>

bool LinkStateManager::updateNeighbor(const std::string& neighborIp)
{
    auto [it, inserted] = neighbors.insert_or_assign(neighborIp, std::chrono::steady_clock::now());
    return inserted;
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
