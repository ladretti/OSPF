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
#include <zlib.h>
#include "utils.hpp"
#include <bits/this_thread_sleep.h>

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

void PacketManager::sendOptimizedLSA(const std::string &destIp, int port, 
                                    const nlohmann::json& currentLSA, const std::string& hostname)
{
    // Calculer le hash du LSA actuel
    std::string currentData = currentLSA.dump();
    std::string currentHash = computeHMAC(currentData, "lsa_optimization_key");
    
    auto hashIt = lastSentLSAHash.find(destIp);
    bool shouldSendFull = (hashIt == lastSentLSAHash.end() || hashIt->second != currentHash);
    
    if (!shouldSendFull) {
        // LSA identique, pas d'envoi nécessaire
        return;
    }
    
    json messageToSend;
    auto lsaIt = lastSentLSA.find(destIp);
    
    if (lsaIt != lastSentLSA.end()) {
        // Créer un LSA différentiel
        json diffLSA = createDifferentialLSA(lsaIt->second, currentLSA, hostname);
        
        // Vérifier si le différentiel est plus petit que le LSA complet
        std::string diffStr = diffLSA.dump();
        std::string fullStr = currentLSA.dump();
        
        if (diffStr.size() < fullStr.size() * 0.7) { // 30% d'économie minimum
            messageToSend = diffLSA;
            stats.differentialMessages++;
        } else {
            messageToSend = currentLSA;
            messageToSend["type"] = "LSA_FULL_COMPRESSED";
            stats.fullMessages++;
        }
    } else {
        // Premier envoi, envoyer LSA complet
        messageToSend = currentLSA;
        messageToSend["type"] = "LSA_FULL_COMPRESSED";
        stats.fullMessages++;
    }
    
    // Compression des données si elles sont importantes
    std::string jsonStr = messageToSend.dump();
    if (jsonStr.size() > 500) { // Comprimer si > 500 bytes
        std::string compressed = compressData(jsonStr);
        if (compressed.size() < jsonStr.size() * 0.8) { // 20% d'économie minimum
            messageToSend = json{
                {"type", "LSA_COMPRESSED"},
                {"compressed_data", compressed},
                {"hostname", hostname}
            };
            stats.compressedMessages++;
        }
    }
    
    // Envoyer le message optimisé
    sendLSA(destIp, port, messageToSend);
    
    // Mettre à jour le cache
    lastSentLSAHash[destIp] = currentHash;
    lastSentLSA[destIp] = currentLSA;
    stats.totalBytesSent += messageToSend.dump().size();
}

json PacketManager::createDifferentialLSA(const json& oldLSA, const json& newLSA, 
                                         const std::string& hostname)
{
    json diffLSA = {
        {"type", "LSA_DIFFERENTIAL"},
        {"hostname", hostname},
        {"sequence", newLSA.value("sequence", 1)},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"changes", json::object()}
    };
    
    // Comparer les voisins
    if (oldLSA.contains("neighbors") && newLSA.contains("neighbors")) {
        if (oldLSA["neighbors"] != newLSA["neighbors"]) {
            // Calculer les ajouts et suppressions
            auto oldNeighbors = oldLSA["neighbors"];
            auto newNeighbors = newLSA["neighbors"];
            
            json added = json::array();
            json removed = json::array();
            
            // Trouver les nouveaux voisins
            for (const auto& neighbor : newNeighbors) {
                bool found = false;
                for (const auto& oldN : oldNeighbors) {
                    if (neighbor == oldN) {
                        found = true;
                        break;
                    }
                }
                if (!found) added.push_back(neighbor);
            }
            
            // Trouver les voisins supprimés
            for (const auto& oldN : oldNeighbors) {
                bool found = false;
                for (const auto& neighbor : newNeighbors) {
                    if (neighbor == oldN) {
                        found = true;
                        break;
                    }
                }
                if (!found) removed.push_back(oldN);
            }
            
            if (!added.empty() || !removed.empty()) {
                diffLSA["changes"]["neighbors"] = {
                    {"added", added},
                    {"removed", removed}
                };
            }
        }
    } else if (newLSA.contains("neighbors")) {
        diffLSA["changes"]["neighbors"] = {
            {"added", newLSA["neighbors"]},
            {"removed", json::array()}
        };
    }
    
    // Comparer les interfaces
    if (oldLSA.contains("interfaces") && newLSA.contains("interfaces")) {
        if (oldLSA["interfaces"] != newLSA["interfaces"]) {
            diffLSA["changes"]["interfaces"] = newLSA["interfaces"];
        }
    } else if (newLSA.contains("interfaces")) {
        diffLSA["changes"]["interfaces"] = newLSA["interfaces"];
    }
    
    // Comparer les capacités de liens
    if (oldLSA.contains("link_capacities") && newLSA.contains("link_capacities")) {
        if (oldLSA["link_capacities"] != newLSA["link_capacities"]) {
            diffLSA["changes"]["link_capacities"] = newLSA["link_capacities"];
        }
    } else if (newLSA.contains("link_capacities")) {
        diffLSA["changes"]["link_capacities"] = newLSA["link_capacities"];
    }
    
    return diffLSA;
}

std::string PacketManager::compressData(const std::string& data)
{
    uLongf compressedSize = compressBound(data.size());
    std::vector<Bytef> compressed(compressedSize);
    
    int result = compress(compressed.data(), &compressedSize,
                         reinterpret_cast<const Bytef*>(data.c_str()), data.size());
    
    if (result == Z_OK) {
        compressed.resize(compressedSize);
        // Encoder en base64 pour transmission JSON
        return toHex(std::string(compressed.begin(), compressed.end()));
    }
    
    return data; // Retourner non-compressé en cas d'erreur
}

std::string PacketManager::decompressData(const std::string& compressedHex)
{
    std::vector<Bytef> compressed;
    for (size_t i = 0; i < compressedHex.length(); i += 2) {
        unsigned int byte;
        std::istringstream(compressedHex.substr(i, 2)) >> std::hex >> byte;
        compressed.push_back(static_cast<Bytef>(byte));
    }
    
    uLongf decompressedSize = compressed.size() * 4; // Estimation
    std::vector<Bytef> decompressed(decompressedSize);
    
    int result = uncompress(decompressed.data(), &decompressedSize,
                           compressed.data(), compressed.size());
    
    if (result == Z_OK) {
        decompressed.resize(decompressedSize);
        return std::string(decompressed.begin(), decompressed.end());
    }
    
    return ""; // Erreur de décompression
}

bool PacketManager::shouldSendHello(const std::string& neighborIp, int adaptiveInterval)
{
    auto now = std::chrono::steady_clock::now();
    auto it = lastHelloSent.find(neighborIp);
    
    if (it == lastHelloSent.end()) {
        lastHelloSent[neighborIp] = now;
        return true; // Premier Hello
    }
    
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
    if (elapsed >= adaptiveInterval) {
        lastHelloSent[neighborIp] = now;
        return true;
    }
    
    return false;
}

void PacketManager::resetOptimizationCache()
{
    lastSentLSAHash.clear();
    lastSentLSA.clear();
    lastHelloSent.clear();
}