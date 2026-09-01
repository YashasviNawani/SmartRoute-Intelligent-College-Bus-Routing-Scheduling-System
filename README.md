# SmartRoute-Intelligent-College-Bus-Routing-Scheduling-System SmartRoute
Intelligent College Bus Routing & Scheduling System
SmartRoute is a C++ console application that automates the allocation, scheduling, and optimization of a college's bus transportation network — matching student demand to bus capacity across routes and stops, detecting scheduling conflicts, and modeling hypothetical disruptions through a built-in What-If Simulator.
 Overview
Colleges with multiple buses, routes, and hundreds of students face a recurring logistics problem: which bus should cover which route, what happens when a route's demand exceeds a single bus's capacity, and how does the schedule adapt when something changes — a bus breaks down, a stop closes, or 30 extra students show up overnight?

SmartRoute solves this by modeling the problem as a set of classical data-structure and algorithm challenges — graphs, greedy allocation, priority queues, and interval scheduling — wrapped in a clean, modular C++ system.

 Key Features
•Bus, Route, Stop, and Student Management — full CRUD operations with fast hashmap-based lookups

•Automatic Capacity Allocation — splits route demand across multiple buses when one bus isn't enough (e.g. 72 students → Bus 1 takes 50, Bus 3 takes the remaining 22)

•Route Optimization — finds the shortest/most efficient path between stops using Dijkstra's algorithm

•Conflict Detection — flags double-booked buses and overlapping trip timings

•What-If Simulator — recalculates the full schedule under hypothetical scenarios:
•"What if Bus 3 is unavailable tomorrow?"
•"What if 30 additional students are added to Route B?"
•"What if a stop becomes unreachable?"

•Demand Analysis — flags underutilized and overburdened routes
Persistent Storage — saves and loads all system data between sessions
System Architecture

SmartRoute follows a layered design so that features stay independently testable and the What-If Simulator can reuse the same scheduling engine as the live system, on a copied dataset
