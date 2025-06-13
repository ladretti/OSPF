// RoutingProtocol.cpp
#include "RoutingProtocol.hpp"

void RoutingProtocol::computeRoutes() {
    // Simule un calcul de plus court chemin
    routingTable["192.168.2.0/24"] = "10.1.0.2"; // Next-hop fictif
}

std::map<std::string, std::string> RoutingProtocol::getRoutingTable() const {
    return routingTable;
}
