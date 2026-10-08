#ifndef PATH_CONSTRAINTS_H
#define PATH_CONSTRAINTS_H

#include <ConstraintSetter.hpp>

#include "../graph/SuperGraph.h"

class PathConstraintSetter: public ConstraintSetter {
public:
    PathConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_service_constraint(const VariableArray& X);
	void set_continuity_constraint(const VariableArray& X, const VariableArray& Y, const VariableArray& YDK, const VariableArray& YKD);
    void set_deposit_arrival_constraint(const VariableArray& YKD);
	void set_deposit_departure_constraint(const VariableArray& YDK);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
