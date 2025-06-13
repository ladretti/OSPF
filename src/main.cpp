#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include <iostream>
#include <bits/this_thread_sleep.h>

int main()
{
    PacketManager pm;
    while (true)
    {
        std::vector<std::string> interfaces = {"192.168.1.1", "10.1.0.1"};
        pm.sendHello("10.1.0.2", 5000, "R_1", interfaces);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
