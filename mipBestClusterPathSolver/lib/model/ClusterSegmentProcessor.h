#ifndef CLUSTER_SEGMENT_PROCESSOR_H
#define CLUSTER_SEGMENT_PROCESSOR_H

#include <model/BestClusterPathInstance.h>
#include <model/ClusterBoundary.h>
#include <model/SolutionEulerCircuit.h>
#include <graph/SuperGraph.h>
#include <map>
#include <set>

class ClusterSegmentProcessor {
public:
    ClusterSegmentProcessor(
        const SuperGraph& super_graph,
        const BestClusterPathInstance& instance,
        const map<int, NeutralInOut>& neutral_in_out_by_arc,
        const map<int, set<int>>& clusters_by_node);

    map<int, vector<ClusterVisit>> process(const vector<OrderedSolutionArc>& circuit);

private:
    struct ActiveClusterVisit {
        int cluster;
        ClusterPortal entry;
        vector<int> internal_arc_ids;
        vector<int> service_edge_ids;
        bool has_physical_arc = false;

        ClusterVisit finish(int id, ClusterPortal exit) const {
            return {id, entry, exit, service_edge_ids, !has_physical_arc, has_physical_arc ? vector<int>() : internal_arc_ids};
        }
    };

    const SuperGraph& super_graph;
    const BestClusterPathInstance& instance;
    const map<int, NeutralInOut>& neutral_in_out_by_arc;
    const map<int, set<int>>& clusters_by_node;

    ActiveClusterVisit active_visit{};
    bool visiting_cluster = false;
    int next_visit_id = 1;
    map<int, vector<ClusterVisit>> visits_by_cluster;

    pair<int, int> resolve_adj_deposit_node(int virtual_node) const;
    void process_segment(const OrderedSolutionArc& segment);
    void process_deposit_segment(const OrderedSolutionArc& segment);
    void process_internal_segment(const OrderedSolutionArc& segment);
    void process_cluster_crossing(const OrderedSolutionArc& segment, const NeutralInOut& crossing);
    void open_visit(int cluster, const ClusterPortal& entry);
    void close_visit(int expected_cluster, const ClusterPortal& exit);
};

#endif
