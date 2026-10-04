#ifndef PATH_SORT_INSTANCE_H
#define PATH_SORT_INSTANCE_H

#include <utility>
#include <vector>

typedef std::vector<std::vector<std::pair<int, int>>> graph;

struct PathEdge {
    // from/to are zero-based virtual nodes; -1 denotes the deposit.
    // original_edge_id indexes Instance::edges followed by Instance::arcs.
    // Turn and deposit connectors use -1 and -2 respectively.
    int super_arc_id = -1;
    int original_edge_id = -1;
    int from = -1;
    int to = -1;
    int vehicle = 0;
    long long service_count = 0;
    long long deadhead_count = 0;

    long long times() const noexcept { return service_count + deadhead_count; }
};

struct PathSortInstance {
    int vehicles = 0;
    std::vector<PathEdge> edges;
    // adj[v] contains (u, super_arc_id) for every selected arc v -> u.
    // The synthetic deposit uses its non-negative SuperGraph node id here.
    graph adj;
};

#endif
