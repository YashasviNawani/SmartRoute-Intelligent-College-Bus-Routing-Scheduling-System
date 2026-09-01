#include <iostream>
#include <fstream>
#include <cstdio>
#include "busManager.h"
using namespace std;
void addBus()
{
    Bus b, temp;
    int id;
    cout << "\nEnter Bus ID: ";
    cin >> id;
    ifstream check("buses.dat", ios::binary);
    while (check.read((char*)&temp, sizeof(temp)))
    {
        if (temp.id == id)
        {
            cout << "Bus ID already exists!\n";
            check.close();
            return;
        }
    }
    check.close();
    b.id = id;
    cout << "Enter Bus Number: ";
    cin >> b.busNo;
    cout << "Enter Driver Name: ";
    cin >> b.driverName;
    cout << "Enter Capacity: ";
    cin >> b.cap;
    if (b.cap <= 0)
    {
        cout << "Invalid capacity!\n";
        return;
    }
    b.currS = 0;
    b.available = 1;
    ofstream file("buses.dat", ios::binary | ios::app);
    file.write((char*)&b, sizeof(b));
    file.close();
    cout << "Bus added successfully!\n";
}
void displayAllBuses()
{
    Bus b;
    ifstream file("buses.dat", ios::binary);
    if (!file)
    {
        cout << "\nNo bus data found.\n";
        return;
    }
    bool found = false;
    cout << "\n========== ALL BUSES ==========\n";
    while (file.read((char*)&b, sizeof(b)))
    {
        found = true;
        cout << "\nBus ID: " << b.id;
        cout << "\nBus No: " << b.busNo;
        cout << "\nDriver Name: " << b.driverName;
        cout << "\nCapacity: " << b.cap;
        cout << "\nCurrent Students: "<< b.currS;
        cout << "\nRemaining Capacity: "<< b.cap - b.currS;
        cout << "\nAvailability: ";
             if(b.available)
             cout<<"Available";
             else
             cout<<"Not Available";
        cout << "\n-----------------------------\n";
    }
    file.close();
    if (!found)
        cout << "No buses found.\n";
}

void removeBus()
{
    int id;
    Bus b;
    cout << "\nEnter Bus ID to remove: ";
    cin >> id;
    ifstream file("buses.dat", ios::binary);
    ofstream temp("temp.dat", ios::binary);
    bool found = false;
    while (file.read((char*)&b, sizeof(b)))
    {
        if (b.id == id)
        {
            found = true;
            continue;
        }
        temp.write((char*)&b, sizeof(b));
    }
    file.close();
    temp.close();
    remove("buses.dat");
    rename("temp.dat", "buses.dat");
    if (found)
        cout << "Bus removed successfully!\n";
    else
        cout << "Bus not found!\n";
}
void updateBus()
{
    int id;
    Bus b;
    cout << "\nEnter Bus ID to update: ";
    cin >> id;
    ifstream file("buses.dat", ios::binary);
    ofstream temp("temp.dat", ios::binary);
    bool found = false;
    while (file.read((char*)&b, sizeof(b)))
    {
        if (b.id == id)
        {
            found = true;
            cout << "Enter new Bus Number: ";
            cin >> b.busNo;
            cout << "Enter new Driver Name: ";
            cin >> b.driverName;
            cout << "Enter new Capacity: ";
            cin >> b.cap;
            if (b.cap < b.currS)
            {
                cout << "Capacity cannot be less than "<< b.currS << ".\n";
                file.close();
                temp.close();
                remove("temp.dat");
                return;
            }
        }
        temp.write((char*)&b, sizeof(b));
    }
    file.close();
    temp.close();
    remove("buses.dat");
    rename("temp.dat", "buses.dat");
    if (found)
        cout << "Bus updated successfully!\n";
    else
        cout << "Bus not found!\n";
}