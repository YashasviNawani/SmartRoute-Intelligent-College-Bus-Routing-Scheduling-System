#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <string>
#include <unordered_map>
#include "user.h"

// ---------------------------------------------------------------------------
// Class: AuthManager   (Authentication module)
// Demonstrates: hashing (unordered_map: username -> User), salted password
//               hashing, enum class, input validation, file handling
// Note: the hash is for learning. Real systems use bcrypt / Argon2.
// ---------------------------------------------------------------------------
class AuthManager {
private:
    std::unordered_map<std::string, User> users;   // username -> User
    std::string lastError;

    bool validUsername(const std::string& username);
    bool validPassword(const std::string& password);

public:
    static std::string hashPassword(const std::string& salt, const std::string& password);

    bool addUser(const std::string& username, const std::string& password,
                 Role role, int linkedStudentID = -1);
    bool removeUser(const std::string& username);        // refuses to remove the last admin
    bool changePassword(const std::string& username,
                        const std::string& oldPassword, const std::string& newPassword);
    bool resetPassword(const std::string& username, const std::string& newPassword);  // admin use
    bool login(const std::string& username, const std::string& password, User& out) const;

    bool usernameExists(const std::string& username) const;
    bool studentHasAccount(int studentID) const;
    int countAdmins() const;
    bool isEmpty() const;
    void createDefaultAdmin();                           // used when no users.dat exists yet
    void displayUsers() const;

    std::string getLastError() const;

    void saveToFile(const std::string& filename = "data/users.dat") const;
    void loadFromFile(const std::string& filename = "data/users.dat");
};

#endif
