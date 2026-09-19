#ifndef FLOW_CONSTRAINTS_H
#define FLOW_CONSTRAINTS_H

#include "../SuperGraph.h"
#include "ConstraintSetter.hpp"

class FlowConstraintSetter: public ConstraintSetter {
public:
    FlowConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_deposit_flow_constraint(const NumVarMatrix3& X, const NumVarMatrix& FDK);
    void set_flow_conservation_constraint(const NumVarMatrix3& X, const NumVarMatrix3& F, const NumVarMatrix& FDK);
    void set_flow_bounds_constraint(const NumVarMatrix3& X, const NumVarMatrix3& Y, const NumVarMatrix3& F, const NumVarMatrix& FDK, const NumVarMatrix& YDK, double capacity);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
