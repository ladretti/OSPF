#include "PacketManager.hpp"
#include "../include/json.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using json = nlohmann::json;

void PacketManager::sendHello(const std::string& destIp, int port,
                              const std::string& hostname,
                              const std::vector<std::string>& interfaces) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    // Créer le message JSON
    json j;
    j["type"] = "HELLO";
    j["ip"] = destIp; // ou l'IP locale à déterminer dynamiquement
    j["hostname"] = hostname;
    j["interfaces"] = interfaces;

    std::string message = j.dump();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, destIp.c_str(), &addr.sin_addr);

    int sent = sendto(sock, message.c_str(), message.size(), 0,
                      (sockaddr*)&addr, sizeof(addr));
    if (sent < 0) {
        perror("sendto");
    } else {
        std::cout << "Sent HELLO to " << destIp << ":" << port << std::endl;
    }

    close(sock);
}

void PacketManager::receivePackets(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return;
    }

    std::cout << "Listening for packets on UDP port " << port << "...\n";

    char buffer[2048];
    while (true) {
        
        sockaddr_in sender{};
        socklen_t senderLen = sizeof(sender);

        ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&sender, &senderLen);
        if (len > 0) {
            buffer[len] = '\0';

            try {
                json j = json::parse(buffer);
                std::cout << "Received JSON:\n" << j.dump(4) << "\n";

                if (j.contains("type") && j["type"] == "HELLO") {
                    char senderIp[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sender.sin_addr, senderIp, INET_ADDRSTRLEN);

                    // Envoyer un HELLO de retour
                    std::string hostname = "R_2";  // <- adapte si tu es sur une autre VM
                    std::vector<std::string> myInterfaces = {"192.168.2.1", "10.1.0.2"};

                    PacketManager replyManager;
                    replyManager.sendHello(senderIp, 5000, hostname, myInterfaces);
                }

            } catch (...) {
                std::cout << "Received invalid JSON: " << buffer << "\n";
            }
        }
    }

    close(sock);
}
