#pragma once
#include <string>
#include <vector>
#include <map>
#include <openssl/hmac.h>
#include <string>
#include <sstream>
#include <iomanip>

struct RouterConfig {
    std::string hostname;
    std::vector<std::string> interfaces;
    int port;
};

// Parse the router config file and return all router configurations
std::map<std::string, RouterConfig> parseRouterConfig(const std::string& configFile);

// Get a specific router's configuration
RouterConfig getRouterConfig(const std::string& routerId, const std::string& configFile);

// Split a string by delimiter
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