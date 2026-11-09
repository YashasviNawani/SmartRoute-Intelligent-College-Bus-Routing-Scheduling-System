#ifndef CONFLICT_H
#define CONFLICT_H

#include <string>
#include <vector>
#include "bus.h"
#include "route.h"

// ---------------------------------------------------------------------------
// Module 6 (part 2): Conflict detection
// Demonstrates: interval overlap, sorting + sweep, hashing (ID lookups)
// ---------------------------------------------------------------------------

// One scheduled run of a bus on a route. Times are minutes since midnight.
struct Trip {
    int routeID;
    int busID;
    int startMin;
    int endMin;
    std::string label;      // e.g. "Morning" / "Evening"
};

enum class ConflictType {
    BusDoubleBooked,        // same bus, two trips overlapping in time
    BusUnavailable,         // trip given to a bus that is marked unavailable
    UnknownBus,             // trip refers to a bus ID that does not exist
    SharedStopOverlap       // two routes share a stop and run at the same time
};

struct Conflict {
    ConflictType type;
    int busID;              // -1 if not relevant
    int routeA;             // -1 if not relevant
    int routeB;             // -1 if not relevant
    bool severe;            // false = warning only (shared stops)
    std::string description;
};

// "HH:MM" for a minutes-since-midnight value.
std::string formatMinutes(int minutes);

// Two trips overlap when their time windows intersect (touching ends do not).
bool hasOverlap(const Trip& a, const Trip& b);

// Two routes overlap when they have at least one stop in common.
bool hasOverlap(const Route& a, const Route& b);
int sharedStopCount(const Route& a, const Route& b);

std::vector<Conflict> detectConflicts(const std::vector<Trip>& trips,
                                      const std::vector<Bus>& buses,
                                      const std::vector<Route>& routes);

#endif
