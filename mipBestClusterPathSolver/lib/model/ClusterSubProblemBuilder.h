#ifndef CLUSTER_SUB_PROBLEM_BUILDER_H
#define CLUSTER_SUB_PROBLEM_BUILDER_H

#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <graph/NeutralExpandedGraph.h>
#include <model/ClusterBoundary.h>
#include <model/Solution.h>
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <vector>

struct ClusterArcMapping {
    map<int, int> local_to_global_arc;
    map<int, int> local_to_global_node;
};

struct ClusterProblem {
    int cluster = 0;
    ClusterBoundary boundary;
    Instance instance;
    unique_ptr<Graph> graph;
    unique_ptr<SuperGraph> super_graph;
    ClusterArcMapping mapping;

    ClusterProblem(int cluster, ClusterBoundary boundary, const NeutralExpandedGraph& expanded_graph)
        : cluster(cluster), boundary(boundary) {
            instance.vehicles = max(
                1, static_cast<int>(this->boundary.optimized_visit_count()));
            instance.nodes = expanded_graph.nodes_amount();
            instance.turns = expanded_graph.get_turns();
            instance.illegal_turns = expanded_graph.get_illegal_turns();
        }

    void add_mapping(const SuperArc& local_arc, const SuperArc& global_arc) {
        mapping.local_to_global_arc[local_arc.id] = global_arc.id;
        mapping.local_to_global_node[local_arc.from] = global_arc.from;
        mapping.local_to_global_node[local_arc.to] = global_arc.to;
    }

    int local_to_global_node(int local) {
        return mapping.local_to_global_node.at(local);
    }
    
};

class ClusterSubProblemBuilder {
public:
    ClusterSubProblemBuilder(
        const NeutralExpandedGraph& instance,
        const SuperGraph& global_super_graph,
        const Solution& global_solution,
        const map<int, ClusterBoundary>& boundaries)
        : _expanded_graph(instance),
          _global_super_graph(global_super_graph),
          _global_solution(global_solution),
          _boundaries(boundaries) {}

    vector<ClusterProblem> get_subproblems();

private:
    const NeutralExpandedGraph& _expanded_graph;
    const SuperGraph& _global_super_graph;
    const Solution& _global_solution;
    const map<int, ClusterBoundary>& _boundaries;

    set<int> serviced_edges;

    vector<ClusterProblem> problems;
    map<int, size_t> problem_index;

    map<int, vector<int>> global_edge_by_cluster;

    // cluster → arista global → vehículo lógico local
    map<int, map<int, int>> local_vehicle_by_global_edge;

    void service_assignments();
    void assign_edges(const vector<int>& global_edge_ids, bool bidirectional);
    
};

#endif
