#ifndef STUDENT_H
#define STUDENT_H

#include <string>
class Student {
private:
    int id;
    std::string name;
    int departmentID;
    int busStopID;      
    int assignedRouteID;

public:
    Student();
    Student(int id, const std::string& name, int departmentID, int busStopID = -1);

    int getID() const;
    std::string getName() const;
    int getDepartmentID() const;
    int getBusStopID() const;
    int getAssignedRouteID() const;

    void setName(const std::string& n);
    void setDepartmentID(int deptID);
    void setBusStopID(int stopID);
    void setAssignedRouteID(int routeID);

    void display() const;
};

#endif
