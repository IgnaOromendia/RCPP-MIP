#ifndef EDGES_H
#define EDGES_H

#include <iostream>
#include <unordered_set>

using namespace std;

struct SuperArc {
    // id and pair index the supergraph (pair == -1 when unpaired).
    // edge_id is the original global Edge::id, or -1 for turns / -2 for deposit connectors.
    // requested_idx indexes required original edges, or is -1 for non-required arcs.
    int id, from, to, zone, pair, edge_id, requested_idx;
    double cost, demand;
    bool requested;
    
    SuperArc(int id, int edge_id, int requested_idx, int from, int to, int zone, double cost, double demand, int pair) {
        this->id            = id;
        this->edge_id       = edge_id;
        this->from          = from;
        this->to            = to;
        this->zone          = zone;
        this->cost          = cost;
        this->demand        = demand;
        this->requested     = zone != 0;
        this->pair          = pair;
        this->requested_idx = requested_idx;
    }
};

struct Edge {
    // id indexes Graph::_all_edges, independently of the local _edges/_arcs position.
    // requested_idx is consecutive among required edges only; otherwise -1.
    int id, from, to, zone, requested_idx;
    double cost, demand;
    bool requested, is_bidirectional;

    Edge(int id, int from, int to, int zone, double cost, double demand, int requested_idx, bool is_bidirectional = false) {
        this->id                = id;
        this->from              = from;
        this->to                = to;
        this->zone              = zone;
        this->cost              = cost;
        this->demand            = demand;
        this->requested         = zone != 0;
        this->is_bidirectional  = is_bidirectional;
        this->requested_idx     = requested ? requested_idx : -1;
    }

    SuperArc to_super_arc(int id, int from, int to, int pair = -1) const {
        return SuperArc(id, this->id, this->requested_idx, from, to, zone, cost, demand, pair);
    }
};

typedef pair<int, int> EdgeKey;

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& key) const noexcept {
        const std::size_t h1 = std::hash<int>{}(key.first);
        const std::size_t h2 = std::hash<int>{}(key.second);
        return h1 ^ (h2 + 0x9e3779b9U + (h1 << 6) + (h1 >> 2));
    }
};

typedef std::unordered_set<EdgeKey, EdgeKeyHash> edgeKeySet;

struct Node {
    int id;
    double cost;
    Node(int id, double cost): id(id),  cost(cost) {}
};

#endif
