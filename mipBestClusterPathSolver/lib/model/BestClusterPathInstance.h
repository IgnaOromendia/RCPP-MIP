#ifndef BEST_CLUSTER_PATH_INSTANCE_H
#define BEST_CLUSTER_PATH_INSTANCE_H

#include <graph/Edges.h>
#include <model/Instance.h>
#include <map>
#include <vector>

struct ClusterTransition {
    int source_cluster;
    int target_cluster;
    EdgeKey edge;
};

struct BestClusterPathInstance {
    Instance rcpp;
    std::map<int, std::vector<EdgeKey>> in_out_edges_by_cluster;
    std::vector<ClusterTransition> transitions;
    // One entry per graph record (undirected edges first, then directed arcs).
    // Zero identifies a synthetic transition between clusters.
    std::vector<int> record_clusters;
};

#endif
