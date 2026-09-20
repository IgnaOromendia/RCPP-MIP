#ifndef FLOW_CONSTRAINTS_H
#define FLOW_CONSTRAINTS_H

#include "../graph/SuperGraph.h"
#include "ConstraintSetter.hpp"

class FlowConstraintSetter: public ConstraintSetter {
public:
    FlowConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_deposit_flow_constraint(const ArcVariables& X, const ArcVariables& FDK);
    void set_flow_conservation_constraint(const ArcVariables& X, const ArcVariables& F, const ArcVariables& FDK);
    void set_flow_bounds_constraint(const ArcVariables& X, const ArcVariables& Y, const ArcVariables& F, const ArcVariables& FDK, const ArcVariables& YDK, double capacity);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
