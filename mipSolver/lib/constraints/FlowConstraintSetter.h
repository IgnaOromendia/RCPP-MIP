#ifndef FLOW_CONSTRAINTS_H
#define FLOW_CONSTRAINTS_H

#include <ConstraintSetter.hpp>

#include "../graph/SuperGraph.h"

class FlowConstraintSetter: public ConstraintSetter {
public:
    FlowConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_deposit_flow_constraint(const VariableArray& X, const VariableArray& FDK);
    void set_flow_conservation_constraint(const VariableArray& X, const VariableArray& F, const VariableArray& FDK);
    void set_flow_bounds_constraint(const VariableArray& X, const VariableArray& Y, const VariableArray& F, const VariableArray& FDK, const VariableArray& YDK, double capacity);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
