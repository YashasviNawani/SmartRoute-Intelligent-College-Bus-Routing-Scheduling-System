#ifndef BUS_STOP_H
#define BUS_STOP_H

#include <string>

// ---------------------------------------------------------------------------
// Class: BusStop
// Design note: every stop gets a numeric ID now so it can become a graph
// vertex directly in Phase 2 with no redesign.
// ---------------------------------------------------------------------------
class BusStop {
private:
    int stopID;
    std::string stopName;

public:
    BusStop();
    BusStop(int stopID, const std::string& stopName);

    int getStopID() const;
    std::string getStopName() const;
    void setStopName(const std::string& name);

    void display() const;
};

#endif
