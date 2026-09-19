#ifndef EDGES_H
#define EDGES_H

#include<iostream>

using namespace std;

struct SuperArc {
    int id, from, to, zone, pair, edge_id, requested_idx;
    double cost, demand;
    bool requested;
    
    SuperArc(int id, int edge_id, int requested_idx, int from, int to, int zone, float cost, float demand, int pair) {
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
    int id, from, to, zone, requested_idx;
    float cost, demand;
    bool requested, is_bidirectional;

    Edge(int id, int from, int to, int zone, float cost, float demand, int requested_idx, bool is_bidirectional = false) {
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

struct Node {
    int id;
    double cost;
    Node(int id, double cost): id(id),  cost(cost) {}
};

#endif