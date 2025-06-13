#include "PacketManager.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

void PacketManager::sendHello(const std::string& destIp, int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return;
    }

    std::string message = "HELLO from router";

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

    char buffer[1024];
    while (true) {
        sockaddr_in sender{};
        socklen_t senderLen = sizeof(sender);

        ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&sender, &senderLen);
        if (len > 0) {
            buffer[len] = '\0';
            char senderIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender.sin_addr, senderIp, INET_ADDRSTRLEN);
            std::cout << "Received from " << senderIp << ": " << buffer << "\n";
        }
    }

    close(sock);
}
