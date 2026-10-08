#ifndef CLUSTER_PATH_SORT_INSTANCE_H
#define CLUSTER_PATH_SORT_INSTANCE_H

#include <model/PathSortInstance.h>
#include <vector>

struct ClusterPathSortInstance {
    PathSortInstance path;
    // edge_clusters[e] is the positive cluster identifier of path.edges[e].
    std::vector<int> edge_clusters;
};

#endif
