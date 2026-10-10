#ifndef ROUTE_H
#define ROUTE_H

#include <string>
#include <vector>

class Route {
private:
    int id;
    std::string name;
    std::vector<int> stopIDs;   // ordered list of bus stop IDs

public:
    Route();
    Route(int id, const std::string& name);

    int getID() const;
    std::string getName() const;
    void setName(const std::string& n);

    // First and last stop of the route (-1 if the route has no stops)
    int getStartStopID() const;
    int getEndStopID() const;

    const std::vector<int>& getStops() const;
    int getStopCount() const;

    bool hasStop(int stopID) const;
    bool addStop(int stopID);      // appends; false if already in route
    bool removeStop(int stopID);   // false if not in route

    void display() const;
};

#endif
