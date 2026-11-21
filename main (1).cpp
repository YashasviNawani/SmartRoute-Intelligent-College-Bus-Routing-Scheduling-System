
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

#include "allocator.h"
#include "authmanager.h"
#include "busManager.h"
#include "conflict.h"
#include "graph.h"
#include "routemanager.h"
#include "scheduler.h"
#include "simulator.h"
#include "studentManager.h"

#ifdef _WIN32
#  include <conio.h>
#else
#  include <termios.h>
#  include <unistd.h>
#endif

// The program saves into a "data" folder. std::filesystem (C++17) is used to
// create it automatically when the compiler supports it.
#if defined(_MSVC_LANG)
#  define CPP_VERSION _MSVC_LANG
#else
#  define CPP_VERSION __cplusplus
#endif
#if defined(__has_include)
#  if __has_include(<filesystem>) && (CPP_VERSION >= 201703L)
#    include <filesystem>
#    define HAVE_STD_FILESYSTEM 1
#  endif
#endif

// ---------------------------------------------------------------------------
// File names
// ---------------------------------------------------------------------------
const std::string STOPS_FILE    = "data/stops.dat";
const std::string ROUTES_FILE   = "data/routes.dat";
const std::string SCHEDULE_FILE = "data/schedule.dat";   // schedule settings + travel times
const std::string TRIPS_FILE    = "data/trips.dat";      // manually added trips
const std::string USERS_FILE    = "data/users.dat";

// ---------------------------------------------------------------------------
// Everything the program keeps in memory
// ---------------------------------------------------------------------------
struct System {
    BusManager bm;
    StudentManager sm;
    RouteManager rm;
    Allocator alloc;
    AuthManager auth;
    ScheduleConfig cfg;               // schedule settings + per-pair travel times
    std::vector<Trip> manualTrips;    // trips typed in by staff
    ScheduleResult lastSchedule;      // result of the last "run schedule"
    bool haveSchedule = false;

    System() : alloc(bm, sm, rm) {}   // bm, sm, rm are declared above alloc
};

// ---------------------------------------------------------------------------
// Safe input helpers (a wrong key never crashes the program)
// ---------------------------------------------------------------------------
struct InputClosed {};   // thrown when input ends (Ctrl+D / Ctrl+Z)

static std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) throw InputClosed();
    return trim(line);
}

static int readInt(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        try {
            std::size_t pos = 0;
            int value = std::stoi(s, &pos);
            if (pos == s.size()) return value;
        } catch (const std::exception&) {}
        std::cout << "Please enter a valid whole number.\n";
    }
}

static double readPositiveDouble(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        try {
            std::size_t pos = 0;
            double value = std::stod(s, &pos);
            if (pos == s.size() && value > 0) return value;
        } catch (const std::exception&) {}
        std::cout << "Please enter a number greater than 0.\n";
    }
}

// Text must not be empty and must not contain '|' (it is the file separator)
static std::string readText(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        if (!s.empty() && s.find('|') == std::string::npos) return s;
        std::cout << "Text cannot be empty or contain the '|' character.\n";
    }
}

// Time of day as HH:MM (24 hour), returned as minutes since midnight
static int readTime(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        std::size_t colon = s.find(':');
        if (colon != std::string::npos) {
            try {
                std::size_t p1 = 0, p2 = 0;
                std::string hh = s.substr(0, colon), mm = s.substr(colon + 1);
                int h = std::stoi(hh, &p1);
                int m = std::stoi(mm, &p2);
                if (p1 == hh.size() && p2 == mm.size() && h >= 0 && h < 24 && m >= 0 && m < 60)
                    return h * 60 + m;
            } catch (const std::exception&) {}
        }
        std::cout << "Please enter a time as HH:MM, for example 08:30.\n";
    }
}

// Password input that is not shown on screen
static std::string readPassword(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string pw;
#ifdef _WIN32
    int ch;
    while ((ch = _getch()) != '\r' && ch != '\n') {
        if (ch == 3) throw InputClosed();                 // Ctrl+C
        if (ch == 0 || ch == 224) { _getch(); continue; } // function / arrow keys
        if (ch == '\b') { if (!pw.empty()) pw.pop_back(); }
        else pw.push_back(static_cast<char>(ch));
    }
    std::cout << "\n";
#else
    termios oldSettings;
    bool isTerminal = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &oldSettings) == 0;
    if (isTerminal) {
        termios hidden = oldSettings;
        hidden.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &hidden);
    }
    bool ok = static_cast<bool>(std::getline(std::cin, pw));
    if (isTerminal) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
        std::cout << "\n";
    }
    if (!ok) throw InputClosed();
    if (!pw.empty() && pw.back() == '\r') pw.pop_back();
#endif
    return pw;
}

static void line() { std::cout << "----------------------------------------\n"; }

static std::string stopLabel(RouteManager& rm, int stopID) {
    BusStop* s = rm.searchStop(stopID);
    return s ? s->getName() + " (#" + std::to_string(stopID) + ")" : "Stop #" + std::to_string(stopID);
}

// ---------------------------------------------------------------------------
// Load / save everything
// ---------------------------------------------------------------------------
static void ensureDataFolder() {
#ifdef HAVE_STD_FILESYSTEM
    std::error_code ec;
    std::filesystem::create_directories("data", ec);
    if (ec) std::cout << "Warning: could not create the 'data' folder.\n";
#else
    std::cout << "Note: make sure a folder named 'data' exists next to the program, "
                 "otherwise data cannot be saved.\n";
#endif
}

// schedule.dat:  CONFIG|collegeStart|collegeEnd|defaultEdgeMinutes|dwell|autoReinforce
//                EDGE|stopA|stopB|minutes
static void saveSchedule(const System& sys) {
    std::ofstream out(SCHEDULE_FILE);
    if (!out) { std::cout << "Could not write " << SCHEDULE_FILE << ".\n"; return; }
    const ScheduleConfig& c = sys.cfg;
    out << "CONFIG|" << c.collegeStartMin << '|' << c.collegeEndMin << '|' << c.defaultEdgeMinutes
        << '|' << c.dwellMinutesPerStop << '|' << (c.autoReinforce ? 1 : 0) << '\n';
    for (const EdgeWeight& e : c.edgeWeights) out << "EDGE|" << e.a << '|' << e.b << '|' << e.weight << '\n';
}

static void loadSchedule(System& sys) {
    std::ifstream in(SCHEDULE_FILE);
    if (!in) return;
    std::string text;
    while (std::getline(in, text)) {
        if (text.empty()) continue;
        std::stringstream ss(text);
        std::vector<std::string> f;
        std::string part;
        while (std::getline(ss, part, '|')) f.push_back(part);
        try {
            if (f[0] == "CONFIG" && f.size() >= 6) {
                sys.cfg.collegeStartMin = std::stoi(f[1]);
                sys.cfg.collegeEndMin = std::stoi(f[2]);
                sys.cfg.defaultEdgeMinutes = std::stod(f[3]);
                sys.cfg.dwellMinutesPerStop = std::stoi(f[4]);
                sys.cfg.autoReinforce = (std::stoi(f[5]) != 0);
            } else if (f[0] == "EDGE" && f.size() >= 4) {
                EdgeWeight e;
                e.a = std::stoi(f[1]);
                e.b = std::stoi(f[2]);
                e.weight = std::stod(f[3]);
                if (e.weight > 0) sys.cfg.edgeWeights.push_back(e);
            }
        } catch (const std::exception&) {
            std::cout << "Skipped a corrupted line in " << SCHEDULE_FILE << ".\n";
        }
    }
}

// trips.dat:  routeID|busID|startMin|endMin|label
static void saveTrips(const System& sys) {
    std::ofstream out(TRIPS_FILE);
    if (!out) { std::cout << "Could not write " << TRIPS_FILE << ".\n"; return; }
    for (const Trip& t : sys.manualTrips)
        out << t.routeID << '|' << t.busID << '|' << t.startMin << '|' << t.endMin << '|' << t.label << '\n';
}

static void loadTrips(System& sys) {
    std::ifstream in(TRIPS_FILE);
    if (!in) return;
    std::string text;
    while (std::getline(in, text)) {
        if (text.empty()) continue;
        std::stringstream ss(text);
        std::string r, b, s, e, label;
        std::getline(ss, r, '|'); std::getline(ss, b, '|'); std::getline(ss, s, '|');
        std::getline(ss, e, '|'); std::getline(ss, label, '|');
        try {
            Trip t;
            t.routeID = std::stoi(r); t.busID = std::stoi(b);
            t.startMin = std::stoi(s); t.endMin = std::stoi(e);
            t.label = label;
            if (t.endMin > t.startMin) sys.manualTrips.push_back(t);
        } catch (const std::exception&) {
            std::cout << "Skipped a corrupted line in " << TRIPS_FILE << ".\n";
        }
    }
}

static void loadAll(System& sys) {
    // Order matters: routes need stops, allocations need buses and routes
    sys.bm.loadFromFile();
    sys.rm.loadStopsFromFile(STOPS_FILE);
    sys.rm.loadRoutesFromFile(ROUTES_FILE);
    sys.sm.loadDepartmentsFromFile();
    sys.sm.loadFromFile();
    sys.alloc.loadFromFile();
    loadSchedule(sys);
    loadTrips(sys);
}

static void saveAll(System& sys) {
    sys.bm.saveToFile();
    sys.rm.saveStopsToFile(STOPS_FILE);
    sys.rm.saveRoutesToFile(ROUTES_FILE);
    sys.sm.saveDepartmentsToFile();
    sys.sm.saveToFile();
    sys.alloc.saveToFile();
    saveSchedule(sys);
    saveTrips(sys);
    sys.auth.saveToFile(USERS_FILE);
}

// ---------------------------------------------------------------------------
// 1-4. Original modules (unchanged behaviour)
// ---------------------------------------------------------------------------
static void busMenu(BusManager& bm) {
    int choice;
    do {
        line();
        std::cout << "BUS MANAGEMENT\n"
                  << " 1. Add a bus\n 2. Remove a bus\n 3. Update bus details\n 4. Search for a bus\n"
                  << " 5. Display all buses\n 6. Check bus capacity\n 7. Display available buses\n"
                  << " 8. Change bus availability\n 9. Sort buses by capacity\n10. Sort buses by ID\n"
                  << " 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            int id = readInt("Bus ID: ");
            if (id <= 0) { std::cout << "Bus ID must be positive.\n"; continue; }
            if (bm.busIDExists(id)) { std::cout << "A bus with this ID already exists.\n"; continue; }
            std::string reg = readText("Registration number: ");
            std::string driver = readText("Driver name: ");
            int cap = readInt("Capacity: ");
            if (cap <= 0) { std::cout << "Capacity must be positive.\n"; continue; }
            bm.addBus(Bus(id, reg, driver, cap));
            std::cout << "Bus added.\n";
        } else if (choice == 2) {
            int id = readInt("Bus ID to remove: ");
            Bus* b = bm.searchBus(id);
            if (!b) { std::cout << "Bus not found.\n"; continue; }
            if (b->getCurrentStudents() > 0) {
                std::cout << "This bus still carries students. Release them first, or mark the bus unavailable.\n";
                continue;
            }
            bm.removeBus(id);
            std::cout << "Bus removed.\n";
        } else if (choice == 3) {
            int id = readInt("Bus ID to update: ");
            Bus* b = bm.searchBus(id);
            if (!b) { std::cout << "Bus not found.\n"; continue; }
            std::string reg = readText("New registration number: ");
            std::string driver = readText("New driver name: ");
            int cap = readInt("New capacity: ");
            if (cap <= 0 || cap < b->getCurrentStudents()) {
                std::cout << "Capacity must be positive and not below the current " << b->getCurrentStudents() << " students.\n";
                continue;
            }
            b->setRegistrationNo(reg);
            b->setDriverName(driver);
            b->setCapacity(cap);
            std::cout << "Bus updated.\n";
        } else if (choice == 4) {
            Bus* b = bm.searchBus(readInt("Bus ID: "));
            if (b) b->display(); else std::cout << "Bus not found.\n";
        } else if (choice == 5) {
            bm.displayAllBuses();
        } else if (choice == 6) {
            int id = readInt("Bus ID: ");
            int seats = bm.getAvailableSeats(id);
            if (seats < 0) std::cout << "Bus not found.\n";
            else std::cout << "Free seats: " << seats << (seats == 0 ? " (bus is full)" : "") << "\n";
        } else if (choice == 7) {
            bm.displayAvailableBuses();
        } else if (choice == 8) {
            int id = readInt("Bus ID: ");
            int status = readInt("Enter 1 for available, 0 for unavailable: ");
            if (status != 0 && status != 1) { std::cout << "Please enter 1 or 0.\n"; continue; }
            if (bm.changeAvailability(id, status == 1)) std::cout << "Availability updated.\n";
            else std::cout << "Bus not found.\n";
        } else if (choice == 9) {
            bm.sortBusesByCapacity();
            bm.displayAllBuses();
        } else if (choice == 10) {
            bm.sortBusesByID();
            bm.displayAllBuses();
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

static void studentMenu(StudentManager& sm, RouteManager& rm, Allocator& alloc) {
    int choice;
    do {
        line();
        std::cout << "STUDENT & DEPARTMENT MANAGEMENT\n"
                  << " 1. Add a student\n 2. Remove a student\n 3. Update a student\n 4. Search for a student\n"
                  << " 5. Display all students\n 6. Sort students by name\n 7. Sort students by department\n"
                  << " 8. Add a department\n 9. Find a department\n10. Assign student to a bus stop\n"
                  << "11. Find students at a stop\n12. Count students by department\n 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            int id = readInt("Student ID: ");
            if (id <= 0) { std::cout << "Student ID must be positive.\n"; continue; }
            if (sm.studentIDExists(id)) { std::cout << "A student with this ID already exists.\n"; continue; }
            std::string name = readText("Name: ");
            int dept = readInt("Department ID: ");
            if (!sm.departmentIDExists(dept)) { std::cout << "That department does not exist. Add it first.\n"; continue; }
            int stop = readInt("Bus stop ID (-1 for none): ");
            if (stop != -1 && !rm.stopIDExists(stop)) { std::cout << "That bus stop does not exist.\n"; continue; }
            sm.addStudent(Student(id, name, dept, stop));
            std::cout << "Student added.\n";
        } else if (choice == 2) {
            int id = readInt("Student ID to remove: ");
            if (!sm.studentIDExists(id)) { std::cout << "Student not found.\n"; continue; }
            alloc.releaseStudent(id);          // give back the bus seat, if any
            sm.removeStudent(id);
            std::cout << "Student removed.\n";
        } else if (choice == 3) {
            Student* s = sm.searchStudent(readInt("Student ID to update: "));
            if (!s) { std::cout << "Student not found.\n"; continue; }
            std::string name = readText("New name: ");
            int dept = readInt("New department ID: ");
            if (!sm.departmentIDExists(dept)) { std::cout << "That department does not exist.\n"; continue; }
            s->setName(name);
            s->setDepartmentID(dept);
            std::cout << "Student updated.\n";
        } else if (choice == 4) {
            Student* s = sm.searchStudent(readInt("Student ID: "));
            if (s) s->display(); else std::cout << "Student not found.\n";
        } else if (choice == 5) {
            sm.displayStudents();
        } else if (choice == 6) {
            sm.sortStudentsByName();
        } else if (choice == 7) {
            sm.sortStudentsByDepartment();
        } else if (choice == 8) {
            int id = readInt("Department ID: ");
            if (id <= 0) { std::cout << "Department ID must be positive.\n"; continue; }
            std::string name = readText("Department name: ");
            if (sm.addDepartment(Department(id, name))) std::cout << "Department added.\n";
            else std::cout << "A department with this ID already exists.\n";
        } else if (choice == 9) {
            Department* d = sm.findDepartment(readInt("Department ID: "));
            if (d) d->display(); else std::cout << "Department not found.\n";
        } else if (choice == 10) {
            int sid = readInt("Student ID: ");
            Student* s = sm.searchStudent(sid);
            if (!s) { std::cout << "Student not found.\n"; continue; }
            int stop = readInt("Bus stop ID: ");
            if (!rm.stopIDExists(stop)) { std::cout << "That bus stop does not exist.\n"; continue; }
            if (s->getAssignedRouteID() != -1) {
                alloc.releaseStudent(sid);     // the old seat no longer matches the new stop
                std::cout << "Previous seat released. Run allocation again for this student.\n";
            }
            sm.assignStudentToStop(sid, stop);
            std::cout << "Student assigned to stop " << stop << ".\n";
        } else if (choice == 11) {
            std::vector<Student> list = sm.findStudentsByStop(readInt("Bus stop ID: "));
            if (list.empty()) std::cout << "No students use this stop.\n";
            for (const auto& s : list) s.display();
        } else if (choice == 12) {
            sm.countStudentsByDepartment();
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

static void routeMenu(RouteManager& rm, BusManager& bm, StudentManager& sm, Allocator& alloc) {
    int choice;
    do {
        line();
        std::cout << "BUS STOPS & ROUTES\n"
                  << " 1. Add a bus stop\n 2. Remove a bus stop\n 3. Update a bus stop\n 4. Display bus stops\n"
                  << " 5. Create a route\n 6. Remove a route\n 7. Add a stop to a route\n"
                  << " 8. Remove a stop from a route\n 9. Display a route\n10. Display all routes\n"
                  << "11. Check if a stop is on a route\n 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            int id = readInt("Stop ID: ");
            std::string name = readText("Stop name: ");
            if (rm.addStop(BusStop(id, name))) std::cout << "Bus stop added.\n";
            else std::cout << "Could not add stop (ID must be positive and not already used).\n";
        } else if (choice == 2) {
            int id = readInt("Stop ID to remove: ");
            if (!rm.stopIDExists(id)) { std::cout << "Stop not found.\n"; continue; }
            // students using this stop lose their seat and their stop
            for (const auto& s : sm.findStudentsByStop(id)) {
                alloc.releaseStudent(s.getID());
                sm.assignStudentToStop(s.getID(), -1);
            }
            rm.removeStop(id);                 // also removes it from every route
            std::cout << "Stop removed (and removed from all routes).\n";
        } else if (choice == 3) {
            int id = readInt("Stop ID to update: ");
            std::string name = readText("New name: ");
            if (rm.updateStop(id, name)) std::cout << "Stop updated.\n";
            else std::cout << "Stop not found.\n";
        } else if (choice == 4) {
            rm.displayStops();
        } else if (choice == 5) {
            int id = readInt("Route ID: ");
            std::string name = readText("Route name: ");
            if (rm.createRoute(Route(id, name))) std::cout << "Route created. Now add stops to it.\n";
            else std::cout << "Could not create route (ID must be positive and not already used).\n";
        } else if (choice == 6) {
            int id = readInt("Route ID to remove: ");
            if (!rm.routeIDExists(id)) { std::cout << "Route not found.\n"; continue; }
            for (const auto& s : sm.getAllStudents()) {
                if (s.getAssignedRouteID() == id) {
                    std::cout << "Students are still allocated to this route. Release them first.\n";
                    id = -1;
                    break;
                }
            }
            if (id == -1) continue;
            for (Bus& b : bm.getBuses())       // buses on this route become unassigned
                if (alloc.getRouteOfBus(b.getID()) == id) alloc.unassignBus(b.getID());
            rm.removeRoute(id);
            std::cout << "Route removed.\n";
        } else if (choice == 7) {
            int rid = readInt("Route ID: ");
            int sid = readInt("Stop ID to add: ");
            if (rm.addStopToRoute(rid, sid)) std::cout << "Stop added to route.\n";
            else std::cout << "Failed: the route and stop must exist, and the stop must not already be on the route.\n";
        } else if (choice == 8) {
            int rid = readInt("Route ID: ");
            int sid = readInt("Stop ID to remove: ");
            if (rm.removeStopFromRoute(rid, sid)) std::cout << "Stop removed from route.\n";
            else std::cout << "Failed: route not found or stop is not on that route.\n";
        } else if (choice == 9) {
            rm.displayRoute(readInt("Route ID: "));
        } else if (choice == 10) {
            rm.displayAllRoutes();
        } else if (choice == 11) {
            int rid = readInt("Route ID: ");
            int sid = readInt("Stop ID: ");
            if (!rm.routeIDExists(rid)) std::cout << "Route not found.\n";
            else std::cout << (rm.isStopInRoute(rid, sid) ? "Yes, the stop is on this route.\n"
                                                           : "No, the stop is not on this route.\n");
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

static void allocationMenu(Allocator& alloc) {
    int choice;
    do {
        line();
        std::cout << "CAPACITY & BUS ALLOCATION\n"
                  << " 1. Assign a bus to a route\n 2. Unassign a bus from its route\n 3. Show the route of a bus\n"
                  << " 4. Check bus capacity\n 5. Update bus capacity\n 6. Add one student to the allocation queue\n"
                  << " 7. Queue all unallocated students\n 8. Process the queue (allocate students)\n"
                  << " 9. Release a student's seat\n10. Show waiting list\n11. Retry waiting list\n"
                  << "12. Show route priority (highest demand first)\n13. Allocation report\n 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            int bid = readInt("Bus ID: ");
            int rid = readInt("Route ID: ");
            if (alloc.assignBusToRoute(bid, rid)) std::cout << "Bus " << bid << " assigned to route " << rid << ".\n";
            else std::cout << "Failed: both the bus and the route must exist.\n";
        } else if (choice == 2) {
            if (alloc.unassignBus(readInt("Bus ID: "))) std::cout << "Bus unassigned.\n";
            else std::cout << "That bus was not assigned to any route.\n";
        } else if (choice == 3) {
            int bid = readInt("Bus ID: ");
            int rid = alloc.getRouteOfBus(bid);
            if (rid == -1) std::cout << "That bus is not assigned to any route.\n";
            else std::cout << "Bus " << bid << " serves route " << rid << ".\n";
        } else if (choice == 4) {
            int bid = readInt("Bus ID: ");
            std::cout << (alloc.checkCapacity(bid) ? "The bus has free seats.\n"
                                                    : "The bus is full, or does not exist.\n");
        } else if (choice == 5) {
            int bid = readInt("Bus ID: ");
            int cap = readInt("New capacity: ");
            if (alloc.updateCapacity(bid, cap)) std::cout << "Capacity updated.\n";
            else std::cout << "Failed: bus not found, or capacity is below the current number of students.\n";
        } else if (choice == 6) {
            if (alloc.addStudentToQueue(readInt("Student ID: "))) std::cout << "Student added to the queue.\n";
            else std::cout << "Not added (student missing, no stop, already allocated, or already queued).\n";
        } else if (choice == 7) {
            std::cout << alloc.queueAllUnallocatedStudents() << " student(s) added to the queue.\n";
        } else if (choice == 8) {
            int seated = alloc.processAllocationQueue();
            std::cout << seated << " student(s) seated. Waiting list: " << alloc.getWaitingCount() << ".\n";
        } else if (choice == 9) {
            if (alloc.releaseStudent(readInt("Student ID: "))) std::cout << "Seat released.\n";
            else std::cout << "That student has no allocated seat.\n";
        } else if (choice == 10) {
            alloc.displayWaitingList();
        } else if (choice == 11) {
            int seated = alloc.retryWaitingList();
            std::cout << seated << " student(s) seated on retry. Waiting list: " << alloc.getWaitingCount() << ".\n";
        } else if (choice == 12) {
            alloc.prioritizeRoutes();
        } else if (choice == 13) {
            alloc.displayAllocationReport();
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// ---------------------------------------------------------------------------
// 5. Route Optimization menu (Dijkstra, BFS, DFS)
// ---------------------------------------------------------------------------
static Graph currentGraph(System& sys) {
    return buildGraph(sys.rm.getAllStops(), sys.rm.getAllRoutes(),
                      sys.cfg.edgeWeights, sys.cfg.defaultEdgeMinutes);
}

static void showRoadNetwork(System& sys) {
    Graph g = currentGraph(sys);
    std::vector<int> ids = g.nodes();
    std::sort(ids.begin(), ids.end());
    if (ids.empty()) { std::cout << "No bus stops yet.\n"; return; }
    std::cout << "Road network (" << g.nodeCount() << " stops, " << g.edgeCount() << " links; times in minutes):\n";
    for (int id : ids) {
        std::cout << "  " << stopLabel(sys.rm, id) << " -> ";
        const std::vector<Edge>& nb = g.neighbors(id);
        if (nb.empty()) std::cout << "(not on any route)";
        for (std::size_t i = 0; i < nb.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << stopLabel(sys.rm, nb[i].to) << " [" << nb[i].weight << "]";
        }
        std::cout << "\n";
    }
}

static void setTravelTime(System& sys) {
    int a = readInt("First stop ID: ");
    int b = readInt("Second stop ID: ");
    if (!sys.rm.stopIDExists(a) || !sys.rm.stopIDExists(b)) { std::cout << "Both stops must exist.\n"; return; }
    Graph g = currentGraph(sys);
    if (!g.hasEdge(a, b)) {
        std::cout << "These stops are not next to each other on any route. "
                     "Travel times can only be set between neighbouring stops.\n";
        return;
    }
    double minutes = readPositiveDouble("Travel time in minutes: ");
    std::vector<EdgeWeight>& w = sys.cfg.edgeWeights;
    w.erase(std::remove_if(w.begin(), w.end(), [&](const EdgeWeight& e) {
                return (e.a == a && e.b == b) || (e.a == b && e.b == a);
            }), w.end());
    EdgeWeight e;
    e.a = a; e.b = b; e.weight = minutes;
    w.push_back(e);
    std::cout << "Travel time between " << stopLabel(sys.rm, a) << " and " << stopLabel(sys.rm, b)
              << " set to " << minutes << " min.\n";
}

static void optimizationMenu(System& sys) {
    int choice;
    do {
        line();
        std::cout << "ROUTE OPTIMIZATION\n"
                  << " 1. Most efficient path between two stops (Dijkstra)\n"
                  << " 2. Explore stops from a starting stop (BFS)\n"
                  << " 3. Explore stops from a starting stop (DFS)\n"
                  << " 4. Check whether one stop can reach another\n"
                  << " 5. Set travel time between two neighbouring stops\n"
                  << " 6. Show the road network\n"
                  << " 7. Show the trip duration of every route\n"
                  << " 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            int from = readInt("From stop ID: ");
            int to = readInt("To stop ID: ");
            showEfficientRoute(sys.rm, from, to, sys.cfg);
        } else if (choice == 2) {
            showStopTraversal(sys.rm, readInt("Start stop ID: "), true, sys.cfg);
        } else if (choice == 3) {
            showStopTraversal(sys.rm, readInt("Start stop ID: "), false, sys.cfg);
        } else if (choice == 4) {
            int from = readInt("From stop ID: ");
            int to = readInt("To stop ID: ");
            if (!sys.rm.stopIDExists(from) || !sys.rm.stopIDExists(to)) { std::cout << "Both stops must exist.\n"; continue; }
            Graph g = currentGraph(sys);
            std::cout << (isReachable(g, from, to) ? "Yes, a bus can get from one stop to the other.\n"
                                                   : "No, there is no connection between these stops.\n");
        } else if (choice == 5) {
            setTravelTime(sys);
        } else if (choice == 6) {
            showRoadNetwork(sys);
        } else if (choice == 7) {
            std::vector<Route> routes = sys.rm.getAllRoutes();
            if (routes.empty()) std::cout << "No routes defined.\n";
            Graph g = currentGraph(sys);
            for (const Route& r : routes)
                std::cout << "Route " << r.getID() << " (" << r.getName() << "): "
                          << tripDuration(r, g, sys.cfg) << " min for " << r.getStopCount() << " stop(s)\n";
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// ---------------------------------------------------------------------------
// 6. Scheduling & Conflict Detection menu
// ---------------------------------------------------------------------------
static void printConflicts(const std::vector<Conflict>& conflicts) {
    if (conflicts.empty()) { std::cout << "No scheduling conflicts.\n"; return; }
    std::cout << "Conflicts (" << conflicts.size() << "):\n";
    for (const Conflict& c : conflicts)
        std::cout << "  [" << (c.severe ? "ERROR" : "warning") << "] " << c.description << "\n";
}

static void addManualTrip(System& sys) {
    int rid = readInt("Route ID: ");
    if (!sys.rm.routeIDExists(rid)) { std::cout << "Route not found.\n"; return; }
    int bid = readInt("Bus ID: ");
    if (!sys.bm.busIDExists(bid)) { std::cout << "Bus not found.\n"; return; }
    int start = readTime("Start time (HH:MM): ");
    int end = readTime("End time (HH:MM): ");
    if (end <= start) { std::cout << "The trip must end after it starts.\n"; return; }
    Trip t;
    t.routeID = rid; t.busID = bid; t.startMin = start; t.endMin = end;
    t.label = readText("Label (for example Special): ");
    sys.manualTrips.push_back(t);
    std::cout << "Trip added.\n";
    // warn straight away if the new trip clashes with an existing manual trip
    std::vector<Conflict> now = detectConflicts(sys.manualTrips, sys.bm.getBuses(), sys.rm.getAllRoutes());
    for (const Conflict& c : now)
        if (c.severe && c.busID == bid) { std::cout << "Note: " << c.description << "\n"; break; }
}

static void showManualTrips(System& sys) {
    if (sys.manualTrips.empty()) { std::cout << "No manual trips.\n"; return; }
    std::vector<Trip> sorted = sys.manualTrips;
    std::sort(sorted.begin(), sorted.end(), [](const Trip& x, const Trip& y) { return x.startMin < y.startMin; });
    for (std::size_t i = 0; i < sorted.size(); ++i)
        std::cout << (i + 1) << ". Bus " << sorted[i].busID << " on route " << sorted[i].routeID
                  << ", " << formatMinutes(sorted[i].startMin) << " - " << formatMinutes(sorted[i].endMin)
                  << " (" << sorted[i].label << ")\n";
}

static void scheduleSettings(System& sys) {
    ScheduleConfig& c = sys.cfg;
    int choice;
    do {
        line();
        std::cout << "SCHEDULE SETTINGS\n"
                  << " 1. Morning trips arrive by      : " << formatMinutes(c.collegeStartMin) << "\n"
                  << " 2. Evening trips leave at       : " << formatMinutes(c.collegeEndMin) << "\n"
                  << " 3. Loading time per stop (min)  : " << c.dwellMinutesPerStop << "\n"
                  << " 4. Default travel time (min)    : " << c.defaultEdgeMinutes << "\n"
                  << " 5. Auto-reinforce busy routes   : " << (c.autoReinforce ? "ON" : "OFF") << "\n"
                  << " 0. Back\n";
        choice = readInt("Choice: ");
        if (choice == 1) c.collegeStartMin = readTime("Arrival time (HH:MM): ");
        else if (choice == 2) c.collegeEndMin = readTime("Departure time (HH:MM): ");
        else if (choice == 3) {
            int d = readInt("Minutes per stop: ");
            if (d < 0) std::cout << "Cannot be negative.\n"; else c.dwellMinutesPerStop = d;
        } else if (choice == 4) c.defaultEdgeMinutes = readPositiveDouble("Minutes between stops: ");
        else if (choice == 5) c.autoReinforce = !c.autoReinforce;
        else if (choice != 0) std::cout << "Invalid choice.\n";
        if (c.collegeEndMin <= c.collegeStartMin && (choice == 1 || choice == 2))
            std::cout << "Warning: the evening departure should be later than the morning arrival.\n";
    } while (choice != 0);
}

static void schedulingMenu(System& sys) {
    int choice;
    do {
        line();
        std::cout << "SCHEDULING & CONFLICT DETECTION\n"
                  << " 1. Run the scheduler (seats students, reinforces busy routes, builds trips)\n"
                  << " 2. Show the last schedule\n"
                  << " 3. Add a manual trip\n"
                  << " 4. Remove a manual trip\n"
                  << " 5. Show manual trips\n"
                  << " 6. Check conflicts (last schedule + manual trips)\n"
                  << " 7. Schedule settings\n"
                  << " 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            sys.lastSchedule = runSchedule(sys.rm, sys.bm, sys.sm, sys.alloc, sys.cfg);
            sys.haveSchedule = true;
            displaySchedule(sys.lastSchedule, sys.rm);
        } else if (choice == 2) {
            if (sys.haveSchedule) displaySchedule(sys.lastSchedule, sys.rm);
            else std::cout << "No schedule has been run yet.\n";
        } else if (choice == 3) {
            addManualTrip(sys);
        } else if (choice == 4) {
            showManualTrips(sys);
            if (sys.manualTrips.empty()) continue;
            // numbers refer to the list sorted by start time, as shown above
            std::vector<Trip> sorted = sys.manualTrips;
            std::sort(sorted.begin(), sorted.end(), [](const Trip& x, const Trip& y) { return x.startMin < y.startMin; });
            int n = readInt("Trip number to remove: ");
            if (n < 1 || n > static_cast<int>(sorted.size())) { std::cout << "No such trip.\n"; continue; }
            const Trip& victim = sorted[n - 1];
            for (std::size_t i = 0; i < sys.manualTrips.size(); ++i) {
                const Trip& t = sys.manualTrips[i];
                if (t.routeID == victim.routeID && t.busID == victim.busID &&
                    t.startMin == victim.startMin && t.endMin == victim.endMin && t.label == victim.label) {
                    sys.manualTrips.erase(sys.manualTrips.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
            }
            std::cout << "Trip removed.\n";
        } else if (choice == 5) {
            showManualTrips(sys);
        } else if (choice == 6) {
            std::vector<Trip> all = sys.manualTrips;
            if (sys.haveSchedule)
                all.insert(all.end(), sys.lastSchedule.assignments.begin(), sys.lastSchedule.assignments.end());
            if (all.empty()) { std::cout << "There are no trips to check. Run the scheduler or add a manual trip.\n"; continue; }
            std::cout << "Checking " << all.size() << " trip(s)...\n";
            printConflicts(detectConflicts(all, sys.bm.getBuses(), sys.rm.getAllRoutes()));
        } else if (choice == 7) {
            scheduleSettings(sys);
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// ---------------------------------------------------------------------------
// 7. What-If Simulator menu (runs on a copy; live data is never changed)
// ---------------------------------------------------------------------------
static void simulatorMenu(System& sys) {
    int choice;
    do {
        line();
        std::cout << "WHAT-IF SIMULATOR (live data is never changed)\n"
                  << " 1. What if a bus is unavailable?\n"
                  << " 2. What if extra students join a route?\n"
                  << " 3. What if a bus stop becomes unreachable?\n"
                  << " 0. Back\n";
        choice = readInt("Choice: ");
        if (choice < 1 || choice > 3) {
            if (choice != 0) std::cout << "Invalid choice.\n";
            continue;
        }
        Scenario sc;
        sc.amount = 0;
        if (choice == 1) {
            sc.type = ScenarioType::BusUnavailable;
            sc.targetID = readInt("Bus ID: ");
        } else if (choice == 2) {
            sc.type = ScenarioType::DemandIncrease;
            sc.targetID = readInt("Route ID: ");
            sc.amount = readInt("Number of extra students: ");
        } else {
            sc.type = ScenarioType::StopUnreachable;
            sc.targetID = readInt("Stop ID: ");
        }
        Simulator sim(sys.bm, sys.rm, sys.sm, sys.alloc, sys.cfg);
        sim.displayReport(sim.run(sc));
    } while (choice != 0);
}

// ---------------------------------------------------------------------------
// 8. Demand Analysis
// ---------------------------------------------------------------------------
static void demandAnalysis(System& sys) {
    Simulator sim(sys.bm, sys.rm, sys.sm, sys.alloc, sys.cfg);
    std::vector<RouteDemandInfo> info = sim.analyzeDemand();
    if (info.empty()) return;

    int spare = 0;
    for (Bus& b : sys.bm.getBuses())
        if (b.isAvailable() && sys.alloc.getRouteOfBus(b.getID()) == -1) spare++;

    bool any = false;
    for (const RouteDemandInfo& r : info) {
        if (r.status == DemandStatus::Overburdened) {
            std::cout << "Route " << r.routeID << " is overburdened: " << (r.demand - r.capacity)
                      << " seat(s) short.\n";
            any = true;
        } else if (r.status == DemandStatus::NoBus && r.demand > 0) {
            std::cout << "Route " << r.routeID << " has " << r.demand << " student(s) but no bus.\n";
            any = true;
        } else if (r.status == DemandStatus::Underutilized) {
            std::cout << "Route " << r.routeID << " is underutilized (" << r.utilizationPct
                      << "%): a bus could be moved to a busier route.\n";
            any = true;
        }
    }
    if (!any) std::cout << "Every route is balanced.\n";
    if (spare > 0) std::cout << spare << " spare bus(es) are not assigned to any route.\n";
}

// ---------------------------------------------------------------------------
// 9. Users & passwords
// ---------------------------------------------------------------------------
static void doChangePassword(System& sys, const User& user) {
    std::string oldPw = readPassword("Current password: ");
    std::string newPw = readPassword("New password: ");
    if (newPw != readPassword("Repeat new password: ")) { std::cout << "The passwords do not match.\n"; return; }
    if (sys.auth.changePassword(user.getUsername(), oldPw, newPw)) {
        sys.auth.saveToFile(USERS_FILE);
        std::cout << "Password changed.\n";
    } else {
        std::cout << sys.auth.getLastError() << "\n";
    }
}

static void userMenu(System& sys, const User& me) {
    int choice;
    do {
        line();
        std::cout << "USER MANAGEMENT\n"
                  << " 1. List users\n 2. Add a user\n 3. Remove a user\n 4. Reset a user's password\n 0. Back\n";
        choice = readInt("Choice: ");

        if (choice == 1) {
            sys.auth.displayUsers();
        } else if (choice == 2) {
            std::string name = readLine("New username: ");
            std::cout << "Role: 1 = Admin, 2 = Staff, 3 = Student\n";
            int r = readInt("Role: ");
            if (r < 1 || r > 3) { std::cout << "Invalid role.\n"; continue; }
            Role role = (r == 1) ? Role::ADMIN : (r == 2) ? Role::STAFF : Role::STUDENT;
            int linked = -1;
            if (role == Role::STUDENT) {
                linked = readInt("Student ID this account belongs to: ");
                if (!sys.sm.studentIDExists(linked)) { std::cout << "That student does not exist.\n"; continue; }
            }
            std::string pw = readPassword("Password: ");
            if (pw != readPassword("Repeat password: ")) { std::cout << "The passwords do not match.\n"; continue; }
            if (sys.auth.addUser(name, pw, role, linked)) {
                sys.auth.saveToFile(USERS_FILE);
                std::cout << "User created.\n";
            } else {
                std::cout << sys.auth.getLastError() << "\n";
            }
        } else if (choice == 3) {
            std::string name = readLine("Username to remove: ");
            if (name == me.getUsername()) { std::cout << "You cannot remove the account you are logged in with.\n"; continue; }
            if (sys.auth.removeUser(name)) { sys.auth.saveToFile(USERS_FILE); std::cout << "User removed.\n"; }
            else std::cout << sys.auth.getLastError() << "\n";
        } else if (choice == 4) {
            std::string name = readLine("Username: ");
            std::string pw = readPassword("New password: ");
            if (pw != readPassword("Repeat new password: ")) { std::cout << "The passwords do not match.\n"; continue; }
            if (sys.auth.resetPassword(name, pw)) { sys.auth.saveToFile(USERS_FILE); std::cout << "Password reset.\n"; }
            else std::cout << sys.auth.getLastError() << "\n";
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// ---------------------------------------------------------------------------
// Login and sessions
// ---------------------------------------------------------------------------
static bool login(System& sys, User& user) {
    line();
    std::cout << "SMARTROUTE LOGIN (type 'exit' as the username to quit)\n";
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::string name = readLine("Username: ");
        if (name == "exit") return false;
        std::string pw = readPassword("Password: ");
        if (sys.auth.login(name, pw, user)) return true;
        std::cout << "Incorrect username or password.\n";
    }
    std::cout << "Too many failed attempts.\n";
    return false;
}

static void forcePasswordChange(System& sys, const std::string& username) {
    std::cout << "This is the first run: please choose a new password for '" << username << "'.\n";
    while (true) {
        std::string pw = readPassword("New password: ");
        if (pw != readPassword("Repeat new password: ")) { std::cout << "The passwords do not match.\n"; continue; }
        if (sys.auth.resetPassword(username, pw)) break;
        std::cout << sys.auth.getLastError() << "\n";
    }
    sys.auth.saveToFile(USERS_FILE);
    std::cout << "Password updated.\n";
}

// STAFF and ADMIN
static void staffSession(System& sys, const User& user) {
    bool isAdmin = (user.getRole() == Role::ADMIN);
    int choice;
    do {
        line();
        std::cout << "SMARTROUTE - " << user.getUsername() << " (" << User::roleToString(user.getRole()) << ")\n"
                  << " 1. Bus Management\n"
                  << " 2. Student & Department Management\n"
                  << " 3. Bus Stops & Routes\n"
                  << " 4. Capacity & Bus Allocation\n"
                  << " 5. Route Optimization (Dijkstra, BFS, DFS)\n"
                  << " 6. Scheduling & Conflict Detection\n"
                  << " 7. What-If Simulator\n"
                  << " 8. Demand Analysis\n";
        if (isAdmin) std::cout << " 9. User Management\n";
        std::cout << "10. Change my password\n"
                  << "11. Save all data\n"
                  << " 0. Log out\n";
        choice = readInt("Choice: ");

        if (choice == 1) busMenu(sys.bm);
        else if (choice == 2) studentMenu(sys.sm, sys.rm, sys.alloc);
        else if (choice == 3) routeMenu(sys.rm, sys.bm, sys.sm, sys.alloc);
        else if (choice == 4) allocationMenu(sys.alloc);
        else if (choice == 5) optimizationMenu(sys);
        else if (choice == 6) schedulingMenu(sys);
        else if (choice == 7) simulatorMenu(sys);
        else if (choice == 8) demandAnalysis(sys);
        else if (choice == 9 && isAdmin) userMenu(sys, user);
        else if (choice == 10) doChangePassword(sys, user);
        else if (choice == 11) { saveAll(sys); std::cout << "All data saved.\n"; }
        else if (choice != 0) std::cout << "Invalid choice.\n";
    } while (choice != 0);
}

// STUDENT: read-only view of their own record
static void studentSession(System& sys, const User& user) {
    int choice;
    do {
        line();
        std::cout << "STUDENT PORTAL - " << user.getUsername() << "\n"
                  << " 1. My details\n 2. My bus route\n 3. My bus\n 4. Change my password\n 0. Log out\n";
        choice = readInt("Choice: ");

        Student* me = sys.sm.searchStudent(user.getLinkedStudentID());
        if (choice >= 1 && choice <= 3 && me == nullptr) {
            std::cout << "Your student record could not be found. Please contact the transport office.\n";
            continue;
        }

        if (choice == 1) {
            me->display();
            Department* d = sys.sm.findDepartment(me->getDepartmentID());
            if (d) { std::cout << "Department: "; d->display(); }
            if (me->getBusStopID() == -1) std::cout << "You have no bus stop yet.\n";
            else std::cout << "Your stop: " << stopLabel(sys.rm, me->getBusStopID()) << "\n";
        } else if (choice == 2) {
            if (me->getAssignedRouteID() == -1) {
                std::cout << "You have not been given a seat yet. The transport office will allocate one.\n";
            } else {
                sys.rm.displayRoute(me->getAssignedRouteID());
            }
        } else if (choice == 3) {
            int rid = me->getAssignedRouteID();
            if (rid == -1) { std::cout << "You have not been given a seat yet.\n"; continue; }
            bool found = false;
            for (Bus& b : sys.bm.getBuses()) {
                if (b.isAvailable() && sys.alloc.getRouteOfBus(b.getID()) == rid) {
                    b.display();
                    found = true;
                }
            }
            if (!found) std::cout << "No bus is currently running on your route.\n";
        } else if (choice == 4) {
            doChangePassword(sys, user);
        } else if (choice != 0) {
            std::cout << "Invalid choice.\n";
        }
    } while (choice != 0);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    System sys;
    ensureDataFolder();

    try {
        loadAll(sys);
    } catch (const std::exception&) {
        std::cout << "A data file looks corrupted; some saved data could not be loaded.\n";
    }

    sys.auth.loadFromFile(USERS_FILE);
    bool firstRun = sys.auth.isEmpty();
    if (firstRun) {
        sys.auth.createDefaultAdmin();
        sys.auth.saveToFile(USERS_FILE);
        std::cout << "First run: an admin account was created.\n"
                     "  Username: admin   Password: admin123\n";
    }

    try {
        User user;
        while (login(sys, user)) {
            if (firstRun && user.getUsername() == "admin") {
                forcePasswordChange(sys, "admin");
                firstRun = false;
            }
            if (user.getRole() == Role::STUDENT) studentSession(sys, user);
            else staffSession(sys, user);
            saveAll(sys);
            std::cout << "Logged out. Data saved.\n";
        }
    } catch (const InputClosed&) {
        std::cout << "\nInput closed.\n";
    }

    saveAll(sys);
    std::cout << "Data saved. Goodbye!\n";
    return 0;
}
            

            
