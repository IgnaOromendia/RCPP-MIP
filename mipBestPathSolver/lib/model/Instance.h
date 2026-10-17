#ifndef INSTANCE_H
#define INSTANCE_H

#include "../graph/Turn.h"
#include <vector>

// Original nodes are zero-based. Synthetic deposit connections belong to Graph.
struct InstanceEdge {
    int from, to, zone;
    double cost, demand;
};

struct Instance {
    int vehicles = 0;
    int nodes = 0;
    std::vector<int> deposit_nodes;
    std::vector<InstanceEdge> edges, arcs;
    std::vector<Turn> turns, illegal_turns;

    // Also validate instances assembled directly in memory before building Graph.
    void validate() const;
};

#endif
