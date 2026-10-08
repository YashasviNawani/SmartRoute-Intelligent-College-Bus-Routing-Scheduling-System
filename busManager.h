#ifndef BUS_H
#define BUS_H

#include <string>
class Bus {
private:
    int id;
    std::string registrationNo;
    std::string driverName;
    int capacity;
    int currentStudents;
    bool available;

public:
    Bus();  // default constructor
    Bus(int id, const std::string& registrationNo, const std::string& driverName,
        int capacity, int currentStudents = 0, bool available = true);  // parameterized

    // ---- Getters ----
    int getID() const;
    std::string getRegistrationNo() const;
    std::string getDriverName() const;
    int getCapacity() const;
    int getCurrentStudents() const;
    bool isAvailable() const;
    int getAvailableSeats() const;

    // ---- Setters ----
    void setRegistrationNo(const std::string& reg);
    void setDriverName(const std::string& name);
    void setCapacity(int cap);
    void setCurrentStudents(int count);
    void setAvailable(bool status);

    // ---- Behavior ----
    bool addStudents(int count);   // returns false if it would overcrowd the bus
    void removeStudents(int count);

    // Operator overloading: lets std::sort compare Bus objects directly by ID
    bool operator<(const Bus& other) const;

    void display() const;
};

#endif
