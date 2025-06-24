#pragma once
#include <string>
#include <vector>
#include <map>
#include <openssl/hmac.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <iostream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct RouterConfig
{
    std::string hostname;
    std::vector<std::string> interfaces;
    std::vector<std::string> interfacesNames;
    int port;
};

std::map<std::string, RouterConfig> parseRouterConfig(const std::string &configFile);

RouterConfig getRouterConfig(const std::string &routerId, const std::string &configFile);

std::vector<std::string> split(const std::string &str, char delimiter);

inline std::string computeHMAC(const std::string &data, const std::string &key)
{
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char *>(data.data()), data.size(), result, &len);
    return std::string(reinterpret_cast<char *>(result), len);
}

inline std::string toHex(const std::string &input)
{
    std::ostringstream oss;
    for (unsigned char c : input)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

inline void addRoute(const std::string &network, const std::string &via, const std::string &iface)
{
    std::string cmd = "ip route replace " + network + " via " + via + " dev " + iface;
    std::cout << "Executing command: " << cmd << std::endl;
    std::system(cmd.c_str());
}

inline std::vector<std::pair<std::string, std::string>> getLocalIpInterfaceMapping()
{
    std::vector<std::pair<std::string, std::string>> result;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1)
        return result;

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
        {
            char ip[INET_ADDRSTRLEN];
            void *addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr, ip, INET_ADDRSTRLEN);
            result.emplace_back(ip, ifa->ifa_name);
        }
    }
    freeifaddrs(ifaddr);
    return result;
}
