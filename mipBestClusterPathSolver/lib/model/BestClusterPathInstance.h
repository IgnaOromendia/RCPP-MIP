#ifndef BEST_CLUSTER_PATH_INSTANCE_H
#define BEST_CLUSTER_PATH_INSTANCE_H

#include <graph/Edges.h>
#include <model/Instance.h>
#include <map>
#include <vector>

struct BestClusterPathInstance {
    Instance rcpp;
    std::map<int, std::vector<EdgeKey>> in_out_edges_by_cluster;
};

#endif
