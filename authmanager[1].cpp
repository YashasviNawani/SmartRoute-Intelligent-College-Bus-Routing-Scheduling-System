#include "authmanager.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <sstream>
#include <vector>

// ---------- helpers ----------

static std::string generateSalt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0u, 0xFFFFFFFFu);
    std::ostringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << dist(gen);
    return ss.str();
}

// Salted, repeated FNV-1a hash (64-bit). Repeating it makes guessing slower.
std::string AuthManager::hashPassword(const std::string& salt, const std::string& password) {
    unsigned long long h = 1469598103934665603ULL;
    std::string data = salt + password;
    for (int round = 0; round < 1000; round++) {
        for (std::size_t i = 0; i < data.size(); i++) {
            h ^= static_cast<unsigned char>(data[i]);
            h *= 1099511628211ULL;
        }
        data = std::to_string(h) + salt;       // feed the result into the next round
    }
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << h;
    return ss.str();
}

bool AuthManager::validUsername(const std::string& username) {
    if (username.size() < 3 || username.size() > 20) {
        lastError = "Username must be 3 to 20 characters long.";
        return false;
    }
    if (username == "exit") {
        lastError = "That username is reserved.";
        return false;
    }
    for (std::size_t i = 0; i < username.size(); i++) {
        char c = username[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok) {
            lastError = "Username may only contain letters, digits, '_' and '.'.";
            return false;
        }
    }
    return true;
}

bool AuthManager::validPassword(const std::string& password) {
    if (password.size() < 6) {
        lastError = "Password must be at least 6 characters long.";
        return false;
    }
    if (password.find('|') != std::string::npos) {
        lastError = "Password cannot contain the '|' character.";
        return false;
    }
    return true;
}

// ---------- user operations ----------

bool AuthManager::addUser(const std::string& username, const std::string& password,
                          Role role, int linkedStudentID) {
    if (!validUsername(username) || !validPassword(password)) return false;
    if (usernameExists(username)) { lastError = "That username is already taken."; return false; }
    if (role == Role::STUDENT) {
        if (linkedStudentID <= 0) { lastError = "A student account must be linked to a student ID."; return false; }
        if (studentHasAccount(linkedStudentID)) { lastError = "That student already has an account."; return false; }
    } else {
        linkedStudentID = -1;
    }
    std::string salt = generateSalt();
    users[username] = User(username, salt, hashPassword(salt, password), role, linkedStudentID);
    return true;
}

bool AuthManager::removeUser(const std::string& username) {
    auto it = users.find(username);
    if (it == users.end()) { lastError = "User not found."; return false; }
    if (it->second.getRole() == Role::ADMIN && countAdmins() <= 1) {
        lastError = "Cannot remove the last admin account.";
        return false;
    }
    users.erase(it);
    return true;
}

bool AuthManager::changePassword(const std::string& username,
                                 const std::string& oldPassword, const std::string& newPassword) {
    auto it = users.find(username);
    if (it == users.end()) { lastError = "User not found."; return false; }
    if (hashPassword(it->second.getSalt(), oldPassword) != it->second.getPasswordHash()) {
        lastError = "Current password is incorrect.";
        return false;
    }
    return resetPassword(username, newPassword);
}

bool AuthManager::resetPassword(const std::string& username, const std::string& newPassword) {
    auto it = users.find(username);
    if (it == users.end()) { lastError = "User not found."; return false; }
    if (!validPassword(newPassword)) return false;
    std::string salt = generateSalt();            // fresh salt on every password change
    it->second.setCredentials(salt, hashPassword(salt, newPassword));
    return true;
}

bool AuthManager::login(const std::string& username, const std::string& password, User& out) const {
    auto it = users.find(username);
    if (it == users.end()) return false;
    if (hashPassword(it->second.getSalt(), password) != it->second.getPasswordHash()) return false;
    out = it->second;
    return true;
}

bool AuthManager::usernameExists(const std::string& username) const {
    return users.find(username) != users.end();
}

bool AuthManager::studentHasAccount(int studentID) const {
    for (const auto& entry : users)
        if (entry.second.getRole() == Role::STUDENT && entry.second.getLinkedStudentID() == studentID)
            return true;
    return false;
}

int AuthManager::countAdmins() const {
    int count = 0;
    for (const auto& entry : users)
        if (entry.second.getRole() == Role::ADMIN) count++;
    return count;
}

bool AuthManager::isEmpty() const { return users.empty(); }

void AuthManager::createDefaultAdmin() {
    addUser("admin", "admin123", Role::ADMIN);
}

void AuthManager::displayUsers() const {
    if (users.empty()) { std::cout << "No users.\n"; return; }
    std::vector<User> all;
    for (const auto& entry : users) all.push_back(entry.second);
    std::sort(all.begin(), all.end(),
              [](const User& a, const User& b) { return a.getUsername() < b.getUsername(); });
    for (const auto& u : all) u.display();
}

std::string AuthManager::getLastError() const { return lastError; }

// ---------- file handling ----------
// users.dat : username|salt|passwordHash|role|linkedStudentID

void AuthManager::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out) { std::cout << "Could not open " << filename << " for writing.\n"; return; }
    for (const auto& entry : users) {
        const User& u = entry.second;
        out << u.getUsername() << '|' << u.getSalt() << '|' << u.getPasswordHash() << '|'
            << User::roleToString(u.getRole()) << '|' << u.getLinkedStudentID() << '\n';
    }
    out.close();
}

void AuthManager::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) return;                       // first run: no users yet
    users.clear();
    std::string lineText;
    while (std::getline(in, lineText)) {
        if (lineText.empty()) continue;
        std::stringstream ss(lineText);
        std::string name, salt, hash, roleStr, linkedStr;
        std::getline(ss, name, '|');
        std::getline(ss, salt, '|');
        std::getline(ss, hash, '|');
        std::getline(ss, roleStr, '|');
        std::getline(ss, linkedStr, '|');
        try {
            Role role;
            if (name.empty() || salt.empty() || hash.empty() || !User::parseRole(roleStr, role))
                throw std::invalid_argument("bad record");
            int linked = linkedStr.empty() ? -1 : std::stoi(linkedStr);
            if (users.find(name) == users.end())
                users[name] = User(name, salt, hash, role, linked);
        } catch (const std::exception&) {
            std::cout << "Skipped corrupted line in " << filename << "\n";
        }
    }
    in.close();
}
