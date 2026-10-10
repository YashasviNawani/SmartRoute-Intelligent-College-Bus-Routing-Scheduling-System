#include "route.h"
#include <algorithm>
#include <iostream>

Route::Route() : id(0), name("") {}

Route::Route(int id, const std::string& name) : id(id), name(name) {}

int Route::getID() const { return id; }
std::string Route::getName() const { return name; }
void Route::setName(const std::string& n) { name = n; }

int Route::getStartStopID() const { return stopIDs.empty() ? -1 : stopIDs.front(); }
int Route::getEndStopID() const { return stopIDs.empty() ? -1 : stopIDs.back(); }

const std::vector<int>& Route::getStops() const { return stopIDs; }
int Route::getStopCount() const { return static_cast<int>(stopIDs.size()); }

bool Route::hasStop(int stopID) const {
    return std::find(stopIDs.begin(), stopIDs.end(), stopID) != stopIDs.end();
}

bool Route::addStop(int stopID) {
    if (hasStop(stopID)) return false;
    stopIDs.push_back(stopID);
    return true;
}

bool Route::removeStop(int stopID) {
    auto it = std::find(stopIDs.begin(), stopIDs.end(), stopID);
    if (it == stopIDs.end()) return false;
    stopIDs.erase(it);
    return true;
}

void Route::display() const {
    std::cout << "Route ID: " << id << " | Name: " << name
              << " | Stops: " << stopIDs.size() << "\n";
}
