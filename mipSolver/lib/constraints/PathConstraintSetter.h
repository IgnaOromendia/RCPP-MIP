#ifndef PATH_CONSTRAINTS_H
#define PATH_CONSTRAINTS_H

#include "../graph/SuperGraph.h"
#include "ConstraintSetter.hpp"

class PathConstraintSetter: public ConstraintSetter {
public:
    PathConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_service_constraint(const NumVarMatrix3& X);
	void set_continuity_constraint(const NumVarMatrix3& X, const NumVarMatrix3& Y, const NumVarMatrix& YDK, const NumVarMatrix& YKD);
    void set_depoist_arrival_constraint(const NumVarMatrix& YKD);
	void set_depoist_departure_constraint(const NumVarMatrix& YDK);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
