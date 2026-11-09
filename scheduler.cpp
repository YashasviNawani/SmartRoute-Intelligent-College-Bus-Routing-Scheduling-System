#include "scheduler.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <queue>
#include <utility>

namespace {

int firstRouteWithStop(int stopID, const std::vector<Route>& routes) {
    for (const Route& r : routes)
        if (r.hasStop(stopID)) return r.getID();
    return -1;
}

std::string stopName(RouteManager& rm, int stopID) {
    BusStop* s = rm.searchStop(stopID);
    return s ? s->getName() : "Stop " + std::to_string(stopID);
}

Graph graphFor(RouteManager& rm, const ScheduleConfig& cfg) {
    return buildGraph(rm.getAllStops(), rm.getAllRoutes(), cfg.edgeWeights, cfg.defaultEdgeMinutes);
}

}  // namespace

// ---------- helpers shared with the simulator ----------

std::unordered_map<int, int> computeRouteNeed(RouteManager& rm, StudentManager& sm) {
    std::vector<Route> routes = rm.getAllRoutes();
    std::unordered_map<int, int> need;
    for (const Student& s : sm.getAllStudents()) {
        int r = s.getAssignedRouteID();
        if (r == -1 && s.getBusStopID() != -1) r = firstRouteWithStop(s.getBusStopID(), routes);
        if (r != -1) need[r]++;
    }
    return need;
}

int routeCapacity(BusManager& bm, Allocator& alloc, int routeID) {
    int total = 0;
    for (Bus& b : bm.getBuses())
        if (b.isAvailable() && alloc.getRouteOfBus(b.getID()) == routeID) total += b.getCapacity();
    return total;
}

int tripDuration(const Route& route, const Graph& graph, const ScheduleConfig& cfg) {
    const std::vector<int>& st = route.getStops();
    if (st.empty()) return 0;
    double total = 0.0;
    for (size_t i = 0; i + 1 < st.size(); ++i) {
        double w = graph.getWeight(st[i], st[i + 1]);
        total += (w < 0) ? cfg.defaultEdgeMinutes : w;
    }
    total += static_cast<double>(cfg.dwellMinutesPerStop) * static_cast<double>(st.size());
    return static_cast<int>(std::ceil(total));
}

// ---------- the scheduler ----------

ScheduleResult runSchedule(RouteManager& rm, BusManager& bm, StudentManager& sm,
                           Allocator& alloc, const ScheduleConfig& cfg) {
    ScheduleResult result;

    // 1. seat everyone who has a stop but no route yet
    alloc.queueAllUnallocatedStudents();
    result.seated = alloc.processAllocationQueue();

    std::vector<Route> routes = rm.getAllRoutes();
    Graph graph = graphFor(rm, cfg);

    // 2. greedy reinforcement: biggest shortfall first (max-heap)
    std::unordered_map<int, int> need = computeRouteNeed(rm, sm);
    std::priority_queue<std::pair<int, int>> pq;          // (shortfall, -routeID)
    for (const Route& r : routes) {
        int shortfall = need[r.getID()] - routeCapacity(bm, alloc, r.getID());
        if (shortfall > 0) pq.push(std::make_pair(shortfall, -r.getID()));
    }

    std::vector<std::pair<int, int>> spare;               // (capacity, busID), biggest first
    for (Bus& b : bm.getBuses())
        if (b.isAvailable() && alloc.getRouteOfBus(b.getID()) == -1)
            spare.push_back(std::make_pair(b.getCapacity(), b.getID()));
    std::sort(spare.begin(), spare.end(), [](const std::pair<int, int>& x, const std::pair<int, int>& y) {
        if (x.first != y.first) return x.first > y.first;
        return x.second < y.second;
    });

    size_t nextSpare = 0;
    if (cfg.autoReinforce) {
        while (!pq.empty() && nextSpare < spare.size()) {
            std::pair<int, int> top = pq.top();
            pq.pop();
            int shortfall = top.first;
            int routeID = -top.second;
            int cap = spare[nextSpare].first;
            int busID = spare[nextSpare].second;
            ++nextSpare;
            if (alloc.assignBusToRoute(busID, routeID)) {
                Reinforcement ri;
                ri.busID = busID;
                ri.routeID = routeID;
                result.reinforcements.push_back(ri);
                shortfall -= cap;
            }
            if (shortfall > 0) pq.push(std::make_pair(shortfall, -routeID));
        }
        if (!result.reinforcements.empty()) result.seated += alloc.retryWaitingList();
    }
    while (!pq.empty()) {
        UnservedRoute u;
        u.routeID = -pq.top().second;
        u.seatsShort = pq.top().first;
        result.unserved.push_back(u);
        pq.pop();
    }
    result.waiting = alloc.getWaitingCount();

    // 3. build trips: every available bus does a morning and an evening run
    std::vector<int> busIDs;
    for (Bus& b : bm.getBuses()) busIDs.push_back(b.getID());
    std::sort(busIDs.begin(), busIDs.end());

    for (const Route& r : routes) {
        int dur = tripDuration(r, graph, cfg);
        if (dur <= 0) continue;
        for (int busID : busIDs) {
            Bus* b = bm.searchBus(busID);
            if (b == nullptr || !b->isAvailable() || alloc.getRouteOfBus(busID) != r.getID()) continue;

            Trip morning;
            morning.routeID = r.getID();
            morning.busID = busID;
            morning.endMin = cfg.collegeStartMin;
            morning.startMin = morning.endMin - dur;
            if (morning.startMin < 0) { morning.startMin = 0; morning.endMin = dur; }
            morning.label = "Morning";
            result.assignments.push_back(morning);

            Trip evening;
            evening.routeID = r.getID();
            evening.busID = busID;
            evening.startMin = cfg.collegeEndMin;
            evening.endMin = evening.startMin + dur;
            evening.label = "Evening";
            result.assignments.push_back(evening);
        }
    }
    std::sort(result.assignments.begin(), result.assignments.end(), [](const Trip& x, const Trip& y) {
        if (x.startMin != y.startMin) return x.startMin < y.startMin;
        return x.busID < y.busID;
    });

    result.conflicts = detectConflicts(result.assignments, bm.getBuses(), routes);
    return result;
}

void displaySchedule(const ScheduleResult& result, RouteManager& rm) {
    std::cout << "============ Schedule ============\n";
    std::cout << "Students seated this run: " << result.seated
              << " | still waiting: " << result.waiting << "\n";

    if (!result.reinforcements.empty()) {
        std::cout << "Spare buses sent to busy routes:\n";
        for (const Reinforcement& ri : result.reinforcements)
            std::cout << "  Bus " << ri.busID << " -> Route " << ri.routeID << "\n";
    }

    if (result.assignments.empty()) {
        std::cout << "No trips scheduled (assign buses to routes first).\n";
    } else {
        std::cout << std::left << std::setw(8) << "Bus" << std::setw(24) << "Route"
                  << std::setw(10) << "Run" << "Time\n";
        for (const Trip& t : result.assignments) {
            Route* r = rm.searchRoute(t.routeID);
            std::string rname = std::to_string(t.routeID) + (r ? " (" + r->getName() + ")" : "");
            std::cout << std::left << std::setw(8) << t.busID << std::setw(24) << rname
                      << std::setw(10) << t.label
                      << formatMinutes(t.startMin) << " - " << formatMinutes(t.endMin) << "\n";
        }
    }

    for (const UnservedRoute& u : result.unserved)
        std::cout << "WARNING: route " << u.routeID << " is short of " << u.seatsShort << " seat(s).\n";

    if (result.conflicts.empty()) {
        std::cout << "No scheduling conflicts.\n";
    } else {
        std::cout << "Conflicts (" << result.conflicts.size() << "):\n";
        for (const Conflict& c : result.conflicts)
            std::cout << "  [" << (c.severe ? "ERROR" : "warning") << "] " << c.description << "\n";
    }
}

// ---------- route optimization menu helpers ----------

bool showEfficientRoute(RouteManager& rm, int fromStop, int toStop, const ScheduleConfig& cfg) {
    if (!rm.stopIDExists(fromStop) || !rm.stopIDExists(toStop)) {
        std::cout << "Both stops must exist.\n";
        return false;
    }
    Graph g = graphFor(rm, cfg);
    PathResult pr = findMostEfficientRoute(g, fromStop, toStop);
    if (!pr.found) {
        std::cout << "No route connects " << stopName(rm, fromStop) << " and " << stopName(rm, toStop) << ".\n";
        return false;
    }
    std::cout << "Most efficient path (" << pr.distance << " min):\n  ";
    for (size_t i = 0; i < pr.path.size(); ++i) {
        if (i) std::cout << " -> ";
        std::cout << stopName(rm, pr.path[i]);
    }
    std::cout << "\n";
    return true;
}

bool showStopTraversal(RouteManager& rm, int startStop, bool useBfs, const ScheduleConfig& cfg) {
    if (!rm.stopIDExists(startStop)) {
        std::cout << "Stop " << startStop << " does not exist.\n";
        return false;
    }
    Graph g = graphFor(rm, cfg);
    std::vector<int> order = useBfs ? bfsTraverse(g, startStop) : dfsTraverse(g, startStop);
    std::cout << (useBfs ? "BFS" : "DFS") << " from " << stopName(rm, startStop) << ":\n  ";
    for (size_t i = 0; i < order.size(); ++i) {
        if (i) std::cout << " -> ";
        std::cout << stopName(rm, order[i]);
    }
    std::cout << "\n";
    return true;
}

// ---------- allocation menu helpers ----------

bool assignBusToRoute(Allocator& alloc, int busID, int routeID) {
    return alloc.assignBusToRoute(busID, routeID);
}

void showAllocationReport(Allocator& alloc) {
    alloc.displayAllocationReport();
}
