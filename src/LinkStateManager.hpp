// LinkStateManager.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <vector>

struct NeighborInfo
{
    std::string ip;
    std::string hostname; // Ajoute ce champ
    std::chrono::steady_clock::time_point lastSeen;
};

class LinkStateManager
{
public:
    bool updateNeighbor(const std::string &neighborIp, const std::string &neighborHostname);
    std::vector<std::string> getActiveNeighborHostnames() const;
    std::vector<std::string> getActiveNeighbors() const;
    void purgeInactiveNeighbors();

private:
    std::unordered_map<std::string, NeighborInfo> neighbors;
};