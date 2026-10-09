#include "studentManager.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

bool StudentManager::studentIDExists(int id) const { return students.exists(id); }
bool StudentManager::departmentIDExists(int id) const { return departments.exists(id); }

bool StudentManager::addStudent(const Student& s) {
    if (studentIDExists(s.getID())) return false;  
    students.insert(s.getID(), s);
    return true;
}

bool StudentManager::removeStudent(int id) {
    return students.remove(id);
}

bool StudentManager::updateStudent(int id, const Student& updated) {
    if (!studentIDExists(id)) return false;
    students.insert(id, updated); 
    return true;
}

Student* StudentManager::searchStudent(int id) {
    return students.find(id);
}

void StudentManager::displayStudents() const {
    auto all = students.getAll();
    if (all.empty()) { std::cout << "No students in the system.\n"; return; }
    for (const auto& s : all) s.display();
}

bool StudentManager::addDepartment(const Department& d) {
    if (departmentIDExists(d.getID())) return false;
    departments.insert(d.getID(), d);
    return true;
}

Department* StudentManager::findDepartment(int id) {
    return departments.find(id);
}

bool StudentManager::assignStudentToStop(int studentID, int stopID) {
    Student* s = students.find(studentID);
    if (!s) return false;
    s->setBusStopID(stopID);
    return true;
}

std::vector<Student> StudentManager::findStudentsByStop(int stopID) const {
    std::vector<Student> result;
    for (const auto& s : students.getAll()) {
        if (s.getBusStopID() == stopID) result.push_back(s);
    }
    return result;
}

void StudentManager::countStudentsByDepartment() const {
    std::unordered_map<int, int> counts; 
    for (const auto& s : students.getAll()) {
        counts[s.getDepartmentID()]++;
    }
    for (const auto& entry : counts) {
        std::cout << "Department #" << entry.first << ": " << entry.second << " student(s)\n";
    }
}

void StudentManager::sortStudentsByName() const {
    auto all = students.getAll();
    std::sort(all.begin(), all.end(),
              [](const Student& a, const Student& b) { return a.getName() < b.getName(); });
    for (const auto& s : all) s.display();
}

void StudentManager::sortStudentsByDepartment() const {
    auto all = students.getAll();
    std::sort(all.begin(), all.end(),
              [](const Student& a, const Student& b) { return a.getDepartmentID() < b.getDepartmentID(); });
    for (const auto& s : all) s.display();
}

void StudentManager::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) { std::cout << "Could not open " << filename << " for writing.\n"; return; }
    for (const auto& s : students.getAll()) {
        out << s.getID() << '|' << s.getName() << '|' << s.getDepartmentID() << '|'
            << s.getBusStopID() << '|' << s.getAssignedRouteID() << '\n';
    }
    out.close();
}

void StudentManager::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, name, deptStr, stopStr, routeStr;
        std::getline(ss, idStr, '|');
        std::getline(ss, name, '|');
        std::getline(ss, deptStr, '|');
        std::getline(ss, stopStr, '|');
        std::getline(ss, routeStr, '|');
        Student s(std::stoi(idStr), name, std::stoi(deptStr), std::stoi(stopStr));
        s.setAssignedRouteID(std::stoi(routeStr));
        students.insert(s.getID(), s);
    }
    in.close();
}

void StudentManager::saveDepartmentsToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) return;
    for (const auto& d : departments.getAll()) {
        out << d.getID() << '|' << d.getName() << '\n';
    }
    out.close();
}

void StudentManager::loadDepartmentsFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, name;
        std::getline(ss, idStr, '|');
        std::getline(ss, name, '|');
        departments.insert(std::stoi(idStr), Department(std::stoi(idStr), name));
    }
    in.close();
}

std::vector<Student> StudentManager::getAllStudents() const {
    return students.getAll();
}
