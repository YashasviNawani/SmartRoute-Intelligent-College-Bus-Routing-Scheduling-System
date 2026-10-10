#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "busmanager.h"
#include "studentmanager.h"
#include "routemanager.h"

// ---------------------------------------------------------------------------
// Class: Allocator   (Module 4: Capacity & Bus Allocation)
// Demonstrates: queue (FIFO allocation), priority_queue (route demand),
//               hashing (busID -> routeID, routeID -> demand),
//               class interaction (Bus, Student, Route managers)
// ---------------------------------------------------------------------------
class Allocator {
private:
    BusManager& busManager;
    StudentManager& studentManager;
    RouteManager& routeManager;

    std::queue<int> allocationQueue;                // student IDs waiting to be processed (FIFO)
    std::unordered_set<int> queuedIDs;              // prevents a student being queued twice
    std::vector<int> waitingList;                   // students who could not be seated (excess demand)
    std::unordered_map<int, int> busToRoute;        // busID   -> routeID
    std::unordered_map<int, int> routeDemand;       // routeID -> number of students needing it

    int firstRouteWithStop(int stopID, const std::vector<Route>& routes) const;
    int findRouteForStop(int stopID, const std::vector<Route>& routes);   // prefers a route that has a free seat
    int routeFreeSeats(int routeID);
    void recomputeDemand();

public:
    Allocator(BusManager& bm, StudentManager& sm, RouteManager& rm);

    // ---- Bus <-> route assignment ----
    bool assignBusToRoute(int busID, int routeID);
    bool unassignBus(int busID);
    int getRouteOfBus(int busID) const;               // -1 if the bus is not assigned
    Bus* findAvailableBus(int routeID);               // available bus on route with most free seats

    // ---- Capacity ----
    bool checkCapacity(int busID) const;              // true if the bus has a free seat
    bool wouldOvercrowd(int busID, int count) const;  // true if 'count' students do not fit
    bool updateCapacity(int busID, int newCapacity);  // refuses a capacity below current load

    // ---- Queue-based allocation ----
    bool addStudentToQueue(int studentID);
    int queueAllUnallocatedStudents();                // queues every student with a stop but no route
    int processAllocationQueue();                     // returns number of students seated
    bool releaseStudent(int studentID);               // frees a seat and clears the student's route

    // ---- Excess demand ----
    void displayWaitingList() const;
    int retryWaitingList();                           // re-queue waiting students and process again
    int getWaitingCount() const;

    // ---- Priority queue + hashing ----
    std::vector<int> prioritizeRoutes();              // route IDs, highest demand first
    int getRouteDemand(int routeID) const;

    void displayAllocationReport();

    // ---- File handling (busID|routeID) ----
    void saveToFile(const std::string& filename = "data/allocations.dat") const;
    void loadFromFile(const std::string& filename = "data/allocations.dat");
};

#endif
