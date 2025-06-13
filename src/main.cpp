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
        pm.sendHello("10.1.0.2");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
