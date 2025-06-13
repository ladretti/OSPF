// RouterNode.cpp
#include "RouterNode.hpp"
#include <iostream>

RouterNode::RouterNode(const std::string& name) : name(name) {}

void RouterNode::addInterface(const Interface& iface) {
    interfaces.push_back(iface);
}

void RouterNode::printInterfaces() const {
    for (const auto& iface : interfaces) {
        std::cout << iface.ip << " [" << (iface.active ? "UP" : "DOWN") << "]\n";
    }
}

std::vector<Interface> RouterNode::getActiveInterfaces() const {
    std::vector<Interface> active;
    for (const auto& iface : interfaces) {
        if (iface.active) active.push_back(iface);
    }
    return active;
}

std::string RouterNode::getName() const {
    return name;
}
