#include "student.h"
#include <iostream>

Student::Student()
    : id(0), name(""), departmentID(-1), busStopID(-1), assignedRouteID(-1) {}

Student::Student(int id, const std::string& name, int departmentID, int busStopID)
    : id(id), name(name), departmentID(departmentID), busStopID(busStopID),
      assignedRouteID(-1) {}

int Student::getID() const { return id; }
std::string Student::getName() const { return name; }
int Student::getDepartmentID() const { return departmentID; }
int Student::getBusStopID() const { return busStopID; }
int Student::getAssignedRouteID() const { return assignedRouteID; }

void Student::setName(const std::string& n) { name = n; }
void Student::setDepartmentID(int deptID) { departmentID = deptID; }
void Student::setBusStopID(int stopID) { busStopID = stopID; }
void Student::setAssignedRouteID(int routeID) { assignedRouteID = routeID; }

void Student::display() const {
    std::cout << "Student #" << id << " | " << name
              << " | Dept ID: " << departmentID
              << " | Stop ID: " << busStopID
              << " | Route ID: " << (assignedRouteID == -1 ? "unassigned" : std::to_string(assignedRouteID))
              << std::endl;
}
