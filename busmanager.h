#ifndef BUS_MANAGER_H
#define BUS_MANAGER_H

#include <vector>
#include <string>
#include "bus.h"

// ---------------------------------------------------------------------------
// Class: BusManager
// Demonstrates: vector<Bus>, linear search, simple sorting, file handling
// ---------------------------------------------------------------------------
class BusManager {
private:
    std::vector<Bus> buses;
    int findIndexByID(int id) const;  // private helper: linear search

public:
    bool addBus(const Bus& bus);
    bool removeBus(int id);
    bool updateBus(int id, const Bus& updatedBus);
    Bus* searchBus(int id);                // linear search, returns nullptr if not found
    void displayAllBuses() const;

    bool checkCapacity(int id) const;      // true if bus has free seats
    int getAvailableSeats(int id) const;
    void displayAvailableBuses() const;
    bool changeAvailability(int id, bool status);

    void sortBusesByCapacity();            // descending capacity
    void sortBusesByID();                  // ascending ID (uses Bus::operator<)

    void saveToFile(const std::string& filename = "data/buses.dat") const;
    void loadFromFile(const std::string& filename = "data/buses.dat");

    std::vector<Bus>& getBuses();          // exposes the list to Allocator (Module 4)
    bool busIDExists(int id) const;
};

#endif
 
