#include "RoutingCLI.hpp"
#include <iostream>
#include <sstream>

RoutingCLI::RoutingCLI(const std::string &configFile)
{
    try
    {
        daemon = std::make_unique<RoutingDaemon>(configFile);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error initializing daemon: " << e.what() << std::endl;
        throw;
    }
}

void RoutingCLI::run()
{
    std::cout << "Routing Protocol CLI" << std::endl;
    std::cout << "Type 'help' for available commands" << std::endl;

    std::string line;
    while (true)
    {
        std::cout << "routing> ";
        if (!std::getline(std::cin, line))
        {
            break;
        }

        if (line.empty())
            continue;

        if (line == "quit" || line == "exit")
        {
            break;
        }

        handleCommand(line);
    }

    if (daemon && daemon->isRunning())
    {
        std::cout << "Stopping daemon..." << std::endl;
        daemon->stop();
    }
}

void RoutingCLI::handleCommand(const std::string &command)
{
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "help")
    {
        printHelp();
    }
    else if (cmd == "start")
    {
        if (daemon->start())
        {
            std::cout << "Daemon started successfully" << std::endl;
        }
        else
        {
            std::cout << "Daemon is already running" << std::endl;
        }
    }
    else if (cmd == "stop")
    {
        if (daemon->isRunning())
        {
            daemon->stop();
            std::cout << "Daemon stopped" << std::endl;
        }
        else
        {
            std::cout << "Daemon is not running" << std::endl;
        }
    }
    else if (cmd == "ping")
    {
        std::string target;
        int count = 4;

        if (iss >> target)
        {
            // Optionnel : lire le nombre de pings
            std::string countStr;
            if (iss >> countStr)
            {
                try
                {
                    count = std::stoi(countStr);
                    if (count <= 0 || count > 100)
                    {
                        std::cout << "Count must be between 1 and 100" << std::endl;
                        return;
                    }
                }
                catch (const std::exception &)
                {
                    std::cout << "Invalid count value" << std::endl;
                    return;
                }
            }

            daemon->showPingResults(target, count);
        }
        else if (cmd == "request")
        {
            std::string targetIp;
            if (iss >> targetIp)
            {
                daemon->requestNeighborsFrom(targetIp);
            }
            else
            {
                std::cout << "Usage: request <target_ip>" << std::endl;
                std::cout << "Example: request 192.168.1.1" << std::endl;
            }
        }
        else
        {
            std::cout << "Usage: ping <target> [count]" << std::endl;
            std::cout << "Example: ping 192.168.1.1 4" << std::endl;
        }
    }
    else if (cmd == "pingall")
    {
        if (!daemon->isRunning())
        {
            std::cout << "Daemon must be running to ping neighbors" << std::endl;
            return;
        }

        auto neighbors = daemon->getActiveNeighbors();
        if (neighbors.empty())
        {
            std::cout << "No active neighbors to ping" << std::endl;
            return;
        }

        std::cout << "Pinging all active neighbors..." << std::endl;
        for (const auto &neighbor : neighbors)
        {
            std::cout << "\n--- Pinging " << neighbor << " ---" << std::endl;
            daemon->showPingResults(neighbor, 3);
        }
    }
    else if (cmd == "status")
    {
        daemon->getStatus();
    }
    else if (cmd == "neighbors")
    {
        auto neighbors = daemon->getActiveNeighbors();
        std::cout << "Active neighbors (" << neighbors.size() << "):" << std::endl;
        for (const auto &neighbor : neighbors)
        {
            std::cout << "  - " << neighbor << std::endl;
        }
    }
    else
    {
        std::cout << "Unknown command: " << cmd << std::endl;
        std::cout << "Type 'help' for available commands" << std::endl;
    }
}

void RoutingCLI::printHelp() {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  start       - Start the routing daemon" << std::endl;
    std::cout << "  stop        - Stop the routing daemon" << std::endl;
    std::cout << "  status      - Show daemon status and configuration" << std::endl;
    std::cout << "  neighbors   - List active neighbor routers" << std::endl;
    std::cout << "  request <ip> - Request neighbor list from specific router" << std::endl;
    std::cout << "  ping <ip>   - Ping a specific IP address" << std::endl;
    std::cout << "  ping <ip> <count> - Ping with custom packet count" << std::endl;
    std::cout << "  pingall     - Ping all active neighbors" << std::endl;
    std::cout << "  help        - Show this help message" << std::endl;
    std::cout << "  quit/exit   - Exit the CLI" << std::endl;
}