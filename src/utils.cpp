#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

std::vector<std::string> split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::map<std::string, RouterConfig> parseRouterConfig(const std::string &configFile)
{
    std::map<std::string, RouterConfig> configs;
    std::ifstream file(configFile);
    std::string line;
    std::string currentSection;

    RouterConfig currentConfig;

    while (std::getline(file, line))
    {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#' || (line.size() > 1 && line[0] == '/' && line[1] == '/'))
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            currentSection = line.substr(1, line.size() - 2);
            configs[currentSection] = RouterConfig();
            continue;
        }
        if (line.empty() || line.substr(0, 2) == "//")
        {
            continue;
        }
        // Check for section header [RouterID]
        if (line[0] == '[' && line.back() == ']')
        {
            // Save previous section if exists
            if (!currentSection.empty())
            {
                configs[currentSection] = currentConfig;
            }

            // Start new section
            currentSection = line.substr(1, line.size() - 2);
            currentConfig = RouterConfig();
            continue;
        }

        // Parse key=value pairs
        size_t equalsPos = line.find('=');
        if (equalsPos != std::string::npos)
        {
            std::string key = line.substr(0, equalsPos);
            std::string value = line.substr(equalsPos + 1);

            if (key == "hostname")
            {
                currentConfig.hostname = value;
            }
            else if (key == "interfaces")
            {
                currentConfig.interfaces = split(value, ',');
            }
            else if (key == "interfacesNames")
            {
                currentConfig.interfacesNames = split(value, ',');
            }
            else if (key == "port")
            {
                currentConfig.port = std::stoi(value);
            }
        }
    }

    // Save the last section
    if (!currentSection.empty())
    {
        configs[currentSection] = currentConfig;
    }

    return configs;
}

RouterConfig getRouterConfig(const std::string &routerId, const std::string &configFile)
{
    auto configs = parseRouterConfig(configFile);

    if (configs.find(routerId) != configs.end())
    {
        return configs[routerId];
    }

    // Return empty config if not found
    std::cerr << "Router ID " << routerId << " not found in config file." << std::endl;
    return RouterConfig{};
}