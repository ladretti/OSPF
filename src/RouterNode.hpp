// RouterNode.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct Interface {
    std::string ip;
    bool active;
    int capacity;
};

class RouterNode {
public:
    RouterNode(const std::string& name);
    void addInterface(const Interface& iface);
    void printInterfaces() const;
    std::vector<Interface> getActiveInterfaces() const;
    std::string getName() const;

private:
    std::string name;
    std::vector<Interface> interfaces;
};
