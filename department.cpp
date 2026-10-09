#include "department.h"
#include <iostream>

Department::Department() : id(0), name("") {}
Department::Department(int id, const std::string& name) : id(id), name(name) {}

int Department::getID() const { return id; }
std::string Department::getName() const { return name; }
void Department::setName(const std::string& n) { name = n; }

void Department::display() const {
    std::cout << "Department #" << id << " | " << name << std::endl;
}
