#pragma once
#include <string>
#include <unordered_map>
#include <iostream>

class RoutingTable {
public:
    std::unordered_map<std::string, std::string> table;

    void print() const {
        std::cout << "Routing Table:" << std::endl;
        for (const auto& [dest, nextHop] : table) {
            std::cout << "  " << dest << " via " << nextHop << std::endl;
        }
    }
};