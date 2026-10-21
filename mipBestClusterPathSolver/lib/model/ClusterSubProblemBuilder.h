#ifndef CLUSTER_SUB_PROBLEM_BUILDER_H
#define CLUSTER_SUB_PROBLEM_BUILDER_H

#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <model/BestClusterPathInstance.h>
#include <model/ClusterBoundary.h>
#include <model/Solution.h>
#include <map>
#include <memory>
#include <vector>

struct ClusterArcMapping {
    std::map<int, int> local_to_global_arc;
    std::map<int, int> local_to_global_node;
};

struct ClusterProblem {
    int cluster = 0;
    ClusterBoundary boundary;
    Instance instance;
    std::unique_ptr<Graph> graph;
    std::unique_ptr<SuperGraph> super_graph;
    ClusterArcMapping mapping;

    ClusterProblem(int cluster, ClusterBoundary boundary, const BestClusterPathInstance global_instance)
        : cluster(cluster), boundary(boundary) {
            instance.vehicles = 1;
            instance.nodes = global_instance.rcpp.nodes;
            instance.turns = global_instance.rcpp.turns;
            instance.illegal_turns = global_instance.rcpp.illegal_turns;
        }

    void add_mapping(const SuperArc& locla_arc, const SuperArc& global_arc) {
        mapping.local_to_global_arc[locla_arc.id] = global_arc.id;
        mapping.local_to_global_node[locla_arc.from] = global_arc.from;
        mapping.local_to_global_node[locla_arc.to] = global_arc.to;
    }

    int local_to_global_node(int local) {
        return mapping.local_to_global_node.at(local);
    }
    
};

class ClusterSubProblemBuilder {
public:
    ClusterSubProblemBuilder(
        const BestClusterPathInstance& instance,
        const SuperGraph& global_super_graph,
        const Solution& global_solution,
        const std::map<int, ClusterBoundary>& boundaries)
        : instance(instance),
          global_super_graph(global_super_graph),
          global_solution(global_solution),
          boundaries(boundaries) {}

    std::vector<ClusterProblem> get_subproblems();

private:
    const BestClusterPathInstance& instance;
    const SuperGraph& global_super_graph;
    const Solution& global_solution;
    const std::map<int, ClusterBoundary>& boundaries;

    vector<int> edge_serviced;

    vector<ClusterProblem> problems;
    map<int, size_t> problem_index;

    int global_edge_id = 0;
    map<int, vector<int>> global_edge_by_cluster;

    void service_assignments(vector<int>& assigned);
    void assign_edges(const vector<InstanceEdge>& edges, bool bidirectional);
    
};

#endif
