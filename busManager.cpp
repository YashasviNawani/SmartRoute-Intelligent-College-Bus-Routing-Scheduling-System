#include "busManager.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

int BusManager::findIndexByID(int id) const {
    // Linear search - O(n), intentional DSA demo point for Module 1
    for (size_t i = 0; i < buses.size(); i++) {
        if (buses[i].getID() == id) return static_cast<int>(i);
    }
    return -1;
}

bool BusManager::busIDExists(int id) const {
    return findIndexByID(id) != -1;
}

bool BusManager::addBus(const Bus& bus) {
    if (busIDExists(bus.getID())) return false;  // input validation: no duplicate IDs
    buses.push_back(bus);
    return true;
}

bool BusManager::removeBus(int id) {
    int idx = findIndexByID(id);
    if (idx == -1) return false;
    buses.erase(buses.begin() + idx);
    return true;
}

bool BusManager::updateBus(int id, const Bus& updatedBus) {
    int idx = findIndexByID(id);
    if (idx == -1) return false;
    buses[idx] = updatedBus;
    return true;
}

Bus* BusManager::searchBus(int id) {
    int idx = findIndexByID(id);
    if (idx == -1) return nullptr;
    return &buses[idx];
}

void BusManager::displayAllBuses() const {
    if (buses.empty()) { std::cout << "No buses in the system.\n"; return; }
    for (const auto& b : buses) b.display();
}

bool BusManager::checkCapacity(int id) const {
    int idx = findIndexByID(id);
    if (idx == -1) return false;
    return buses[idx].getAvailableSeats() > 0;
}

int BusManager::getAvailableSeats(int id) const {
    int idx = findIndexByID(id);
    if (idx == -1) return -1;
    return buses[idx].getAvailableSeats();
}

void BusManager::displayAvailableBuses() const {
    bool any = false;
    for (const auto& b : buses) {
        if (b.isAvailable() && b.getAvailableSeats() > 0) {
            b.display();
            any = true;
        }
    }
    if (!any) std::cout << "No available buses right now.\n";
}

bool BusManager::changeAvailability(int id, bool status) {
    int idx = findIndexByID(id);
    if (idx == -1) return false;
    buses[idx].setAvailable(status);
    return true;
}

void BusManager::sortBusesByCapacity() {
    // Simple sorting demo - descending by capacity
    std::sort(buses.begin(), buses.end(),
              [](const Bus& a, const Bus& b) { return a.getCapacity() > b.getCapacity(); });
}

void BusManager::sortBusesByID() {
    std::sort(buses.begin(), buses.end());  // uses Bus::operator<
}

void BusManager::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) { std::cout << "Could not open " << filename << " for writing.\n"; return; }
    for (const auto& b : buses) {
        out << b.getID() << '|' << b.getRegistrationNo() << '|' << b.getDriverName() << '|'
            << b.getCapacity() << '|' << b.getCurrentStudents() << '|' << b.isAvailable() << '\n';
    }
    out.close();
}

void BusManager::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;  // no saved file yet - not an error on first run
    buses.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, reg, driver, capStr, curStr, availStr;
        std::getline(ss, idStr, '|');
        std::getline(ss, reg, '|');
        std::getline(ss, driver, '|');
        std::getline(ss, capStr, '|');
        std::getline(ss, curStr, '|');
        std::getline(ss, availStr, '|');
        Bus b(std::stoi(idStr), reg, driver, std::stoi(capStr), std::stoi(curStr),
              availStr == "1");
        buses.push_back(b);
    }
    in.close();
}

std::vector<Bus>& BusManager::getBuses() { return buses; }
