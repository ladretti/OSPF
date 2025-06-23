#pragma once
#include <string>
#include <vector>
#include <map>
#include <openssl/hmac.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>


struct RouterConfig {
    std::string hostname;
    std::vector<std::string> interfaces;
    std::vector<std::string> interfacesNames;
    int port;
};

std::map<std::string, RouterConfig> parseRouterConfig(const std::string& configFile);

RouterConfig getRouterConfig(const std::string& routerId, const std::string& configFile);

std::vector<std::string> split(const std::string& str, char delimiter);




inline std::string computeHMAC(const std::string &data, const std::string &key) {
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), result, &len);
    return std::string(reinterpret_cast<char*>(result), len);
}

inline std::string toHex(const std::string &input) {
    std::ostringstream oss;
    for (unsigned char c : input) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

inline void addRoute(const std::string& network, const std::string& via, const std::string& iface) {
    std::string cmd = "ip route replace " + network + " via " + via + " dev " + iface;
    std::cout << cmd << std::endl;
    // std::system(cmd.c_str());
}
