#ifndef BEST_CLUSTER_PATH_INSTANCE_H
#define BEST_CLUSTER_PATH_INSTANCE_H

#include <graph/Edges.h>
#include <model/Instance.h>
#include <map>
#include <string>
#include <vector>

struct NeutralInOut {
    int source_cluster;
    int target_cluster;
    EdgeKey edge;
};

struct BestClusterPathInstance {
    BestClusterPathInstance() = default;
    BestClusterPathInstance(
        const std::string& graph_path,
        const std::string& turns_path,
        const std::string& in_out_path,
        const std::string& arc_clusters_path);

    Instance rcpp;
    std::map<int, std::vector<EdgeKey>> in_out_edges_by_cluster;
    std::vector<NeutralInOut> neutral_in_outs;
    // One entry per graph arc source (undirected edges first, then directed arcs).
    // Zero identifies a synthetic neutral_in_out between clusters.
    std::vector<int> arc_clusters;
};

#endif
