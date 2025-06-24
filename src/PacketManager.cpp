#include "PacketManager.hpp"
#include "../include/json.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "LinkStateManager.hpp"
#include <fcntl.h>
#include <atomic>
#include <bits/this_thread_sleep.h>
#include "utils.hpp"

using json = nlohmann::json;

void PacketManager::sendHello(const std::string &destIp, int port,
                              const std::string &hostname,
                              const std::vector<std::string> &interfaces)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0)
    {
        perror("Error: setsockopt SO_BROADCAST");
        close(sock);
        return;
    }

    // Le reste de votre code d'envoi existant
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, destIp.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address: " << destIp << std::endl;
        close(sock);
        return;
    }

    json helloMsg = {
        {"type", "HELLO"},
        {"hostname", hostname},
        {"interfaces", interfaces}};
    std::string helloStr = helloMsg.dump();
    std::string hmac = computeHMAC(helloStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
    helloMsg["hmac"] = toHex(hmac);

    if (sendto(sock, helloMsg.dump().c_str(), helloMsg.dump().length(), 0,
               (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("sendto");
    }

    close(sock);
}

void PacketManager::receivePackets(int port, LinkStateManager &lsm, std::atomic<bool> &running, const std::string &hostname, TopologyDatabase &topoDb)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    // Set socket as non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sock);
        return;
    }

    char buffer[2048];
    while (running)
    {

        sockaddr_in sender{};
        socklen_t senderLen = sizeof(sender);

        ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr *)&sender, &senderLen);

        if (len > 0)
        {
            buffer[len] = '\0';

            try
            {
                json j = json::parse(buffer);
                if (j.contains("hmac"))
                {
                    std::string receivedHmac = j["hmac"];
                    j.erase("hmac");
                    std::string computedHmac = computeHMAC(j.dump(), "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
                    if (receivedHmac != toHex(computedHmac))
                    {
                        std::cerr << "HMAC verification failed! Packet dropped." << std::endl;
                        continue;
                    }
                }
                else
                {
                    std::cerr << "No HMAC found! Packet dropped." << std::endl;
                    continue;
                }
                if (j.contains("hostname") && j["hostname"] == hostname)
                {
                    continue;
                }

                if (j.contains("type") && j["type"] == "HELLO")
                {
                    char senderIp[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender.sin_addr, senderIp, INET_ADDRSTRLEN);

                    std::string neighborHostname = j.value("hostname", "");
                    lsm.updateNeighbor(senderIp, neighborHostname);
                }
                if (j.contains("type") && j["type"] == "LSA")
                {
                    char senderIp[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender.sin_addr, senderIp, INET_ADDRSTRLEN);

                    bool updated = topoDb.updateLSA(j);

                    if (updated)
                    {
                        // RELAY TO ALL ACTIVE NEIGHBORS (except sender)
                        auto neighbors = lsm.getActiveNeighbors();
                        for (const auto &neighborIp : neighbors)
                        {
                            if (neighborIp != senderIp)
                            {
                                sendLSA(neighborIp, port, j);
                            }
                        }
                    }
                }
                if (j.contains("type") && j["type"] == "NEIGHBOR_REQUEST")
                {
                    char senderIp[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender.sin_addr, senderIp, INET_ADDRSTRLEN);

                    auto neighborIps = lsm.getActiveNeighbors();
                    auto neighborHostnames = lsm.getActiveNeighborHostnames();

                    json neighbors = json::array();
                    for (size_t i = 0; i < std::min(neighborIps.size(), neighborHostnames.size()); ++i)
                    {
                        neighbors.push_back({{"ip", neighborIps[i]},
                                             {"hostname", neighborHostnames[i]}});
                    }

                    json responseMsg = {
                        {"type", "NEIGHBOR_RESPONSE"},
                        {"hostname", hostname},
                        {"neighbors", neighbors}};

                    std::string responseStr = responseMsg.dump();
                    std::string hmac = computeHMAC(responseStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
                    responseMsg["hmac"] = toHex(hmac);

                    int sock = socket(AF_INET, SOCK_DGRAM, 0);
                    if (sock >= 0)
                    {
                        sockaddr_in addr{};
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(port);

                        if (inet_pton(AF_INET, senderIp, &addr.sin_addr) > 0)
                        {
                            sendto(sock, responseMsg.dump().c_str(), responseMsg.dump().length(), 0,
                                   (sockaddr *)&addr, sizeof(addr));
                        }
                        close(sock);
                    }
                }

                if (j.contains("type") && j["type"] == "NEIGHBOR_RESPONSE")
                {
                    std::string senderHostname = j.value("hostname", "unknown");
                    std::cout << "\n=== Neighbors of " << senderHostname << " ===" << std::endl;

                    if (j.contains("neighbors") && j["neighbors"].is_array())
                    {
                        auto neighbors = j["neighbors"];
                        std::cout << "Active neighbors (" << neighbors.size() << "):" << std::endl;

                        for (const auto &neighbor : neighbors)
                        {
                            if (neighbor.contains("hostname") && neighbor.contains("ip"))
                            {
                                std::cout << "  - " << neighbor["hostname"].get<std::string>()
                                          << " (" << neighbor["ip"].get<std::string>() << ")" << std::endl;
                            }
                            else if (neighbor.is_string())
                            {
                                // Compatibilité avec l'ancien format
                                std::cout << "  - " << neighbor.get<std::string>() << std::endl;
                            }
                        }
                    }
                    else
                    {
                        std::cout << "No active neighbors" << std::endl;
                    }
                    std::cout << "=========================" << std::endl;
                }
            }
            catch (...)
            {
            }
        }
        else if (len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    close(sock);
}

void PacketManager::sendLSA(const std::string &destIp, int port, const json &lsaMsg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, destIp.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address: " << destIp << std::endl;
        close(sock);
        return;
    }

    json lsaToSend = lsaMsg;
    lsaToSend.erase("hmac");
    std::string lsaStr = lsaToSend.dump();
    std::string hmac = computeHMAC(lsaStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
    lsaToSend["hmac"] = toHex(hmac);

    if (sendto(sock, lsaToSend.dump().c_str(), lsaToSend.dump().length(), 0,
               (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("sendto");
    }
    close(sock);
}

void PacketManager::sendNeighborRequest(const std::string &destIp, int port, const std::string &hostname)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, destIp.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address: " << destIp << std::endl;
        close(sock);
        return;
    }

    json requestMsg = {
        {"type", "NEIGHBOR_REQUEST"},
        {"hostname", hostname}};

    std::string requestStr = requestMsg.dump();
    std::string hmac = computeHMAC(requestStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
    requestMsg["hmac"] = toHex(hmac);

    if (sendto(sock, requestMsg.dump().c_str(), requestMsg.dump().length(), 0,
               (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("sendto");
    }
    close(sock);
}

void PacketManager::sendNeighborResponse(const std::string &destIp, int port, const std::string &hostname, const std::vector<std::string> &neighbors)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, destIp.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address: " << destIp << std::endl;
        close(sock);
        return;
    }

    json responseMsg = {
        {"type", "NEIGHBOR_RESPONSE"},
        {"hostname", hostname},
        {"neighbors", neighbors}};

    std::string responseStr = responseMsg.dump();
    std::string hmac = computeHMAC(responseStr, "rreNofDO7Bdd9xObfMAbC1pDOhpRR9BX7FTk512YV");
    responseMsg["hmac"] = toHex(hmac);

    if (sendto(sock, responseMsg.dump().c_str(), responseMsg.dump().length(), 0,
               (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("sendto");
    }
    close(sock);
}