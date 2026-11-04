#ifndef THREE_CLUSTER_INSTANCE_H
#define THREE_CLUSTER_INSTANCE_H

#include <model/Instance.h>

// Three directed triangles share node 0; the deposit is inside cluster 1.
// Every physical record is required, has unit cost and must be served once.
inline Instance three_cluster_cycles() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 7;
    input.deposit_nodes = {1};
    for (int cluster = 1; cluster <= 3; ++cluster) {
        const int first = 2 * cluster - 1;
        const int second = first + 1;
        input.arcs.push_back({0, first, 1, 1, 1});
        input.arcs.push_back({first, second, 1, 1, 1});
        input.arcs.push_back({second, 0, 1, 1, 1});
        input.record_clusters.insert(input.record_clusters.end(), 3, cluster);
    }
    input.neutral_nodes = {0};
    return input;
}

#endif
