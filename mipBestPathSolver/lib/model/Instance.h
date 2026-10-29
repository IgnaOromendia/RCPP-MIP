#ifndef INSTANCE_H
#define INSTANCE_H

#include "../graph/Turn.h"
#include <string>
#include <vector>

// Original nodes are zero-based. Synthetic deposit connections belong to Graph.
struct InstanceEdge {
    int from, to, zone;
    double cost, demand;
};

struct Instance {
    Instance() = default;
    Instance(const std::string& graph_path, const std::string& turns_path,
             const std::string& clusters_path, const std::string& neutral_path = "");

    int vehicles = 0;
    int nodes = 0;
    std::vector<int> deposit_nodes;
    std::vector<InstanceEdge> edges, arcs;
    std::vector<Turn> turns, illegal_turns;

    std::vector<int> record_clusters; // Original records: edges, then arcs.
    std::vector<int> neutral_nodes;

    // Also validate instances assembled directly in memory before building Graph.
    void validate() const;
};

#endif
