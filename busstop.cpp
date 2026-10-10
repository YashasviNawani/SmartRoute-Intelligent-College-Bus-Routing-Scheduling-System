#include "busstop.h"
#include <iostream>

BusStop::BusStop() : id(0), name("") {}

BusStop::BusStop(int id, const std::string& name) : id(id), name(name) {}

int BusStop::getID() const { return id; }
std::string BusStop::getName() const { return name; }
void BusStop::setName(const std::string& n) { name = n; }

void BusStop::display() const {
    std::cout << "Stop ID: " << id << " | Name: " << name << "\n";
}
