// ---------------------------------------------------------------------------
// College Bus Management System - Phase 1 main program
// Menu-driven console application that connects all modules:
//   Bus Management | Student & Department | Bus Stops & Routes | Allocation
// ---------------------------------------------------------------------------
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include "busmanager.h"
#include "studentmanager.h"
#include "routemanager.h"
#include "allocator.h"

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
const std::string STOPS_FILE  = "data/stops.dat";
const std::string ROUTES_FILE = "data/routes.dat";

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

// Text must not be empty and must not contain '|' (it is the file separator)
static std::string readText(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        if (!s.empty() && s.find('|') == std::string::npos) return s;
        std::cout << "Text cannot be empty or contain the '|' character.\n";
    }
}

static void line() { std::cout << "----------------------------------------\n"; }

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

static void loadAll(BusManager& bm, StudentManager& sm, RouteManager& rm, Allocator& alloc) {
    // Order matters: routes need stops, allocations need buses and routes
    bm.loadFromFile();
    rm.loadStopsFromFile(STOPS_FILE);
    rm.loadRoutesFromFile(ROUTES_FILE);
    sm.loadDepartmentsFromFile();
    sm.loadFromFile();
    alloc.loadFromFile();
}

static void saveAll(BusManager& bm, StudentManager& sm, RouteManager& rm, Allocator& alloc) {
    bm.saveToFile();
    rm.saveStopsToFile(STOPS_FILE);
    rm.saveRoutesToFile(ROUTES_FILE);
    sm.saveDepartmentsToFile();
    sm.saveToFile();
    alloc.saveToFile();
}

// ---------------------------------------------------------------------------
// 1. Bus Management menu
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

// ---------------------------------------------------------------------------
// 2. Student & Department menu
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 3. Bus Stop & Route menu
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// 4. Capacity & Bus Allocation menu
// ---------------------------------------------------------------------------
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
// Main
// ---------------------------------------------------------------------------
int main() {
    BusManager bm;
    StudentManager sm;
    RouteManager rm;
    Allocator alloc(bm, sm, rm);

    ensureDataFolder();
    try {
        loadAll(bm, sm, rm, alloc);
    } catch (const std::exception&) {
        std::cout << "A data file looks corrupted; some saved data could not be loaded.\n";
    }

    try {
        int choice;
        do {
            line();
            std::cout << "COLLEGE BUS MANAGEMENT SYSTEM\n"
                      << " 1. Bus Management\n 2. Student & Department Management\n"
                      << " 3. Bus Stops & Routes\n 4. Capacity & Bus Allocation\n"
                      << " 5. Save all data\n 0. Save and exit\n";
            choice = readInt("Choice: ");

            if (choice == 1) busMenu(bm);
            else if (choice == 2) studentMenu(sm, rm, alloc);
            else if (choice == 3) routeMenu(rm, bm, sm, alloc);
            else if (choice == 4) allocationMenu(alloc);
            else if (choice == 5) { saveAll(bm, sm, rm, alloc); std::cout << "All data saved.\n"; }
            else if (choice != 0) std::cout << "Invalid choice.\n";
        } while (choice != 0);
    } catch (const InputClosed&) {
        std::cout << "\nInput closed.\n";
    }

    saveAll(bm, sm, rm, alloc);
    std::cout << "Data saved. Goodbye!\n";
    return 0;
}
