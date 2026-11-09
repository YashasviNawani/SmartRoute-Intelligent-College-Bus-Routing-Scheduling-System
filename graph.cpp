#include "graph.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_set>
#include <utility>

namespace {
const std::vector<Edge> kNoEdges;

long long pairKey(int a, int b) {
    int lo = std::min(a, b), hi = std::max(a, b);
    return (static_cast<long long>(lo) << 32) ^ static_cast<unsigned int>(hi);
}

void dfsVisit(const Graph& g, int u, std::unordered_set<int>& seen, std::vector<int>& order) {
    seen.insert(u);
    order.push_back(u);
    for (const Edge& e : g.neighbors(u))
        if (!seen.count(e.to)) dfsVisit(g, e.to, seen, order);
}
}  // namespace

// ---------- Graph ----------

void Graph::upsert(int from, int to, double weight) {
    std::vector<Edge>& list = adj[from];
    for (Edge& e : list) {
        if (e.to == to) { e.weight = weight; return; }
    }
    list.push_back(Edge{to, weight});
    std::sort(list.begin(), list.end(), [](const Edge& x, const Edge& y) { return x.to < y.to; });
}

void Graph::addNode(int id) { adj[id]; }

bool Graph::hasNode(int id) const { return adj.count(id) > 0; }

bool Graph::addEdge(int a, int b, double weight) {
    if (weight < 0 || a == b) return false;
    upsert(a, b, weight);
    upsert(b, a, weight);
    return true;
}

bool Graph::hasEdge(int a, int b) const { return getWeight(a, b) >= 0; }

double Graph::getWeight(int a, int b) const {
    auto it = adj.find(a);
    if (it == adj.end()) return -1.0;
    for (const Edge& e : it->second)
        if (e.to == b) return e.weight;
    return -1.0;
}

const std::vector<Edge>& Graph::neighbors(int id) const {
    auto it = adj.find(id);
    return it == adj.end() ? kNoEdges : it->second;
}

std::vector<int> Graph::nodes() const {
    std::vector<int> ids;
    ids.reserve(adj.size());
    for (const auto& kv : adj) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

int Graph::nodeCount() const { return static_cast<int>(adj.size()); }

int Graph::edgeCount() const {
    int total = 0;
    for (const auto& kv : adj) total += static_cast<int>(kv.second.size());
    return total / 2;
}

Graph Graph::withoutNode(int id) const {
    Graph g;
    for (const auto& kv : adj) {
        if (kv.first == id) continue;
        g.addNode(kv.first);
        for (const Edge& e : kv.second)
            if (e.to != id) g.addEdge(kv.first, e.to, e.weight);
    }
    return g;
}

// ---------- building ----------

Graph buildGraph(const std::vector<BusStop>& stops,
                 const std::vector<Route>& routes,
                 const std::vector<EdgeWeight>& weights,
                 double defaultWeight) {
    Graph g;
    for (const BusStop& s : stops) g.addNode(s.getID());

    std::unordered_map<long long, double> overrides;
    for (const EdgeWeight& w : weights) overrides[pairKey(w.a, w.b)] = w.weight;

    for (const Route& r : routes) {
        const std::vector<int>& st = r.getStops();
        for (size_t i = 0; i + 1 < st.size(); ++i) {
            auto it = overrides.find(pairKey(st[i], st[i + 1]));
            g.addEdge(st[i], st[i + 1], it != overrides.end() ? it->second : defaultWeight);
        }
        if (st.size() == 1) g.addNode(st[0]);
    }
    return g;
}

// ---------- Dijkstra ----------

std::unordered_map<int, double> shortestDistances(const Graph& g, int startId) {
    std::unordered_map<int, double> dist;
    if (!g.hasNode(startId)) return dist;

    typedef std::pair<double, int> Item;   // (distance, stop)
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    dist[startId] = 0.0;
    pq.push(Item(0.0, startId));

    while (!pq.empty()) {
        Item top = pq.top();
        pq.pop();
        auto cur = dist.find(top.second);
        if (cur != dist.end() && top.first > cur->second) continue;   // stale entry
        for (const Edge& e : g.neighbors(top.second)) {
            double nd = top.first + e.weight;
            auto f = dist.find(e.to);
            if (f == dist.end() || nd < f->second) {
                dist[e.to] = nd;
                pq.push(Item(nd, e.to));
            }
        }
    }
    return dist;
}

PathResult findMostEfficientRoute(const Graph& g, int startId, int endId) {
    PathResult result;
    if (!g.hasNode(startId) || !g.hasNode(endId)) return result;

    typedef std::pair<double, int> Item;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    std::unordered_map<int, double> dist;
    std::unordered_map<int, int> parent;
    dist[startId] = 0.0;
    pq.push(Item(0.0, startId));

    while (!pq.empty()) {
        Item top = pq.top();
        pq.pop();
        if (top.second == endId) break;
        auto cur = dist.find(top.second);
        if (cur != dist.end() && top.first > cur->second) continue;
        for (const Edge& e : g.neighbors(top.second)) {
            double nd = top.first + e.weight;
            auto f = dist.find(e.to);
            if (f == dist.end() || nd < f->second) {
                dist[e.to] = nd;
                parent[e.to] = top.second;
                pq.push(Item(nd, e.to));
            }
        }
    }

    if (!dist.count(endId)) return result;   // unreachable

    for (int at = endId;;) {
        result.path.push_back(at);
        if (at == startId) break;
        at = parent[at];
    }
    std::reverse(result.path.begin(), result.path.end());
    result.found = true;
    result.distance = dist[endId];
    return result;
}

bool isReachable(const Graph& g, int fromId, int toId) {
    return findMostEfficientRoute(g, fromId, toId).found;
}

// ---------- BFS / DFS ----------

std::vector<int> bfsTraverse(const Graph& g, int startId) {
    std::vector<int> order;
    if (!g.hasNode(startId)) return order;
    std::unordered_set<int> seen;
    std::queue<int> q;
    q.push(startId);
    seen.insert(startId);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        order.push_back(u);
        for (const Edge& e : g.neighbors(u)) {
            if (seen.insert(e.to).second) q.push(e.to);
        }
    }
    return order;
}

std::vector<int> dfsTraverse(const Graph& g, int startId) {
    std::vector<int> order;
    if (!g.hasNode(startId)) return order;
    std::unordered_set<int> seen;
    dfsVisit(g, startId, seen, order);
    return order;
}
