#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include <iostream>

int main()
{
    PacketManager pm;
    pm.sendHello("10.1.0.2"); // envoie à R_2
    return 0;
}
