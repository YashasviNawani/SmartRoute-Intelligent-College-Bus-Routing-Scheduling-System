#include "busStop.h"
#include <iostream>

BusStop::BusStop() : stopID(0), stopName("") {}
BusStop::BusStop(int stopID, const std::string& stopName) : stopID(stopID), stopName(stopName) {}

int BusStop::getStopID() const { return stopID; }
std::string BusStop::getStopName() const { return stopName; }
void BusStop::setStopName(const std::string& name) { stopName = name; }

void BusStop::display() const {
    std::cout << "Stop #" << stopID << " | " << stopName << std::endl;
}
