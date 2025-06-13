#pragma once
#include <string>
#include <vector>
#include <map>

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