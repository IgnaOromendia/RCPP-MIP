#ifndef PATH_CONSTRAINTS_H
#define PATH_CONSTRAINTS_H

#include <ConstraintSetter.hpp>

#include "../graph/SuperGraph.h"

class PathConstraintSetter: public ConstraintSetter {
public:
    PathConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_service_constraint(const VariableMatrix& X);
	void set_continuity_constraint(const VariableMatrix& X, const VariableMatrix& Y, const VariableMatrix& YDK, const VariableMatrix& YKD);
    void set_deposit_arrival_constraint(const VariableMatrix& YKD);
	void set_deposit_departure_constraint(const VariableMatrix& YDK);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
