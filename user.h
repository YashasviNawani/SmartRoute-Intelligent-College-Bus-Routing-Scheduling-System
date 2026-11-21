#ifndef USER_H
#define USER_H

#include <string>

enum class Role { ADMIN, STAFF, STUDENT };

class User {
private:
    std::string username;
    std::string salt;            // random text mixed into the password before hashing
    std::string passwordHash;    // the password itself is never stored
    Role role;
    int linkedStudentID;         // only used when role == STUDENT, otherwise -1

public:
    User();
    User(const std::string& username, const std::string& salt,
         const std::string& passwordHash, Role role, int linkedStudentID = -1);

    std::string getUsername() const;
    std::string getSalt() const;
    std::string getPasswordHash() const;
    Role getRole() const;
    int getLinkedStudentID() const;

    void setCredentials(const std::string& newSalt, const std::string& newHash);

    static std::string roleToString(Role r);
    static bool parseRole(const std::string& text, Role& out);

    void display() const;
};

#endif
