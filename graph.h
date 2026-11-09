#ifndef GRAPH_H
#define GRAPH_H

#include <unordered_map>
#include <vector>
#include "busstop.h"
#include "route.h"

struct Edge {
    int to;
    double weight;
};

struct EdgeWeight {
    int a;
    int b;
    double weight;
};

struct PathResult {
    bool found = false;
    double distance = 0.0;     
    std::vector<int> path;      
};

class Graph {
private:
    std::unordered_map<int, std::vector<Edge>> adj;
    void upsert(int from, int to, double weight);   

public:
    void addNode(int id);
    bool hasNode(int id) const;
    bool addEdge(int a, int b, double weight);      
    bool hasEdge(int a, int b) const;
    double getWeight(int a, int b) const;          
    const std::vector<Edge>& neighbors(int id) const;
    std::vector<int> nodes() const;                
    int nodeCount() const;
    int edgeCount() const;
    Graph withoutNode(int id) const;               
};

Graph buildGraph(const std::vector<BusStop>& stops,
                 const std::vector<Route>& routes,
                 const std::vector<EdgeWeight>& weights = std::vector<EdgeWeight>(),
                 double defaultWeight = 1.0);


std::unordered_map<int, double> shortestDistances(const Graph& g, int startId);

// BFS / DFS traversal order (neighbours visited in ascending stop ID).
std::vector<int> bfsTraverse(const Graph& g, int startId);
std::vector<int> dfsTraverse(const Graph& g, int startId);

// Dijkstra with path reconstruction.
PathResult findMostEfficientRoute(const Graph& g, int startId, int endId);

bool isReachable(const Graph& g, int fromId, int toId);

#endif
