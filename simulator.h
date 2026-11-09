#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>

#include "allocator.h"
#include "busManager.h"
#include "routemanager.h"
#include "scheduler.h"
#include "studentManager.h"

enum class ScenarioType {
    BusUnavailable,     // targetID = bus ID
    DemandIncrease,     // targetID = route ID, amount = extra students
    StopUnreachable     // targetID = stop ID
};

struct Scenario {
    ScenarioType type;
    int targetID;
    int amount;         // only used by DemandIncrease
};

struct Snapshot {
    int students = 0;
    int seated = 0;
    int waiting = 0;            // students with a stop but no seat
    int availableBuses = 0;
    int freeSeats = 0;          // free seats on available buses
    int unservedSeats = 0;      // seats still missing after scheduling
    int severeConflicts = 0;
};

struct SimulationReport {
    bool valid = true;
    std::string description;
    std::string error;
    Snapshot before;
    Snapshot after;
    std::vector<std::string> notes;
};

enum class DemandStatus { NoBus, Overburdened, Balanced, Underutilized };

struct RouteDemandInfo {
    int routeID;
    std::string name;
    int demand;
    int capacity;
    int utilizationPct;     // demand / capacity, 0 when there is no capacity
    DemandStatus status;
};

class Simulator {
private:
    BusManager& busManager;
    RouteManager& routeManager;
    StudentManager& studentManager;
    Allocator& allocator;
    ScheduleConfig config;

public:
    Simulator(BusManager& bm, RouteManager& rm, StudentManager& sm, Allocator& alloc,
              const ScheduleConfig& cfg = ScheduleConfig());

    SimulationReport run(const Scenario& scenario);
    void displayReport(const SimulationReport& report) const;

    // Flags overburdened and underutilized routes on the live data.
    std::vector<RouteDemandInfo> analyzeDemand();
};

#endif
