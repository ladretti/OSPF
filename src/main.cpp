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

    LinkStateManager lsm;
    PacketManager pm;

    std::atomic<bool> running = true;
    std::thread receiverThread([&pm, &lsm, &running, port, hostname, &topoDb]()
                               { pm.receivePackets(port, lsm, running, hostname, topoDb); });

    while (true)
    {

        for (const auto &iface : interfaces)
        {
            std::string broadcastAddr = calculateBroadcastAddress(iface);
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

        for (const auto &neighbor : lsm.getActiveNeighbors())
        {
            std::cout << neighbor << " ";
        }
        std::cout << std::endl;

        auto routingTable = topoDb.computeRoutingTable(hostname);

        routingTable.print();
        for (const auto &[dest, nextHop] : routingTable.table)
        {
            if (nextHop == "local" || nextHop == hostname)
                continue;
            if (dest.find('/') == std::string::npos)
                continue;

            std::string nextHopIp = "";
            std::string iface = "";

            auto lsaIt = topoDb.lsaMap.find(nextHop);
            if (lsaIt != topoDb.lsaMap.end() && lsaIt->second.contains("interfaces"))
            {
                const auto &nextHopIfaces = lsaIt->second["interfaces"];
                for (size_t i = 0; i < interfaces.size(); ++i)
                {
                    const std::string &localIp = interfaces[i];
                    size_t lastDot = localIp.find_last_of('.');
                    if (lastDot == std::string::npos)
                        continue;
                    std::string localNet = localIp.substr(0, lastDot + 1);

                    for (const auto &nhIp : nextHopIfaces)
                    {
                        size_t nhLastDot = nhIp.get<std::string>().find_last_of('.');
                        if (nhLastDot == std::string::npos)
                            continue;
                        std::string nhNet = nhIp.get<std::string>().substr(0, nhLastDot + 1);

                        if (localNet == nhNet)
                        {
                            nextHopIp = nhIp.get<std::string>();
                            // Associe le nom d'interface à l'IP locale trouvée
                            auto it = std::find(interfaces.begin(), interfaces.end(), localIp);
                            if (it != interfaces.end())
                            {
                                size_t idx = std::distance(interfaces.begin(), it);
                                iface = (idx < config.interfacesNames.size()) ? config.interfacesNames[idx] : "";
                            }
                            break;
                        }
                    }
                    if (!nextHopIp.empty())
                        break;
                }
            }
            if (!nextHopIp.empty() && !iface.empty())
            {
                addRoute(dest, nextHopIp, iface);
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    running = false;
    if (receiverThread.joinable())
    {
        receiverThread.join();
    }

    return 0;
}
