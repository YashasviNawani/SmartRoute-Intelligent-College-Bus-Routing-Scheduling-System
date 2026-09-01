#ifndef BUSMANAGER_H
#define BUSMANAGER_H
struct Bus
{
    int id;
    char registrationNo[20];
    char driverName[30];
    int capacity;
    int currentStudents;
    int available;
};
void addBus();
void removeBus();
void updateBus();
void displayAllBuses();
#endif