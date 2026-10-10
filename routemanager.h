#ifndef ROUTEMANAGER_H
#define ROUTEMANAGER_H

#include "busstop.h"
#include "route.h"
#include <string>
#include <vector>

class RouteManager {
private:
    std::vector<BusStop> stops;
    std::vector<Route> routes;

    int findStopIndex(int id) const;    // -1 if not found
    int findRouteIndex(int id) const;   // -1 if not found

public:
    // ---- Bus stops ----
    bool stopIDExists(int id) const;
    bool addStop(const BusStop& s);
    bool removeStop(int id);                              // also removes it from every route
    bool updateStop(int id, const std::string& newName);
    BusStop* searchStop(int id);
    void displayStops() const;

    // ---- Routes ----
    bool routeIDExists(int id) const;
    bool createRoute(const Route& r);
    bool removeRoute(int id);
    Route* searchRoute(int id);
    bool addStopToRoute(int routeID, int stopID);         // stop must exist
    bool removeStopFromRoute(int routeID, int stopID);
    bool isStopInRoute(int routeID, int stopID) const;
    void displayRoute(int routeID) const;                 // full stop-by-stop view
    void displayAllRoutes() const;

    // ---- Accessors (useful for the Allocator later) ----
    std::vector<BusStop> getAllStops() const;
    std::vector<Route> getAllRoutes() const;

    // ---- File handling ----
    void saveStopsToFile(const std::string& filename) const;
    void loadStopsFromFile(const std::string& filename);
    void saveRoutesToFile(const std::string& filename) const;
    void loadRoutesFromFile(const std::string& filename);
};

#endif
