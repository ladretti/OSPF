#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../include/json.hpp"
#include <iostream>

class TopologyDatabase {
public:
    // Map hostname -> dernier LSA reçu (sous forme de JSON)
    std::unordered_map<std::string, nlohmann::json> lsaMap;

    void updateLSA(const nlohmann::json& lsa) {
        if (lsa.contains("hostname")) {
            lsaMap[lsa["hostname"]] = lsa;
        }
    }

    // Pour debug/affichage
    void print() const {
        for (const auto& [hostname, lsa] : lsaMap) {
            std::cout << "LSA from " << hostname << ": " << lsa.dump(2) << std::endl;
        }
    }
};