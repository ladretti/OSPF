#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include <iostream>
#include <bits/this_thread_sleep.h>

int main()
{
    PacketManager pm;
    pm.receivePackets(); // écoute en boucle sur le port 5000
    return 0;
}
