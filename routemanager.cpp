#include "routemanager.h"
#include <fstream>
#include <iostream>
#include <sstream>

// ---------- private helpers ----------

int RouteManager::findStopIndex(int id) const {
    for (size_t i = 0; i < stops.size(); i++)
        if (stops[i].getID() == id) return static_cast<int>(i);
    return -1;
}

int RouteManager::findRouteIndex(int id) const {
    for (size_t i = 0; i < routes.size(); i++)
        if (routes[i].getID() == id) return static_cast<int>(i);
    return -1;
}

// ---------- bus stops ----------

bool RouteManager::stopIDExists(int id) const { return findStopIndex(id) != -1; }

bool RouteManager::addStop(const BusStop& s) {
    if (s.getID() <= 0) return false;
    if (s.getName().empty() || s.getName().find('|') != std::string::npos) return false;
    if (stopIDExists(s.getID())) return false;
    stops.push_back(s);
    return true;
}

bool RouteManager::removeStop(int id) {
    int idx = findStopIndex(id);
    if (idx == -1) return false;
    stops.erase(stops.begin() + idx);
    for (auto& r : routes) r.removeStop(id);   // keep routes consistent
    return true;
}

bool RouteManager::updateStop(int id, const std::string& newName) {
    if (newName.empty() || newName.find('|') != std::string::npos) return false;
    int idx = findStopIndex(id);
    if (idx == -1) return false;
    stops[idx].setName(newName);
    return true;
}

BusStop* RouteManager::searchStop(int id) {
    int idx = findStopIndex(id);
    return idx == -1 ? nullptr : &stops[idx];
}

void RouteManager::displayStops() const {
    if (stops.empty()) { std::cout << "No bus stops in the system.\n"; return; }
    for (const auto& s : stops) s.display();
}

// ---------- routes ----------

bool RouteManager::routeIDExists(int id) const { return findRouteIndex(id) != -1; }

bool RouteManager::createRoute(const Route& r) {
    if (r.getID() <= 0) return false;
    if (r.getName().empty() || r.getName().find('|') != std::string::npos) return false;
    if (routeIDExists(r.getID())) return false;
    routes.push_back(r);
    return true;
}

bool RouteManager::removeRoute(int id) {
    int idx = findRouteIndex(id);
    if (idx == -1) return false;
    routes.erase(routes.begin() + idx);
    return true;
}

Route* RouteManager::searchRoute(int id) {
    int idx = findRouteIndex(id);
    return idx == -1 ? nullptr : &routes[idx];
}

bool RouteManager::addStopToRoute(int routeID, int stopID) {
    int ridx = findRouteIndex(routeID);
    if (ridx == -1 || !stopIDExists(stopID)) return false;
    return routes[ridx].addStop(stopID);
}

bool RouteManager::removeStopFromRoute(int routeID, int stopID) {
    int ridx = findRouteIndex(routeID);
    if (ridx == -1) return false;
    return routes[ridx].removeStop(stopID);
}

bool RouteManager::isStopInRoute(int routeID, int stopID) const {
    int ridx = findRouteIndex(routeID);
    return ridx != -1 && routes[ridx].hasStop(stopID);
}

void RouteManager::displayRoute(int routeID) const {
    int ridx = findRouteIndex(routeID);
    if (ridx == -1) { std::cout << "Route not found.\n"; return; }

    const Route& r = routes[ridx];
    r.display();
    if (r.getStopCount() == 0) { std::cout << "  (no stops added yet)\n"; return; }

    std::cout << "  ";
    const std::vector<int>& ids = r.getStops();
    for (size_t i = 0; i < ids.size(); i++) {
        int sidx = findStopIndex(ids[i]);
        std::cout << (sidx == -1 ? "?" : stops[sidx].getName());
        if (i + 1 < ids.size()) std::cout << " -> ";
    }
    std::cout << "\n";
}

void RouteManager::displayAllRoutes() const {
    if (routes.empty()) { std::cout << "No routes in the system.\n"; return; }
    for (const auto& r : routes) displayRoute(r.getID());
}

std::vector<BusStop> RouteManager::getAllStops() const { return stops; }
std::vector<Route> RouteManager::getAllRoutes() const { return routes; }

// ---------- file handling ----------
// stops.dat  : id|name
// routes.dat : id|name|stopID,stopID,stopID

void RouteManager::saveStopsToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) { std::cout << "Could not open " << filename << " for writing.\n"; return; }
    for (const auto& s : stops) out << s.getID() << '|' << s.getName() << '\n';
    out.close();
}

void RouteManager::loadStopsFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;
    stops.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, name;
        std::getline(ss, idStr, '|');
        std::getline(ss, name, '|');
        try {
            BusStop s(std::stoi(idStr), name);
            if (s.getID() > 0 && !s.getName().empty() && !stopIDExists(s.getID()))
                stops.push_back(s);
        } catch (const std::exception&) {
            std::cout << "Skipped corrupted line in " << filename << "\n";
        }
    }
    in.close();
}

void RouteManager::saveRoutesToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) { std::cout << "Could not open " << filename << " for writing.\n"; return; }
    for (const auto& r : routes) {
        out << r.getID() << '|' << r.getName() << '|';
        const std::vector<int>& ids = r.getStops();
        for (size_t i = 0; i < ids.size(); i++) {
            out << ids[i];
            if (i + 1 < ids.size()) out << ',';
        }
        out << '\n';
    }
    out.close();
}

void RouteManager::loadRoutesFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;
    routes.clear();
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, name, stopsStr;
        std::getline(ss, idStr, '|');
        std::getline(ss, name, '|');
        std::getline(ss, stopsStr, '|');
        try {
            Route r(std::stoi(idStr), name);
            std::stringstream ls(stopsStr);
            std::string token;
            while (std::getline(ls, token, ',')) {
                if (token.empty()) continue;
                int sid = std::stoi(token);
                if (stopIDExists(sid)) r.addStop(sid);   // skip stops that no longer exist
            }
            if (r.getID() > 0 && !r.getName().empty() && !routeIDExists(r.getID()))
                routes.push_back(r);
        } catch (const std::exception&) {
            std::cout << "Skipped corrupted line in " << filename << "\n";
        }
    }
    in.close();
}
