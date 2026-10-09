#ifndef STUDENT_MANAGER_H
#define STUDENT_MANAGER_H

#include <vector>
#include <string>
#include <unordered_map>
#include "student.h"
#include "department.h"
#include "hashTable.h"

class StudentManager {
private:
    HashTable<Student> students;      
    HashTable<Department> departments; 

public:
    bool addStudent(const Student& s);
    bool removeStudent(int id);
    bool updateStudent(int id, const Student& updated);
    Student* searchStudent(int id);                
    void displayStudents() const;

    bool addDepartment(const Department& d);
    Department* findDepartment(int id);         

    bool assignStudentToStop(int studentID, int stopID);
    std::vector<Student> findStudentsByStop(int stopID) const;
    void countStudentsByDepartment() const;          

    void sortStudentsByName() const;                 
    void sortStudentsByDepartment() const;

    void saveToFile(const std::string& filename = "data/students.dat") const;
    void loadFromFile(const std::string& filename = "data/students.dat");

    void saveDepartmentsToFile(const std::string& filename = "data/departments.dat") const;
    void loadDepartmentsFromFile(const std::string& filename = "data/departments.dat");

    std::vector<Student> getAllStudents() const;    
    bool studentIDExists(int id) const;
    bool departmentIDExists(int id) const;
};

#endif
