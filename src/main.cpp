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
#include <sys/un.h>
#include <unistd.h>

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
    std::string routerId = "R";

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

    std::thread commandThread([&running, &lsm]()
                              {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/routing.sock");
    unlink("/tmp/routing.sock");
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return;
    }

    listen(server_fd, 5);

    while (running) {
        int client_fd = accept(server_fd, nullptr, nullptr);
         if (client_fd < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        char buffer[256] = {0};
        read(client_fd, buffer, sizeof(buffer));
        std::string command(buffer);

        std::string response;
        if (command.find("neighbors") != std::string::npos) {
            auto neighbors = lsm.getActiveNeighborHostnames();
            response = "Neighbors:\n";
            for (const auto& n : neighbors) {
                response += "- " + n + "\n";
            }
        } else if (command.find("stop") != std::string::npos) {
            response = "Stopping daemon\n";
            running = false;
        } else {
            response = "Unknown command\n";
        }

        write(client_fd, response.c_str(), response.size());
        close(client_fd);
    }

    close(server_fd);
    unlink("/tmp/routing.sock"); });

    while (running)
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
        std::cout << "Voisins actifs et leurs interfaces :" << std::endl;
        for (const auto &neighborHostname : neighbors)
        {
            auto lsaIt = topoDb.lsaMap.find(neighborHostname);
            if (lsaIt != topoDb.lsaMap.end() && lsaIt->second.contains("network_interfaces"))
            {
                std::cout << "  " << neighborHostname << " : ";
                for (const auto &ni : lsaIt->second["network_interfaces"])
                {
                    std::cout << ni["interface_name"].get<std::string>() << "("
                              << ni["interface_ip"].get<std::string>() << ") ";
                }
                std::cout << std::endl;
            }
        }

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
        auto ipIfacePairs = getLocalIpInterfaceMapping();
        for (const auto &[ifaceIp, ifaceName] : ipIfacePairs)
        {
            // Vérifie que ifaceIp est dans ta config (pour ne pas annoncer les IPs de loopback, etc.)
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

    running = false;
    if (receiverThread.joinable())
        receiverThread.join();

    if (commandThread.joinable())
        commandThread.join();

    return 0;
}

