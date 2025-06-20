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

    RoutingTable computeRoutingTable(const std::string &selfHostname) const
    {
        // Graphe: hostname -> voisins (vector<string>)
        std::unordered_map<std::string, std::vector<std::string>> graph;
        for (const auto &[hostname, lsa] : lsaMap)
        {
            if (lsa.contains("neighbors"))
            {
                for (const auto &neighbor : lsa["neighbors"])
                {
                    graph[hostname].push_back(neighbor);
                }
            }
        }

        // Dijkstra (BFS simplifié, coût 1 par lien)
        std::unordered_map<std::string, int> dist;
        std::unordered_map<std::string, std::string> prev;
        std::set<std::string> visited;
        std::queue<std::string> q;

        dist[selfHostname] = 0;
        q.push(selfHostname);

        while (!q.empty())
        {
            std::string u = q.front();
            q.pop();
            visited.insert(u);

            for (const auto &v : graph[u])
            {
                if (!visited.count(v))
                {
                    if (!dist.count(v) || dist[v] > dist[u] + 1)
                    {
                        dist[v] = dist[u] + 1;
                        prev[v] = u;
                        q.push(v);
                    }
                }
            }
        }

        RoutingTable rt;
        for (const auto &[dest, _] : dist)
        {
            if (dest == selfHostname)
                continue;
            // remonte le chemin pour trouver le next hop
            std::string hop = dest;
            while (prev[hop] != selfHostname)
            {
                hop = prev[hop];
            }
            rt.table[dest] = hop;
        }
        return rt;
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