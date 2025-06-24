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

bool RoutingDaemon::pingHost(const std::string &target, int count) const
{
    std::string cmd = "ping -c " + std::to_string(count) + " " + target + " > /dev/null 2>&1";
    int result = std::system(cmd.c_str());
    return result == 0;
}

void RoutingDaemon::showPingResults(const std::string &target, int count) const
{
    std::cout << "Pinging " << target << " with " << count << " packets..." << std::endl;

    std::string cmd = "ping -c " + std::to_string(count) + " " + target;
    int result = std::system(cmd.c_str());

    if (result == 0)
    {
        std::cout << "Ping to " << target << " successful" << std::endl;
    }
    else
    {
        std::cout << "Ping to " << target << " failed" << std::endl;
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

std::vector<std::string> RoutingDaemon::getActiveNeighborHostnames() const
{
    if (!lsm)
        return {};
    return lsm->getActiveNeighborHostnames();
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
        auto loopStart = std::chrono::steady_clock::now();

        // 1. Purger les voisins inactifs en premier
        lsm->purgeInactiveNeighbors();

        // 2. Hello optimisés avec intervalles adaptatifs
        // Hello broadcast sur toutes les interfaces (pour découverte initiale)
        for (const auto &iface : interfaces)
        {
            std::string broadcastAddr = calculateBroadcastAddress(iface);
            // Hello broadcast moins fréquent si réseau stable
            static int broadcastCounter = 0;
            if (broadcastCounter % 3 == 0) // Tous les 3 cycles au lieu de chaque cycle
            {
                pm->sendHello(broadcastAddr, port, hostname, interfaces);
            }
            broadcastCounter++;
        }

        // Hello unicast adaptatifs aux voisins connus
        auto activeNeighbors = lsm->getActiveNeighbors();
        for (const auto &neighbor : activeNeighbors)
        {
            // Utiliser l'intervalle adaptatif pour chaque voisin
            int adaptiveInterval = lsm->getAdaptiveHelloInterval(neighbor);
            if (pm->shouldSendHello(neighbor, adaptiveInterval))
            {
                pm->sendHello(neighbor, port, hostname, interfaces);
            }
        }

        // 3. Créer le LSA actuel une seule fois
        auto activeNeighborHostnames = lsm->getActiveNeighborHostnames();
        std::set<std::string> uniqueNeighbors(activeNeighborHostnames.begin(), activeNeighborHostnames.end());
        std::vector<std::string> neighbors(uniqueNeighbors.begin(), uniqueNeighbors.end());

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

        json currentLSA = {
            {"type", "LSA"},
            {"hostname", hostname},
            {"sequence_number", mySeq++},
            {"interfaces", interfaces},
            {"neighbors", neighbors},
            {"networks", networks},
            {"network_interfaces", networkInterfaces},
            {"link_capacities", getLinkCapabilities()},
            {"link_states", getLinkStates()}};

        std::string lsaStr = currentLSA.dump();
        std::string hmac = computeHMAC(lsaStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
        currentLSA["hmac"] = toHex(hmac);

        // 4. Envoyer LSA optimisés (différentiels/compressés) aux voisins
        for (const auto &neighbor : activeNeighbors)
        {
            pm->sendOptimizedLSA(neighbor, port, currentLSA, hostname);
        }

        // 5. Mettre à jour la topologie locale
        topoDb->updateLSA(currentLSA);

        // 6. Recalculer les routes seulement si nécessaire (triggered updates)
        // 6. Recalculer les routes seulement si nécessaire (triggered updates)
        static std::map<std::string, std::string> lastRoutingTable;
        static bool firstRun = true;

        auto newRoutingTable = topoDb->computeRoutingTable(hostname);

        td::cout << "DEBUG: LSA Database contains " << topoDb->lsaMap.size() << " entries:" << std::endl;
        for (const auto &[host, lsa] : topoDb->lsaMap)
        {
            std::cout << "  - " << host << std::endl;
            if (lsa.contains("networks"))
            {
                std::cout << "    Networks: " << lsa["networks"].dump() << std::endl;
            }
        }

        // DIAGNOSTIC 2: Voir la table de routage calculée
        std::cout << "DEBUG: Computed routing table:" << std::endl;
        for (const auto &[dest, nextHop] : newRoutingTable.table)
        {
            std::cout << "  " << dest << " -> " << nextHop << std::endl;
        }

        // DIAGNOSTIC 3: Comparer avec la table précédente
        std::cout << "DEBUG: Last routing table size: " << lastRoutingTable.size() << std::endl;
        std::cout << "DEBUG: New routing table size: " << newRoutingTable.table.size() << std::endl;
        bool routingTableChanged = firstRun; // Force au premier run

        if (!firstRun)
        {
            // Logique existante seulement si ce n'est pas le premier run
            if (newRoutingTable.table.size() != lastRoutingTable.size())
            {
                routingTableChanged = true;
            }
            else
            {
                for (const auto &[dest, nextHop] : newRoutingTable.table)
                {
                    auto it = lastRoutingTable.find(dest);
                    if (it == lastRoutingTable.end() || it->second != nextHop)
                    {
                        routingTableChanged = true;
                        break;
                    }
                }
            }
        }

        // 7. Appliquer les routes seulement si elles ont changé
        if (routingTableChanged)
        {
            // Debug pour voir ce qui se passe
            if (firstRun)
            {
                std::cout << "DEBUG: First run - applying " << newRoutingTable.table.size() << " initial routes" << std::endl;
                firstRun = false; // ← IMPORTANT : Marquer la fin du premier run
            }
            else
            {
                std::cout << "DEBUG: Routing table changed - updating routes" << std::endl;
            }

            for (const auto &[dest, nextHop] : newRoutingTable.table)
            {
                std::cout << "DEBUG: Processing route: " << dest << " -> " << nextHop << std::endl;

                if (nextHop == "local" || nextHop == hostname)
                {
                    std::cout << "DEBUG: Skipping local route" << std::endl;
                    continue;
                }
                if (dest.find('/') == std::string::npos)
                {
                    std::cout << "DEBUG: Skipping route without CIDR" << std::endl;
                    continue;
                }

                std::string nextHopIp = "";
                std::string iface = "";

                auto lsaIt = topoDb->lsaMap.find(nextHop);
                if (lsaIt != topoDb->lsaMap.end() && lsaIt->second.contains("interfaces"))
                {
                    // ...existing code pour résoudre nextHopIp et iface...
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
                    std::cout << "DEBUG: Adding route: " << dest << " via " << nextHopIp << " dev " << iface << std::endl;
                    addRoute(dest, nextHopIp, iface);
                }
                else
                {
                    std::cout << "DEBUG: Cannot resolve nextHopIp or interface for " << dest << std::endl;
                }
            }

            // Sauvegarder la nouvelle table de routage
            lastRoutingTable = std::map<std::string, std::string>(newRoutingTable.table.begin(), newRoutingTable.table.end());
        }

        // 8. Sleep adaptatif basé sur la stabilité du réseau
        int sleepTime = getAdaptiveSleepTime();

        // Calculer le temps déjà écoulé dans cette boucle
        auto loopEnd = std::chrono::steady_clock::now();
        auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopStart).count();

        // Ajuster le sleep pour maintenir un timing cohérent
        int remainingSleep = std::max(1000, sleepTime - static_cast<int>(loopDuration)); // Minimum 1s

        std::this_thread::sleep_for(std::chrono::milliseconds(remainingSleep));
    }
}

void RoutingDaemon::requestNeighborsFrom(const std::string &targetIp) const
{
    if (!running.load())
    {
        std::cout << "Daemon must be running to request neighbors" << std::endl;
        return;
    }

    std::cout << "Requesting neighbor list from " << targetIp << "..." << std::endl;
    pm->sendNeighborRequest(targetIp, port, hostname);
}

std::vector<double> RoutingDaemon::getLinkCapabilities() const
{
    std::vector<double> capacities;
    auto activeNeighbors = lsm->getActiveNeighbors();
    auto ipIfacePairs = getLocalIpInterfaceMapping();

    for (const auto &neighbor : activeNeighbors)
    {
        double capacity = 1000.0; // Valeur par défaut (1 Gbps)

        // Trouver l'interface locale utilisée pour ce voisin
        for (const auto &localIp : interfaces)
        {
            size_t lastDot = localIp.find_last_of('.');
            if (lastDot == std::string::npos)
                continue;
            std::string localNet = localIp.substr(0, lastDot + 1);

            size_t neighLastDot = neighbor.find_last_of('.');
            if (neighLastDot == std::string::npos)
                continue;
            std::string neighNet = neighbor.substr(0, neighLastDot + 1);

            if (localNet == neighNet)
            {
                // Trouver le nom de l'interface
                for (const auto &[ip, ifaceName] : ipIfacePairs)
                {
                    if (ip == localIp)
                    {
                        // Déterminer la capacité basée sur le type d'interface
                        if (ifaceName.find("eth") != std::string::npos ||
                            ifaceName.find("enp") != std::string::npos)
                        {
                            capacity = 1000.0; // Ethernet 1 Gbps
                        }
                        else if (ifaceName.find("wlan") != std::string::npos ||
                                 ifaceName.find("wifi") != std::string::npos)
                        {
                            capacity = 100.0; // WiFi ~100 Mbps
                        }
                        else if (ifaceName.find("lo") != std::string::npos)
                        {
                            capacity = 10000.0; // Loopback très rapide
                        }
                        break;
                    }
                }
                break;
            }
        }

        capacities.push_back(capacity);
    }

    return capacities;
}

std::vector<bool> RoutingDaemon::getLinkStates() const
{
    std::vector<bool> states;
    auto activeNeighbors = lsm->getActiveNeighbors();

    for (const auto &neighbor : activeNeighbors)
    {
        states.push_back(true);
    }

    return states;
}

void RoutingDaemon::showRoutingMetrics() const
{
    std::cout << "\n=== Routing Metrics ===" << std::endl;
    std::cout << "Router: " << hostname << std::endl;

    auto routingTable = topoDb->computeRoutingTable(hostname);

    for (const auto &[dest, nextHop] : routingTable.table)
    {
        std::cout << "Destination: " << dest << " -> Next Hop: " << nextHop << std::endl;

        if (topoDb->lsaMap.count(nextHop))
        {
            const auto &lsa = topoDb->lsaMap.at(nextHop);
            if (lsa.contains("link_capacities") && lsa.contains("neighbors"))
            {
                const auto &neighbors = lsa["neighbors"];
                const auto &capacities = lsa["link_capacities"];

                for (size_t i = 0; i < neighbors.size(); ++i)
                {
                    if (i < capacities.size())
                    {
                        std::cout << "  Link to " << neighbors[i]
                                  << ": " << capacities[i].get<double>() << " Mbps" << std::endl;
                    }
                }
            }
        }
    }
    std::cout << "======================" << std::endl;
}

void RoutingDaemon::showRoutingTable() const
{
    if (!running.load())
    {
        std::cout << "Daemon must be running to show routing table" << std::endl;
        return;
    }

    std::cout << "\n=== Current Routing Table ===" << std::endl;
    std::cout << "Router: " << hostname << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    auto routingTable = topoDb->computeRoutingTable(hostname);

    if (routingTable.table.empty())
    {
        std::cout << "No routes available" << std::endl;
        std::cout << "=================================" << std::endl;
        return;
    }

    // Afficher l'en-tête
    std::cout << std::left << std::setw(20) << "Destination"
              << std::setw(15) << "Next Hop"
              << std::setw(15) << "Interface"
              << std::setw(10) << "Metric" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    // Obtenir les interfaces réseau pour trouver les noms d'interfaces
    auto ipIfacePairs = getLocalIpInterfaceMapping();
    std::vector<json> networkInterfaces;
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

    for (const auto &[dest, nextHop] : routingTable.table)
    {
        std::string interfaceName = "unknown";
        std::string metric = "1";

        if (nextHop == "local" || nextHop == hostname)
        {
            interfaceName = "local";
            metric = "0";
        }
        else
        {
            // Trouver l'interface de sortie
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
                            for (const auto &ni : networkInterfaces)
                            {
                                if (ni["interface_ip"] == localIp)
                                {
                                    interfaceName = ni["interface_name"];
                                    break;
                                }
                            }
                            break;
                        }
                    }
                    if (interfaceName != "unknown")
                        break;
                }
            }
        }

        std::cout << std::left << std::setw(20) << dest
                  << std::setw(15) << nextHop
                  << std::setw(15) << interfaceName
                  << std::setw(10) << metric << std::endl;
    }

    std::cout << "=================================" << std::endl;
    std::cout << "Total routes: " << routingTable.table.size() << std::endl;
}

int RoutingDaemon::getAdaptiveSleepTime() const
{
    auto activeNeighbors = lsm->getActiveNeighbors();
    int stableNeighbors = 0;

    for (const auto &neighbor : activeNeighbors)
    {
        if (lsm->isNeighborStable(neighbor))
        {
            stableNeighbors++;
        }
    }

    // Plus le réseau est stable, plus on peut attendre
    if (stableNeighbors == activeNeighbors.size() && stableNeighbors > 0)
    {
        return 8000; // 8 secondes si tous les voisins sont stables
    }
    else if (stableNeighbors > activeNeighbors.size() / 2)
    {
        return 6000; // 6 secondes si majorité stable
    }
    else
    {
        return 4000; // 4 secondes si réseau instable
    }
}

void RoutingDaemon::showTrafficOptimizationStats() const
{
    const auto &stats = pm->getTrafficStats();

    std::cout << "\n=== Traffic Optimization Statistics ===" << std::endl;
    std::cout << "Total bytes sent: " << stats.totalBytesSent << " bytes" << std::endl;
    std::cout << "Total bytes received: " << stats.totalBytesReceived << " bytes" << std::endl;
    std::cout << "Compressed messages: " << stats.compressedMessages << std::endl;
    std::cout << "Differential messages: " << stats.differentialMessages << std::endl;
    std::cout << "Full messages: " << stats.fullMessages << std::endl;

    if (stats.fullMessages > 0)
    {
        double compressionRatio = (double)(stats.compressedMessages + stats.differentialMessages) /
                                  (stats.fullMessages + stats.compressedMessages + stats.differentialMessages) * 100;
        std::cout << "Optimization ratio: " << std::fixed << std::setprecision(1)
                  << compressionRatio << "%" << std::endl;
    }

    std::cout << "\n=== Neighbor Stability ===" << std::endl;
    auto neighbors = lsm->getActiveNeighbors();
    for (const auto &neighbor : neighbors)
    {
        int interval = lsm->getAdaptiveHelloInterval(neighbor);
        bool stable = lsm->isNeighborStable(neighbor);
        std::cout << "  " << neighbor << ": interval=" << interval
                  << "s, stable=" << (stable ? "Yes" : "No") << std::endl;
    }
    std::cout << "=======================================" << std::endl;
}

void RoutingDaemon::resetOptimizationStats()
{
    if (pm)
    {
        pm->resetOptimizationCache();
    }
}