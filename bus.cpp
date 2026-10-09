#include "bus.h"
#include <iostream>

Bus::Bus()
    : id(0), registrationNo(""), driverName(""), capacity(0),
      currentStudents(0), available(true) {}

Bus::Bus(int id, const std::string& registrationNo, const std::string& driverName,
         int capacity, int currentStudents, bool available)
    : id(id), registrationNo(registrationNo), driverName(driverName),
      capacity(capacity), currentStudents(currentStudents), available(available) {}

// ---- Getters ----
int Bus::getID() const { return id; }
std::string Bus::getRegistrationNo() const { return registrationNo; }
std::string Bus::getDriverName() const { return driverName; }
int Bus::getCapacity() const { return capacity; }
int Bus::getCurrentStudents() const { return currentStudents; }
bool Bus::isAvailable() const { return available; }
int Bus::getAvailableSeats() const { return capacity - currentStudents; }

// ---- Setters ----
void Bus::setRegistrationNo(const std::string& reg) { registrationNo = reg; }
void Bus::setDriverName(const std::string& name) { driverName = name; }
void Bus::setCapacity(int cap) { capacity = cap; }
void Bus::setCurrentStudents(int count) { currentStudents = count; }
void Bus::setAvailable(bool status) { available = status; }

// ---- Behavior ----
bool Bus::addStudents(int count) {
    if (currentStudents + count > capacity) return false;  // prevents overcrowding
    currentStudents += count;
    return true;
}

void Bus::removeStudents(int count) {
    currentStudents -= count;
    if (currentStudents < 0) currentStudents = 0;
}

bool Bus::operator<(const Bus& other) const {
    return id < other.id;
}

void Bus::display() const {
    std::cout << "Bus #" << id
              << " | Reg: " << registrationNo
              << " | Driver: " << driverName
              << " | Capacity: " << capacity
              << " | Occupied: " << currentStudents
              << " | Free: " << getAvailableSeats()
              << " | " << (available ? "AVAILABLE" : "UNAVAILABLE")
              << std::endl;
}
