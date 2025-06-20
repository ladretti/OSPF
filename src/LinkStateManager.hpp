// LinkStateManager.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>

struct NeighborInfo
{
    std::string ip;
    std::chrono::steady_clock::time_point lastSeen;
};

class LinkStateManager
{
public:
    bool updateNeighbor(const std::string &neighborIp);
    std::vector<std::string> getActiveNeighbors() const;
    void purgeInactiveNeighbors();

private:
    std::unordered_map<std::string, NeighborInfo> neighbors;
};
