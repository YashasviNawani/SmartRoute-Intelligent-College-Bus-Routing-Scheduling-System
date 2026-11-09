#include "conflict.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
Conflict makeConflict(ConflictType type, int busID, int routeA, int routeB,
                      bool severe, const std::string& text) {
    Conflict c;
    c.type = type;
    c.busID = busID;
    c.routeA = routeA;
    c.routeB = routeB;
    c.severe = severe;
    c.description = text;
    return c;
}

std::string window(const Trip& t) {
    return formatMinutes(t.startMin) + "-" + formatMinutes(t.endMin);
}
}  // namespace

std::string formatMinutes(int minutes) {
    if (minutes < 0) minutes = 0;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return buf;
}

bool hasOverlap(const Trip& a, const Trip& b) {
    return a.startMin < b.endMin && b.startMin < a.endMin;
}

int sharedStopCount(const Route& a, const Route& b) {
    std::unordered_set<int> inA(a.getStops().begin(), a.getStops().end());
    int count = 0;
    for (int s : b.getStops())
        if (inA.count(s)) count++;
    return count;
}

bool hasOverlap(const Route& a, const Route& b) {
    return sharedStopCount(a, b) > 0;
}

std::vector<Conflict> detectConflicts(const std::vector<Trip>& trips,
                                      const std::vector<Bus>& buses,
                                      const std::vector<Route>& routes) {
    std::vector<Conflict> out;

    std::unordered_map<int, const Bus*> busByID;
    for (const Bus& b : buses) busByID[b.getID()] = &b;
    std::unordered_map<int, const Route*> routeByID;
    for (const Route& r : routes) routeByID[r.getID()] = &r;

    // 1. Bus checks (reported once per bus)
    std::set<int> flaggedMissing, flaggedUnavailable;
    for (const Trip& t : trips) {
        auto it = busByID.find(t.busID);
        if (it == busByID.end()) {
            if (flaggedMissing.insert(t.busID).second)
                out.push_back(makeConflict(ConflictType::UnknownBus, t.busID, t.routeID, -1, true,
                    "Bus " + std::to_string(t.busID) + " is scheduled but does not exist."));
        } else if (!it->second->isAvailable()) {
            if (flaggedUnavailable.insert(t.busID).second)
                out.push_back(makeConflict(ConflictType::BusUnavailable, t.busID, t.routeID, -1, true,
                    "Bus " + std::to_string(t.busID) + " is scheduled on route " +
                    std::to_string(t.routeID) + " but is marked unavailable."));
        }
    }

    // 2. Double-booked buses: sort by (bus, start) and sweep forward
    std::vector<const Trip*> sorted;
    for (const Trip& t : trips) sorted.push_back(&t);
    std::sort(sorted.begin(), sorted.end(), [](const Trip* x, const Trip* y) {
        if (x->busID != y->busID) return x->busID < y->busID;
        if (x->startMin != y->startMin) return x->startMin < y->startMin;
        return x->endMin < y->endMin;
    });
    for (size_t i = 0; i < sorted.size(); ++i) {
        for (size_t j = i + 1; j < sorted.size() && sorted[j]->busID == sorted[i]->busID; ++j) {
            if (sorted[j]->startMin >= sorted[i]->endMin) break;   // later trips start even later
            out.push_back(makeConflict(ConflictType::BusDoubleBooked, sorted[i]->busID,
                sorted[i]->routeID, sorted[j]->routeID, true,
                "Bus " + std::to_string(sorted[i]->busID) + " is double-booked: route " +
                std::to_string(sorted[i]->routeID) + " (" + window(*sorted[i]) + ") overlaps route " +
                std::to_string(sorted[j]->routeID) + " (" + window(*sorted[j]) + ")."));
        }
    }

    // 3. Routes that share a stop and run at the same time (warning only)
    std::set<std::pair<int, int>> reported;
    for (size_t i = 0; i < trips.size(); ++i) {
        for (size_t j = i + 1; j < trips.size(); ++j) {
            const Trip& a = trips[i];
            const Trip& b = trips[j];
            if (a.routeID == b.routeID || a.busID == b.busID) continue;
            if (!hasOverlap(a, b)) continue;
            std::pair<int, int> key(std::min(a.routeID, b.routeID), std::max(a.routeID, b.routeID));
            if (reported.count(key)) continue;
            auto ra = routeByID.find(a.routeID);
            auto rb = routeByID.find(b.routeID);
            if (ra == routeByID.end() || rb == routeByID.end()) continue;
            int shared = sharedStopCount(*ra->second, *rb->second);
            if (shared == 0) continue;
            reported.insert(key);
            out.push_back(makeConflict(ConflictType::SharedStopOverlap, -1, key.first, key.second, false,
                "Routes " + std::to_string(key.first) + " and " + std::to_string(key.second) +
                " share " + std::to_string(shared) + " stop(s) and run at overlapping times."));
        }
    }
    return out;
}
