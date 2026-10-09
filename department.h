#ifndef DEPARTMENT_H
#define DEPARTMENT_H

#include <string>

class Department {
private:
    int id;
    std::string name;

public:
    Department();
    Department(int id, const std::string& name);

    int getID() const;
    std::string getName() const;
    void setName(const std::string& n);

    void display() const;
};

#endif
