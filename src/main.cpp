#include "RoutingCLI.hpp"
#include <iostream>

using json = nlohmann::json;

int main(int argc, char *argv[])
{
    const std::string configFile = "config/router.conf";
    
    try {
        RoutingCLI cli(configFile);
        cli.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}