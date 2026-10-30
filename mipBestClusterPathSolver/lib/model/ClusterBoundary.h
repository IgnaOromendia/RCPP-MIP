#ifndef CLUSTER_BOUNDARY_H
#define CLUSTER_BOUNDARY_H

#include <graph/NeutralExpandedGraph.h>
#include <model/Solution.h>
#include <graph/SuperGraph.h>
#include <algorithm>
#include <map>
#include <vector>

enum class ClusterPortalKind { NeutralInOut, GlobalDeposit };

struct ClusterPortal {
    ClusterPortalKind kind;
    int global_arc_id;
    int global_node;
    long long count;
    bool entry;

};

struct ClusterVisit {
    int id;
    ClusterPortal entry;
    ClusterPortal exit;
    vector<int> service_edge_ids;
    bool passthrough = false;
    // Una visita formada solo por conectores de giro no tiene un recorrido
    // interno que optimizar. Esos arcos globales se conservan al fusionar.
    vector<int> passthrough_arc_ids;

    bool is_passthrough() const noexcept {
        return passthrough;
    }
};

struct BoundaryGate {
    int global_node;
    long long entries = 0;
    long long exits = 0;

    BoundaryGate(int node, long long entry_count, long long exit_count)
        : global_node(node), entries(entry_count), exits(exit_count) {}

    explicit BoundaryGate(const ClusterPortal& portal): global_node(portal.global_node) {
        if (portal.entry) 
            entries = portal.count;
        else 
            exits = portal.count;
    }
};

class ClusterBoundary {
public:
    int cluster = 0;
    vector<BoundaryGate> gates;
    vector<ClusterPortal> portals;
    vector<ClusterVisit> visits;

    size_t optimized_visit_count() const {
        return static_cast<size_t>(std::count_if(
            visits.begin(), visits.end(),
            [](const ClusterVisit& visit) {
                return !visit.is_passthrough();
            }));
    }
};

class ClusterBoundaryExtractor {
public:
    ClusterBoundaryExtractor(
        const SuperGraph& super_graph,
        const NeutralExpandedGraph& instance,
        const Solution& solution);

    map<int, ClusterBoundary> get_boundaries();
    const map<EdgeKey, int>& get_global_arc_by_edge() const;

private:
    const SuperGraph& super_graph;
    const NeutralExpandedGraph& _expanded_graph;
    const Solution& solution;
    map<int, ClusterBoundary> boundaries;

    map<EdgeKey, int> global_arc_by_edge;
    map<int, int> cluster_by_node;

    void add_portal(int cluster, const ClusterPortal& portal);
    void add_visit(int cluster, ClusterVisit visit);
    void validate_balances() const;
};

#endif
