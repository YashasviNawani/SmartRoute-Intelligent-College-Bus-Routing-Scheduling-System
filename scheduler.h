#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "allocator.h"
#include "busManager.h"
#include "conflict.h"
#include "graph.h"
#include "routemanager.h"
#include "studentManager.h"

// ---------------------------------------------------------------------------
// Module 6 (part 1): Scheduler
// Demonstrates: priority queue (greedy bus reinforcement), graph weights for
// trip length, interval scheduling (via conflict.h)
//
// runSchedule does three things:
//   1. seats every unallocated student (Allocator queue)
//   2. sends spare buses to the routes with the biggest seat shortfall
//      (max-heap on shortfall)
//   3. builds the morning + evening trip list and checks it for conflicts
// ---------------------------------------------------------------------------

struct ScheduleConfig {
    int collegeStartMin = 9 * 60;            // morning trips must arrive by this time
    int collegeEndMin = 16 * 60 + 30;        // evening trips leave at this time
    double defaultEdgeMinutes = 5.0;         // travel time between stops with no weight set
    int dwellMinutesPerStop = 2;             // time spent loading at each stop
    bool autoReinforce = true;               // allow spare buses to be put on busy routes
    std::vector<EdgeWeight> edgeWeights;     // per-pair travel times (minutes)
};

struct UnservedRoute {
    int routeID;
    int seatsShort;
};

struct Reinforcement {
    int busID;
    int routeID;
};

struct ScheduleResult {
    std::vector<Trip> assignments;
    std::vector<Conflict> conflicts;
    std::vector<UnservedRoute> unserved;         // routes still short of seats
    std::vector<Reinforcement> reinforcements;   // spare buses that were assigned
    int seated = 0;                              // students seated during this run
    int waiting = 0;                             // students still without a seat
};

ScheduleResult runSchedule(RouteManager& rm, BusManager& bm, StudentManager& sm,
                           Allocator& alloc,
                           const ScheduleConfig& cfg = ScheduleConfig());

void displaySchedule(const ScheduleResult& result, RouteManager& rm);

// Minutes one trip takes: travel along the route's own stop order + loading time.
int tripDuration(const Route& route, const Graph& graph, const ScheduleConfig& cfg);

// Students needing each route (seated + waiting) and the seats on its available buses.
std::unordered_map<int, int> computeRouteNeed(RouteManager& rm, StudentManager& sm);
int routeCapacity(BusManager& bm, Allocator& alloc, int routeID);

// ---- Route optimization menu helpers (keep graph.h out of main) ----
bool showEfficientRoute(RouteManager& rm, int fromStop, int toStop,
                        const ScheduleConfig& cfg = ScheduleConfig());
bool showStopTraversal(RouteManager& rm, int startStop, bool useBfs,
                       const ScheduleConfig& cfg = ScheduleConfig());

// ---- Allocation menu helpers (keep allocator.h out of main) ----
bool assignBusToRoute(Allocator& alloc, int busID, int routeID);
void showAllocationReport(Allocator& alloc);

#endif
