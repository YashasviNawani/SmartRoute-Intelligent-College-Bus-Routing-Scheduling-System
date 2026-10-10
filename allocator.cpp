#include "allocator.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

Allocator::Allocator(BusManager& bm, StudentManager& sm, RouteManager& rm)
    : busManager(bm), studentManager(sm), routeManager(rm) {}

// ---------- private helpers ----------

int Allocator::firstRouteWithStop(int stopID, const std::vector<Route>& routes) const {
    for (const auto& r : routes)
        if (r.hasStop(stopID)) return r.getID();
    return -1;
}

// A stop can lie on several routes. Prefer one that still has a free seat;
// otherwise fall back to the first route that contains the stop.
int Allocator::findRouteForStop(int stopID, const std::vector<Route>& routes) {
    int fallback = -1;
    for (const auto& r : routes) {
        if (!r.hasStop(stopID)) continue;
        if (fallback == -1) fallback = r.getID();
        if (findAvailableBus(r.getID()) != nullptr) return r.getID();
    }
    return fallback;
}

int Allocator::routeFreeSeats(int routeID) {
    int total = 0;
    for (Bus& b : busManager.getBuses()) {
        auto it = busToRoute.find(b.getID());
        if (it != busToRoute.end() && it->second == routeID && b.isAvailable())
            total += b.getAvailableSeats();
    }
    return total;
}

void Allocator::recomputeDemand() {
    routeDemand.clear();
    std::vector<Route> routes = routeManager.getAllRoutes();
    for (const auto& s : studentManager.getAllStudents()) {
        if (s.getBusStopID() == -1) continue;
        int r = firstRouteWithStop(s.getBusStopID(), routes);
        if (r != -1) routeDemand[r]++;
    }
}

// ---------- bus <-> route assignment ----------

bool Allocator::assignBusToRoute(int busID, int routeID) {
    if (!busManager.busIDExists(busID) || !routeManager.routeIDExists(routeID)) return false;
    busToRoute[busID] = routeID;
    return true;
}

bool Allocator::unassignBus(int busID) {
    return busToRoute.erase(busID) > 0;
}

int Allocator::getRouteOfBus(int busID) const {
    auto it = busToRoute.find(busID);
    return it == busToRoute.end() ? -1 : it->second;
}

Bus* Allocator::findAvailableBus(int routeID) {
    Bus* best = nullptr;
    for (Bus& b : busManager.getBuses()) {
        auto it = busToRoute.find(b.getID());
        if (it == busToRoute.end() || it->second != routeID) continue;
        if (!b.isAvailable() || b.getAvailableSeats() <= 0) continue;
        // choosing the bus with most free seats spreads students across buses
        if (best == nullptr || b.getAvailableSeats() > best->getAvailableSeats()) best = &b;
    }
    return best;
}

// ---------- capacity ----------

bool Allocator::checkCapacity(int busID) const {
    return busManager.busIDExists(busID) && busManager.getAvailableSeats(busID) > 0;
}

bool Allocator::wouldOvercrowd(int busID, int count) const {
    if (!busManager.busIDExists(busID)) return true;
    return count > busManager.getAvailableSeats(busID);
}

bool Allocator::updateCapacity(int busID, int newCapacity) {
    Bus* b = busManager.searchBus(busID);
    if (b == nullptr || newCapacity <= 0 || newCapacity < b->getCurrentStudents()) return false;
    b->setCapacity(newCapacity);
    return true;
}

// ---------- queue-based allocation ----------

bool Allocator::addStudentToQueue(int studentID) {
    Student* s = studentManager.searchStudent(studentID);
    if (s == nullptr) return false;
    if (s->getBusStopID() == -1) return false;            // no stop chosen yet
    if (s->getAssignedRouteID() != -1) return false;      // already allocated
    if (queuedIDs.count(studentID)) return false;         // already waiting in the queue
    allocationQueue.push(studentID);
    queuedIDs.insert(studentID);
    return true;
}

int Allocator::queueAllUnallocatedStudents() {
    // The hash table returns students in arbitrary order, so sort by ID first
    // to make the FIFO order predictable (lowest student ID is served first).
    std::vector<Student> all = studentManager.getAllStudents();
    std::sort(all.begin(), all.end(),
              [](const Student& a, const Student& b) { return a.getID() < b.getID(); });
    int added = 0;
    for (const auto& s : all)
        if (addStudentToQueue(s.getID())) added++;
    return added;
}

int Allocator::processAllocationQueue() {
    std::vector<Route> routes = routeManager.getAllRoutes();
    int seated = 0;

    while (!allocationQueue.empty()) {
        int id = allocationQueue.front();   // FIFO: first student queued is served first
        allocationQueue.pop();
        queuedIDs.erase(id);

        Student* s = studentManager.searchStudent(id);
        if (s == nullptr) continue;          // student was removed meanwhile

        int routeID = findRouteForStop(s->getBusStopID(), routes);
        if (routeID == -1) {
            std::cout << "Student #" << id << ": no route serves stop " << s->getBusStopID() << ". Added to waiting list.\n";
            waitingList.push_back(id);
            continue;
        }

        Bus* bus = findAvailableBus(routeID);
        if (bus == nullptr || wouldOvercrowd(bus->getID(), 1) || !bus->addStudents(1)) {
            std::cout << "Student #" << id << ": no free seat on route " << routeID << ". Added to waiting list.\n";
            waitingList.push_back(id);
            continue;
        }

        s->setAssignedRouteID(routeID);
        seated++;
        std::cout << "Student #" << id << " -> Route " << routeID << ", Bus " << bus->getID() << "\n";
    }
    return seated;
}

// Students are tied to a route, not to one particular bus, so the seat is
// returned to the fullest bus on that route.
bool Allocator::releaseStudent(int studentID) {
    Student* s = studentManager.searchStudent(studentID);
    if (s == nullptr || s->getAssignedRouteID() == -1) return false;

    int routeID = s->getAssignedRouteID();
    Bus* fullest = nullptr;
    for (Bus& b : busManager.getBuses()) {
        auto it = busToRoute.find(b.getID());
        if (it == busToRoute.end() || it->second != routeID || b.getCurrentStudents() <= 0) continue;
        if (fullest == nullptr || b.getCurrentStudents() > fullest->getCurrentStudents()) fullest = &b;
    }
    if (fullest != nullptr) fullest->removeStudents(1);
    s->setAssignedRouteID(-1);
    return true;
}

// ---------- excess demand ----------

void Allocator::displayWaitingList() const {
    if (waitingList.empty()) { std::cout << "Waiting list is empty.\n"; return; }
    std::cout << "Students waiting for a seat (" << waitingList.size() << "):\n";
    for (int id : waitingList) {
        Student* s = studentManager.searchStudent(id);
        if (s != nullptr) s->display();
    }
}

int Allocator::retryWaitingList() {
    std::vector<int> pending;
    pending.swap(waitingList);
    for (int id : pending) addStudentToQueue(id);
    return processAllocationQueue();
}

int Allocator::getWaitingCount() const { return static_cast<int>(waitingList.size()); }

// ---------- priority queue + hashing ----------

int Allocator::getRouteDemand(int routeID) const {
    auto it = routeDemand.find(routeID);
    return it == routeDemand.end() ? 0 : it->second;
}

std::vector<int> Allocator::prioritizeRoutes() {
    recomputeDemand();

    // max-heap on (demand, -routeID): highest demand first, lower route ID wins ties
    std::priority_queue<std::pair<int, int>> pq;
    for (const auto& r : routeManager.getAllRoutes())
        pq.push(std::make_pair(getRouteDemand(r.getID()), -r.getID()));

    std::vector<int> order;
    if (pq.empty()) { std::cout << "No routes to prioritize.\n"; return order; }

    std::cout << "Route priority (highest demand first):\n";
    int rank = 1;
    while (!pq.empty()) {
        std::pair<int, int> top = pq.top();
        pq.pop();
        int routeID = -top.second;
        order.push_back(routeID);
        Route* r = routeManager.searchRoute(routeID);
        std::cout << rank++ << ". Route " << routeID << " (" << (r ? r->getName() : "?") << ")"
                  << " | Demand: " << top.first
                  << " | Free seats: " << routeFreeSeats(routeID) << "\n";
    }
    return order;
}

// ---------- report ----------

void Allocator::displayAllocationReport() {
    recomputeDemand();
    std::vector<Student> students = studentManager.getAllStudents();

    std::cout << "========== Allocation Report ==========\n";
    for (const auto& r : routeManager.getAllRoutes()) {
        int allocated = 0;
        for (const auto& s : students)
            if (s.getAssignedRouteID() == r.getID()) allocated++;

        std::cout << "Route " << r.getID() << " (" << r.getName() << ")"
                  << " | Demand: " << getRouteDemand(r.getID())
                  << " | Allocated: " << allocated << "\n";

        bool anyBus = false;
        for (Bus& b : busManager.getBuses()) {
            auto it = busToRoute.find(b.getID());
            if (it == busToRoute.end() || it->second != r.getID()) continue;
            anyBus = true;
            std::cout << "    Bus " << b.getID() << ": " << b.getCurrentStudents() << "/" << b.getCapacity()
                      << (b.isAvailable() ? "" : " [unavailable]") << "\n";
        }
        if (!anyBus) std::cout << "    (no bus assigned)\n";
    }
    std::cout << "Waiting in queue: " << allocationQueue.size()
              << " | On waiting list: " << waitingList.size() << "\n";
}

// ---------- file handling ----------

void Allocator::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) { std::cout << "Could not open " << filename << " for writing.\n"; return; }
    for (const auto& entry : busToRoute) out << entry.first << '|' << entry.second << '\n';
    out.close();
}

void Allocator::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;
    busToRoute.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string busStr, routeStr;
        std::getline(ss, busStr, '|');
        std::getline(ss, routeStr, '|');
        try {
            busToRoute[std::stoi(busStr)] = std::stoi(routeStr);
        } catch (const std::exception&) {
            std::cout << "Skipped corrupted line in " << filename << "\n";
        }
    }
    in.close();
}
