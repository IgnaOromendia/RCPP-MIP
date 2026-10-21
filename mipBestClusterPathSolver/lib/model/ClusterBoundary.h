#ifndef CLUSTER_BOUNDARY_H
#define CLUSTER_BOUNDARY_H

#include <model/BestClusterPathInstance.h>
#include <model/Solution.h>
#include <graph/SuperGraph.h>
#include <map>
#include <set>

enum class ClusterPortalKind { Transition, GlobalDeposit };

struct ClusterPortal {
    ClusterPortalKind kind;
    int global_arc_id;
    int global_node;
    long long count;
    bool entry;

};

struct BoundaryFlow {
    int global_node;
    long long entries = 0;
    long long exits = 0;

    BoundaryFlow(int node, long long entry_count, long long exit_count)
        : global_node(node), entries(entry_count), exits(exit_count) {}

    explicit BoundaryFlow(const ClusterPortal& portal): global_node(portal.global_node) {
        if (portal.entry) 
            entries = portal.count;
        else 
            exits = portal.count;
    }
};

class ClusterBoundary {
public:
    int cluster = 0;
    vector<BoundaryFlow> flows;
    vector<ClusterPortal> portals;
};

class ClusterBoundaryExtractor {
public:
    ClusterBoundaryExtractor(
        const SuperGraph& super_graph,
        const BestClusterPathInstance& instance,
        const Solution& solution);

    map<int, ClusterBoundary> get_boundaries();

private:
    const SuperGraph& super_graph;
    const BestClusterPathInstance& instance;
    const Solution& solution;
    map<int, ClusterBoundary> boundaries;

    map<EdgeKey, int> arc_id_by_edge;
    map<EdgeKey, ClusterTransition> transition_by_edge;
    set<int> transition_records;
    map<int, set<int>> clusters_by_node;

    void add_portal(int cluster, ClusterPortal portal);
    void validate_balances() const;
};

#endif
