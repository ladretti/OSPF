#include "RoutingDaemon.hpp"
#include "utils.hpp"
#include <chrono>
#include <iostream>
#include <set>
#include "LinkStateManager.hpp"
#include "PacketManager.hpp"
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
RoutingDaemon::RoutingDaemon(const std::string &configFile)
    : running(false)
{

    auto configs = parseRouterConfig(configFile);
    if (configs.size() != 1)
    {
        throw std::runtime_error("Config file must contain exactly one router section");
    }

    RouterConfig config = configs.begin()->second;
    if (config.hostname.empty() || config.interfaces.empty())
    {
        throw std::runtime_error("Invalid router configuration");
    }

    hostname = config.hostname;
    interfaces = config.interfaces;
    port = config.port;

    lsm = std::make_unique<LinkStateManager>();
    pm = std::make_unique<PacketManager>();
    topoDb = std::make_unique<TopologyDatabase>();
}

RoutingDaemon::~RoutingDaemon()
{
    stop();
}

bool RoutingDaemon::start()
{
    if (running.load())
    {
        return false;
    }

    running.store(true);

    // Start receiver thread
    receiverThread = std::thread([this]()
                                 { pm->receivePackets(port, *lsm, running, hostname, *topoDb); });

    // Start main daemon thread
    daemonThread = std::thread(&RoutingDaemon::mainLoop, this);

    return true;
}

void RoutingDaemon::stop()
{
    if (!running.load())
    {
        return;
    }

    running.store(false);

    if (receiverThread.joinable())
    {
        receiverThread.join();
    }

    if (daemonThread.joinable())
    {
        daemonThread.join();
    }
}

bool RoutingDaemon::isRunning() const
{
    return running.load();
}

std::vector<std::string> RoutingDaemon::getActiveNeighbors() const
{
    if (!lsm)
        return {};
    return lsm->getActiveNeighbors();
}

void RoutingDaemon::getStatus() const
{
    std::cout << "Daemon Status: " << (running.load() ? "Running" : "Stopped") << std::endl;
    std::cout << "Hostname: " << hostname << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Interfaces: ";
    for (const auto &iface : interfaces)
    {
        std::cout << iface << " ";
    }
    std::cout << std::endl;

    if (running.load() && lsm)
    {
        auto neighbors = lsm->getActiveNeighbors();
        std::cout << "Active Neighbors (" << neighbors.size() << "): ";
        for (const auto &neighbor : neighbors)
        {
            std::cout << neighbor << " ";
        }
        std::cout << std::endl;
    }
}

void RoutingDaemon::mainLoop()
{

    while (running.load())
    {
        for (const auto &iface : interfaces) {
            std::string broadcastAddr = calculateBroadcastAddress(iface);
            pm->sendHello(broadcastAddr, port, hostname, interfaces);
        }

       auto activeNeighbors = lsm->getActiveNeighbors();
        auto activeNeighborHostnames = lsm->getActiveNeighborHostnames();

        std::set<std::string> uniqueNeighbors(activeNeighborHostnames.begin(), activeNeighborHostnames.end());
        std::vector<std::string> neighbors(uniqueNeighbors.begin(), uniqueNeighbors.end());

        for (const auto &neighbor : activeNeighbors) {
            pm->sendHello(neighbor, port, hostname, interfaces);
            pm->sendLSA(neighbor, port, topoDb->lsaMap[hostname]);
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
        auto ipIfacePairs = getLocalIpInterfaceMapping();
        for (const auto &[ifaceIp, ifaceName] : ipIfacePairs)
        {
            if (std::find(interfaces.begin(), interfaces.end(), ifaceIp) != interfaces.end())
            {
                size_t lastDot = ifaceIp.find_last_of('.');
                if (lastDot != std::string::npos)
                {
                    std::string net = ifaceIp.substr(0, lastDot + 1) + "0/24";
                    networkInterfaces.push_back({{"network", net},
                                                 {"interface_ip", ifaceIp},
                                                 {"interface_name", ifaceName}});
                }
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

        topoDb->updateLSA(lsa);

        lsm->purgeInactiveNeighbors();

        for (const auto &neighbor : lsm->getActiveNeighbors())
        {
            std::cout << neighbor << " ";
        }
        std::cout << std::endl;

        auto routingTable = topoDb->computeRoutingTable(hostname);

        routingTable.print();
        for (const auto &[dest, nextHop] : routingTable.table)
        {
            if (nextHop == "local" || nextHop == hostname)
                continue;
            if (dest.find('/') == std::string::npos)
                continue;

            std::string nextHopIp = "";
            std::string iface = "";

            auto lsaIt = topoDb->lsaMap.find(nextHop);
            if (lsaIt != topoDb->lsaMap.end() && lsaIt->second.contains("interfaces"))
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
                            for (const auto &ni : networkInterfaces)
                            {
                                if (ni["interface_ip"] == localIp)
                                {
                                    iface = ni["interface_name"];
                                    break;
                                }
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
}
