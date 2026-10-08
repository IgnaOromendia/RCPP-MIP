#ifndef CLUSTER_PATH_SORT_INSTANCE_H
#define CLUSTER_PATH_SORT_INSTANCE_H

#include <model/PathSortInstance.h>
#include <map>
#include <vector>

struct ClusterPathSortInstance {
    PathSortInstance path;
    // edge_clusters[e] is the zero-based cluster identifier of path.edges[e].
    std::vector<int> edge_clusters;
    // edges_by_cluster[c] contains the indices of path.edges assigned to c.
    std::map<int, std::vector<int>> edges_by_cluster;
    // Number of clusters; valid identifiers are in [0, cluster_count).
    int cluster_count = 0;
};

#endif
