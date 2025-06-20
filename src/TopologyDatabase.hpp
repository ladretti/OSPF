#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../include/json.hpp"
#include <iostream>

class TopologyDatabase
{
public:
    // Map hostname -> dernier LSA reçu (sous forme de JSON)
    std::unordered_map<std::string, nlohmann::json> lsaMap;

    void updateLSA(const nlohmann::json &lsa)
    {
        if (lsa.contains("hostname") && lsa.contains("sequence_number"))
        {
            const std::string &host = lsa["hostname"];
            int seq = lsa["sequence_number"];
            if (!lsaMap.count(host) || lsaMap[host]["sequence_number"] < seq)
            {
                lsaMap[host] = lsa;
            }
        }
    }

    void print() const
    {
        for (const auto &[hostname, lsa] : lsaMap)
        {
            std::cout << "LSA from " << hostname
                      << " (seq " << lsa["sequence_number"] << "): "
                      << lsa.dump(2) << std::endl;
        }
    }
};