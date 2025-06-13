// RoutingProtocol.h
#pragma once
#include <string>
#include <map>

class RoutingProtocol {
public:
    void computeRoutes();
    std::map<std::string, std::string> getRoutingTable() const;
private:
    std::map<std::string, std::string> routingTable;
};
