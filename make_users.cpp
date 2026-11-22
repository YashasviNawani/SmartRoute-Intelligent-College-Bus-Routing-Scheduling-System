// Generates a sample users.dat using the same AuthManager the program uses.
// Build: g++ make_users.cpp user.cpp authmanager.cpp -o make_users
// Run:   ./make_users   (writes users.dat in the current folder)
#include "authmanager.h"
#include <iostream>

void generateUsersFile() {
    AuthManager auth;
    auth.addUser("admin",       "Admin@123",    Role::ADMIN);
    auth.addUser("transport",   "Staff@123",    Role::STAFF);
    auth.addUser("coordinator", "Staff@456",    Role::STAFF);
    auth.addUser("stu1001",     "Student@1001", Role::STUDENT, 1001);
    auth.addUser("stu1002",     "Student@1002", Role::STUDENT, 1002);
    auth.addUser("stu1003",     "Student@1003", Role::STUDENT, 1003);
    auth.addUser("stu1004",     "Student@1004", Role::STUDENT, 1004);
    auth.addUser("stu1005",     "Student@1005", Role::STUDENT, 1005);
    auth.saveToFile("users.dat");
    std::cout << "users.dat written.\n";
 
}
