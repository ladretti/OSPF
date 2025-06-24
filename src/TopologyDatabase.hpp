#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../include/json.hpp"
#include <iostream>
#include "RoutingTable.hpp"
#include <set>
#include <queue>

class TopologyDatabase
{
public:
    std::unordered_map<std::string, nlohmann::json> lsaMap;

    bool updateLSA(const nlohmann::json &lsa)
    {
        if (lsa.contains("hostname") && lsa.contains("sequence_number"))
        {
            const std::string &host = lsa["hostname"];
            int seq = lsa["sequence_number"];
            if (!lsaMap.count(host) || lsaMap[host]["sequence_number"] < seq)
            {
                lsaMap[host] = lsa;
                return true;
            }
        }
        return false;
    }

    struct LinkInfo
    {
        std::string neighbor;
        double capacity;
        bool isActive;
        double weight;
    };

    RoutingTable computeRoutingTable(const std::string &selfHostname) const
    {
        // Graphe: hostname -> voisins (vector<string>)
        std::unordered_map<std::string, std::vector<LinkInfo>> weightedGraph;

        for (const auto &[hostname, lsa] : lsaMap)
        {
            if (lsa.contains("neighbors") && lsa.contains("link_capacities") && lsa.contains("link_states"))
            {
                const auto &neighbors = lsa["neighbors"];
                const auto &capacities = lsa["link_capacities"];
                const auto &states = lsa["link_states"];

                for (size_t i = 0; i < neighbors.size(); ++i)
                {
                    if (i < capacities.size() && i < states.size())
                    {
                        LinkInfo link;
                        link.neighbor = neighbors[i];
                        link.capacity = capacities[i].get<double>();
                        link.isActive = states[i].get<bool>();

                        // Ignorer les liens inactifs
                        if (!link.isActive)
                            continue;

                        // Calculer le poids : inverse de la capacité + pénalité pour faible capacité
                        link.weight = (1.0 / link.capacity) * 1000; // Normaliser

                        weightedGraph[hostname].push_back(link);
                    }
                }
            }
        }

        // Dijkstra (BFS simplifié, coût 1 par lien)
        std::unordered_map<std::string, double> dist;
        std::unordered_map<std::string, std::string> prev;
        std::priority_queue<std::pair<double, std::string>, std::vector<std::pair<double, std::string>>, std::greater<>> pq;

        for (const auto &[hostname, _] : lsaMap)
        {
            dist[hostname] = std::numeric_limits<double>::infinity();
        }
        dist[selfHostname] = 0.0;
        pq.push({0.0, selfHostname});

        while (!pq.empty())
        {
            auto [currentDist, u] = pq.top();
            pq.pop();

            if (currentDist > dist[u])
                continue;

            for (const auto &link : weightedGraph[u])
            {
                const std::string &v = link.neighbor;
                double weight = link.weight;
                double newDist = dist[u] + weight;

                if (newDist < dist[v])
                {
                    dist[v] = newDist;
                    prev[v] = u;
                    pq.push({newDist, v});
                }
            }
        }

        std::set<std::string> localNetworks;
        auto it = lsaMap.find(selfHostname);
        if (it != lsaMap.end() && it->second.contains("networks"))
        {
            for (const auto &net : it->second["networks"])
            {
                localNetworks.insert(net);
            }
        }

        RoutingTable rt;
        // for (const auto &[dest, distance] : dist)
        // {
        //     if (dest == selfHostname || distance == std::numeric_limits<double>::infinity())
        //         continue;

        //     // Trouver le next hop
        //     std::string hop = dest;
        //     while (prev.count(hop) && prev[hop] != selfHostname)
        //     {
        //         hop = prev[hop];
        //     }
        //     rt.table[dest] = hop;
        // }
        for (const auto &[hostname, lsa] : lsaMap)
        {
            if (lsa.contains("networks"))
            {
                for (const auto &net : lsa["networks"])
                {
                    if (localNetworks.count(net) || hostname == selfHostname)
                        continue;

                    // Calculer le next-hop pour ce réseau via Dijkstra
                    if (dist.count(hostname) && dist.at(hostname) != std::numeric_limits<double>::infinity())
                    {
                        std::string hop = hostname;
                        while (prev.count(hop) && prev[hop] != selfHostname)
                        {
                            hop = prev[hop];
                        }
                        rt.table[net] = hop; // Route vers le RÉSEAU avec next-hop

                        std::cout << "DEBUG: Added network route: " << net
                                  << " -> " << hop << " (distance: " << dist.at(hostname) << ")" << std::endl;
                    }
                }
            }
        }

        return rt;
    }
};