#include "RouterNode.hpp"
#include "LinkStateManager.hpp"
#include "RoutingProtocol.hpp"
#include "PacketManager.hpp"
#include <iostream>

int main() {
    RouterNode router("R1");
    router.addInterface({"10.1.0.1", true, 100});
    router.addInterface({"192.168.1.1", true, 1000});

    router.printInterfaces();

    LinkStateManager lsm;
    lsm.updateNeighbor("10.1.0.2");

    RoutingProtocol proto;
    proto.computeRoutes();

    auto table = proto.getRoutingTable();
    for (const auto& [dest, nextHop] : table) {
        std::cout << "Route to " << dest << " via " << nextHop << "\n";
    }

    PacketManager pm;
    pm.sendHello("10.1.0.2");

    return 0;
}
