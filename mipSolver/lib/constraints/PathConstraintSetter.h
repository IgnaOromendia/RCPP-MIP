#ifndef PATH_CONSTRAINTS_H
#define PATH_CONSTRAINTS_H

#include "../graph/SuperGraph.h"
#include "ConstraintSetter.hpp"

class PathConstraintSetter: public ConstraintSetter {
public:
    PathConstraintSetter(const SuperGraph& graph, int truck_limit, IloEnv& env, IloModel& model) 
        : ConstraintSetter(env, model), _super_graph(graph), _truck_limit(truck_limit) {}
    
    void set_service_constraint(const ArcVariables& X);
	void set_continuity_constraint(const ArcVariables& X, const ArcVariables& Y, const ArcVariables& YDK, const ArcVariables& YKD);
    void set_deposit_arrival_constraint(const ArcVariables& YKD);
	void set_deposit_departure_constraint(const ArcVariables& YDK);

private:
    const SuperGraph& _super_graph;
    int _truck_limit;

};

#endif
