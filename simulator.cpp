#include "simulator.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

const int UNDERUSED_BELOW_PCT = 40;   // utilization under this = underutilized

class CoutSilencer {
    std::streambuf* old;
    std::ostringstream sink;
public:
    CoutSilencer() : old(std::cout.rdbuf(sink.rdbuf())) {}
    ~CoutSilencer() { std::cout.rdbuf(old); }
};


struct Sandbox {
    BusManager bm;
    RouteManager rm;
    StudentManager sm;
    Allocator alloc;

    Sandbox(BusManager& liveBm, RouteManager& liveRm, StudentManager& liveSm, Allocator& liveAlloc)
        : bm(liveBm), rm(liveRm), sm(), alloc(bm, sm, rm) {
        // Students (and their departments) are copied through the public API.
        for (const Student& s : liveSm.getAllStudents()) {
            if (!sm.departmentIDExists(s.getDepartmentID())) {
                Department* d = liveSm.findDepartment(s.getDepartmentID());
                if (d != nullptr) sm.addDepartment(*d);
            }
            sm.addStudent(s);
            Student* copy = sm.searchStudent(s.getID());
            if (copy != nullptr) {
                copy->setBusStopID(s.getBusStopID());
                copy->setAssignedRouteID(s.getAssignedRouteID());
            }
        }
        
        for (Bus& b : bm.getBuses()) {
            int r = liveAlloc.getRouteOfBus(b.getID());
            if (r != -1) alloc.assignBusToRoute(b.getID(), r);
        }
    }
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;
};

std::string describe(const Scenario& sc) {
    switch (sc.type) {
        case ScenarioType::BusUnavailable:
            return "Bus " + std::to_string(sc.targetID) + " is unavailable";
        case ScenarioType::DemandIncrease:
            return std::to_string(sc.amount) + " extra students on route " + std::to_string(sc.targetID);
        case ScenarioType::StopUnreachable:
            return "Stop " + std::to_string(sc.targetID) + " becomes unreachable";
    }
    return "Unknown scenario";
}

Snapshot takeSnapshot(Sandbox& sb, const ScheduleResult& sr) {
    Snapshot snap;
    for (const Student& s : sb.sm.getAllStudents()) {
        snap.students++;
        if (s.getAssignedRouteID() != -1) snap.seated++;
        else if (s.getBusStopID() != -1) snap.waiting++;
    }
    for (Bus& b : sb.bm.getBuses()) {
        if (!b.isAvailable()) continue;
        snap.availableBuses++;
        snap.freeSeats += b.getAvailableSeats();
    }
    for (const UnservedRoute& u : sr.unserved) snap.unservedSeats += u.seatsShort;
    for (const Conflict& c : sr.conflicts)
        if (c.severe) snap.severeConflicts++;
    return snap;
}

// Returns false (and fills report.error) when the scenario does not make sense.
bool applyScenario(Sandbox& sb, const Scenario& sc, SimulationReport& rep) {
    switch (sc.type) {

        case ScenarioType::BusUnavailable: {
            Bus* bus = sb.bm.searchBus(sc.targetID);
            if (bus == nullptr) { rep.error = "Bus " + std::to_string(sc.targetID) + " does not exist."; return false; }
            int routeID = sb.alloc.getRouteOfBus(sc.targetID);
            int load = bus->getCurrentStudents();
            bus->removeStudents(load);
            bus->setAvailable(false);
            rep.notes.push_back("Bus " + std::to_string(sc.targetID) + " was carrying " + std::to_string(load) + " student(s).");
            if (routeID != -1 && load > 0) {
                // The bus's riders lose their seat (highest student IDs first, to stay deterministic).
                std::vector<Student> riders;
                for (const Student& s : sb.sm.getAllStudents())
                    if (s.getAssignedRouteID() == routeID) riders.push_back(s);
                std::sort(riders.begin(), riders.end(),
                          [](const Student& a, const Student& b) { return a.getID() > b.getID(); });
                for (int i = 0; i < load && i < static_cast<int>(riders.size()); ++i) {
                    Student* s = sb.sm.searchStudent(riders[i].getID());
                    if (s != nullptr) s->setAssignedRouteID(-1);
                }
                rep.notes.push_back("Route " + std::to_string(routeID) + " must re-seat those students on its other buses.");
            }
            return true;
        }

        case ScenarioType::DemandIncrease: {
            Route* route = sb.rm.searchRoute(sc.targetID);
            if (route == nullptr) { rep.error = "Route " + std::to_string(sc.targetID) + " does not exist."; return false; }
            if (sc.amount <= 0) { rep.error = "The number of extra students must be positive."; return false; }
            int stopID = route->getStartStopID();
            if (stopID == -1) { rep.error = "Route " + std::to_string(sc.targetID) + " has no stops."; return false; }

            int deptID = 0;
            if (!sb.sm.departmentIDExists(deptID)) sb.sm.addDepartment(Department(deptID, "Simulated"));
            int nextID = 1;
            for (const Student& s : sb.sm.getAllStudents()) nextID = std::max(nextID, s.getID() + 1);
            for (int i = 0; i < sc.amount; ++i)
                sb.sm.addStudent(Student(nextID + i, "SimStudent", deptID, stopID));
            rep.notes.push_back(std::to_string(sc.amount) + " students added at the first stop of route " + std::to_string(sc.targetID) + ".");
            return true;
        }

        case ScenarioType::StopUnreachable: {
            if (!sb.rm.stopIDExists(sc.targetID)) { rep.error = "Stop " + std::to_string(sc.targetID) + " does not exist."; return false; }
            int stranded = 0;
            for (const Student& s : sb.sm.getAllStudents()) {
                if (s.getBusStopID() != sc.targetID) continue;
                stranded++;
                if (s.getAssignedRouteID() != -1) sb.alloc.releaseStudent(s.getID());
            }
            std::vector<int> affected;
            for (const Route& r : sb.rm.getAllRoutes())
                if (r.hasStop(sc.targetID)) affected.push_back(r.getID());
            sb.rm.removeStop(sc.targetID);   // also removes the stop from every route
            rep.notes.push_back(std::to_string(stranded) + " student(s) board at this stop and lose their service.");
            if (!affected.empty()) {
                std::string list;
                for (size_t i = 0; i < affected.size(); ++i) list += (i ? ", " : "") + std::to_string(affected[i]);
                rep.notes.push_back("Routes that skip the stop: " + list + ".");
            }
            return true;
        }
    }
    return false;
}

std::string statusText(DemandStatus s) {
    switch (s) {
        case DemandStatus::NoBus: return "NO BUS";
        case DemandStatus::Overburdened: return "OVERBURDENED";
        case DemandStatus::Underutilized: return "UNDERUTILIZED";
        case DemandStatus::Balanced: return "balanced";
    }
    return "";
}

}  // namespace

Simulator::Simulator(BusManager& bm, RouteManager& rm, StudentManager& sm, Allocator& alloc,
                     const ScheduleConfig& cfg)
    : busManager(bm), routeManager(rm), studentManager(sm), allocator(alloc), config(cfg) {}

SimulationReport Simulator::run(const Scenario& scenario) {
    SimulationReport rep;
    rep.description = describe(scenario);

    Sandbox sb(busManager, routeManager, studentManager, allocator);

    {   // baseline: the same pipeline, with nothing changed
        CoutSilencer quiet;
        ScheduleResult base = runSchedule(sb.rm, sb.bm, sb.sm, sb.alloc, config);
        rep.before = takeSnapshot(sb, base);
    }

    if (!applyScenario(sb, scenario, rep)) {
        rep.valid = false;
        rep.after = rep.before;
        return rep;
    }

    {
        CoutSilencer quiet;
        ScheduleResult res = runSchedule(sb.rm, sb.bm, sb.sm, sb.alloc, config);
        rep.after = takeSnapshot(sb, res);
    }
    return rep;
}

void Simulator::displayReport(const SimulationReport& rep) const {
    std::cout << "======== What-If: " << rep.description << " ========\n";
    if (!rep.valid) {
        std::cout << "Cannot simulate: " << rep.error << "\n";
        return;
    }
    for (const std::string& n : rep.notes) std::cout << "  * " << n << "\n";

    struct Row { const char* label; int before; int after; bool higherIsBetter; };
    const Row rows[] = {
        {"Students seated",        rep.before.seated,          rep.after.seated,          true},
        {"Students waiting",       rep.before.waiting,         rep.after.waiting,         false},
        {"Available buses",        rep.before.availableBuses,  rep.after.availableBuses,  true},
        {"Free seats",             rep.before.freeSeats,       rep.after.freeSeats,       true},
        {"Seats still missing",    rep.before.unservedSeats,   rep.after.unservedSeats,   false},
        {"Severe conflicts",       rep.before.severeConflicts, rep.after.severeConflicts, false},
    };
    std::cout << std::left << std::setw(24) << "Metric" << std::setw(10) << "Before"
              << std::setw(10) << "After" << "Change\n";
    for (const Row& r : rows) {
        int delta = r.after - r.before;
        std::cout << std::left << std::setw(24) << r.label << std::setw(10) << r.before
                  << std::setw(10) << r.after << (delta > 0 ? "+" : "") << delta << "\n";
    }
    bool worse = rep.after.waiting > rep.before.waiting || rep.after.unservedSeats > rep.before.unservedSeats ||
                 rep.after.severeConflicts > rep.before.severeConflicts;
    std::cout << (worse ? "Result: service gets worse under this scenario.\n"
                        : "Result: the system absorbs this scenario.\n");
    std::cout << "(Live data was not changed.)\n";
}

std::vector<RouteDemandInfo> Simulator::analyzeDemand() {
    std::vector<RouteDemandInfo> out;
    std::unordered_map<int, int> need = computeRouteNeed(routeManager, studentManager);

    for (const Route& r : routeManager.getAllRoutes()) {
        RouteDemandInfo info;
        info.routeID = r.getID();
        info.name = r.getName();
        info.demand = need[r.getID()];
        info.capacity = routeCapacity(busManager, allocator, r.getID());
        info.utilizationPct = info.capacity > 0 ? (info.demand * 100) / info.capacity : 0;
        if (info.capacity == 0) info.status = DemandStatus::NoBus;
        else if (info.demand > info.capacity) info.status = DemandStatus::Overburdened;
        else if (info.utilizationPct < UNDERUSED_BELOW_PCT) info.status = DemandStatus::Underutilized;
        else info.status = DemandStatus::Balanced;
        out.push_back(info);
    }

    std::cout << "============ Demand Analysis ============\n";
    if (out.empty()) { std::cout << "No routes defined.\n"; return out; }
    std::cout << std::left << std::setw(8) << "Route" << std::setw(20) << "Name" << std::setw(9) << "Demand"
              << std::setw(10) << "Capacity" << std::setw(7) << "Use%" << "Status\n";
    for (const RouteDemandInfo& i : out)
        std::cout << std::left << std::setw(8) << i.routeID << std::setw(20) << i.name << std::setw(9) << i.demand
                  << std::setw(10) << i.capacity << std::setw(7) << i.utilizationPct << statusText(i.status) << "\n";
    return out;
}
