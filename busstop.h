#ifndef BUSSTOP_H
#define BUSSTOP_H

#include <string>

class BusStop {
private:
    int id;
    std::string name;

public:
    BusStop();
    BusStop(int id, const std::string& name);

    int getID() const;
    std::string getName() const;
    void setName(const std::string& n);

    void display() const;
};

#endif
