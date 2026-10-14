#ifndef RCPP_CLUSTER_CONSTRAINTS_H
#define RCPP_CLUSTER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <vector>
#include "../graph/SuperGraph.h"

class RCPPClusterConstraintSetter: public ConstraintSetter {
public:
    RCPPClusterConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_cluster_crossing_constraint(
        const VariableMatrix& X,
        const std::vector<bool>& frontier_edge);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
