#pragma once
#include "RoutingDaemon.hpp"
#include <memory>
#include <string>

class RoutingCLI {
public:
    RoutingCLI(const std::string& configFile);
    void run();
    
private:
    void printHelp();
    void handleCommand(const std::string& command);
    
    std::unique_ptr<RoutingDaemon> daemon;
};