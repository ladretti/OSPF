#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include "utils.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include "TopologyDatabase.hpp"

using json = nlohmann::json;

std::string calculateBroadcastAddress(const std::string &ip)
{
    size_t lastDot = ip.find_last_of('.');
    if (lastDot == std::string::npos)
        return ip;

    return ip.substr(0, lastDot + 1) + "255";
}

int main(int argc, char *argv[])
{
    std::string routerId = "R_1";

    TopologyDatabase topoDb;

    if (argc > 1)
    {
        routerId = argv[1];
    }

    std::cout << "Starting router: " << routerId << std::endl;

    const std::string configFile = "config/router.conf";
    RouterConfig config = getRouterConfig(routerId, configFile);

    if (config.hostname.empty() || config.interfaces.empty())
    {
        std::cerr << "Invalid configuration for router: " << routerId << std::endl;
        return 1;
    }

    const std::string hostname = config.hostname;
    std::vector<std::string> interfaces = config.interfaces;
    int port = config.port;

    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Interfaces:" << std::endl;
    for (const auto &iface : interfaces)
    {
        std::cout << "  " << iface << std::endl;
    }
    std::cout << "Port: " << port << std::endl;

    LinkStateManager lsm;
    PacketManager pm;

    std::atomic<bool> running = true;
    std::thread receiverThread([&pm, &lsm, &running, port, hostname, &topoDb]()
                               {
        std::cout << "Starting receiver thread on port " << port << std::endl;
        pm.receivePackets(port, lsm, running, hostname, topoDb); });

    while (true)
    {
        topoDb.print();

        for (const auto &iface : interfaces)
        {
            std::string broadcastAddr = calculateBroadcastAddress(iface);
            std::cout << "Sending HELLO to broadcast address: " << broadcastAddr << std::endl;
            pm.sendHello(broadcastAddr, port, hostname, interfaces);
        }

        auto activeNeighbors = lsm.getActiveNeighbors();
        auto activeNeighborHostnames = lsm.getActiveNeighborHostnames();

        std::set<std::string> uniqueNeighbors(activeNeighborHostnames.begin(), activeNeighborHostnames.end());
        std::vector<std::string> neighbors(uniqueNeighbors.begin(), uniqueNeighbors.end());

        for (const auto &neighbor : activeNeighbors)
        {
            pm.sendHello(neighbor, port, hostname, interfaces);
            pm.sendLSA(neighbor, port, topoDb.lsaMap[hostname]);
        }

        static int mySeq = 0;
        std::vector<std::string> networks;
        for (const auto &iface : interfaces)
        {
            size_t lastDot = iface.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                networks.push_back(iface.substr(0, lastDot + 1) + "0/24");
            }
        }

        std::vector<json> networkInterfaces;
        for (size_t i = 0; i < interfaces.size(); ++i)
        {
            const auto &ifaceIp = interfaces[i];
            size_t lastDot = ifaceIp.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                std::string net = ifaceIp.substr(0, lastDot + 1) + "0/24";
                std::string ifaceName = (i < config.interfacesNames.size()) ? config.interfacesNames[i] : "";
                networkInterfaces.push_back({{"network", net},
                                             {"interface_ip", ifaceIp},
                                             {"interface_name", ifaceName}});
            }
        }

        json lsa = {
            {"type", "LSA"},
            {"hostname", hostname},
            {"sequence_number", mySeq++},
            {"interfaces", interfaces},
            {"neighbors", neighbors},
            {"networks", networks},
            {"network_interfaces", networkInterfaces}};

        std::string lsaStr = lsa.dump();

        std::string hmac = computeHMAC(lsaStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
        lsa["hmac"] = toHex(hmac);

        topoDb.updateLSA(lsa);

        lsm.purgeInactiveNeighbors();

        std::cout << "Active neighbors: ";
        for (const auto &neighbor : lsm.getActiveNeighbors())
        {
            std::cout << neighbor << " ";
        }
        std::cout << std::endl;

        auto routingTable = topoDb.computeRoutingTable(hostname);

        routingTable.print();
        // for (const auto &[dest, nextHop] : routingTable.table)
        // {
        //     // Ignore les routes locales ou les routes vers des routeurs (hostname)
        //     if (nextHop == "local" || nextHop == hostname)
        //         continue;

        //     // On ne traite que les réseaux (ex: 10.2.0.0/24)
        //     if (dest.find('/') == std::string::npos)
        //         continue;

        //     // Trouver l'IP du nextHop (à adapter selon ta structure)
        //     // Ici, on suppose que tu as une fonction pour obtenir l'IP d'un hostname voisin
        //     std::string nextHopIp = ""; // À compléter selon ta logique
        //     for (const auto &lsaPair : topoDb.lsaMap)
        //     {
        //         if (lsaPair.first == nextHop && lsaPair.second.contains("interfaces"))
        //         {
        //             // Prend la première interface IP du nextHop
        //             nextHopIp = lsaPair.second["interfaces"][0];
        //             break;
        //         }
        //     }

        //     // Trouver l'interface locale à utiliser (ex: la première interface)
        //     std::string iface = interfaces[0];

        //     if (!nextHopIp.empty())
        //     {
        //         addRoute(dest, nextHopIp, iface);
        //     }
        // }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    running = false;
    if (receiverThread.joinable())
    {
        receiverThread.join();
    }

    return 0;
}
