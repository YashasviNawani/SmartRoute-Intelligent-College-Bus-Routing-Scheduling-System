#include "user.h"
#include <iostream>

User::User() : username(""), salt(""), passwordHash(""), role(Role::STUDENT), linkedStudentID(-1) {}

User::User(const std::string& username, const std::string& salt,
           const std::string& passwordHash, Role role, int linkedStudentID)
    : username(username), salt(salt), passwordHash(passwordHash),
      role(role), linkedStudentID(linkedStudentID) {}

std::string User::getUsername() const { return username; }
std::string User::getSalt() const { return salt; }
std::string User::getPasswordHash() const { return passwordHash; }
Role User::getRole() const { return role; }
int User::getLinkedStudentID() const { return linkedStudentID; }

void User::setCredentials(const std::string& newSalt, const std::string& newHash) {
    salt = newSalt;
    passwordHash = newHash;
}

std::string User::roleToString(Role r) {
    switch (r) {
        case Role::ADMIN:   return "ADMIN";
        case Role::STAFF:   return "STAFF";
        case Role::STUDENT: return "STUDENT";
    }
    return "STUDENT";
}

bool User::parseRole(const std::string& text, Role& out) {
    if (text == "ADMIN")   { out = Role::ADMIN;   return true; }
    if (text == "STAFF")   { out = Role::STAFF;   return true; }
    if (text == "STUDENT") { out = Role::STUDENT; return true; }
    return false;
}

void User::display() const {
    std::cout << "User: " << username << " | Role: " << roleToString(role);
    if (role == Role::STUDENT) std::cout << " | Student ID: " << linkedStudentID;
    std::cout << "\n";
}
