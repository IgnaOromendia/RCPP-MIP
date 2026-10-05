#ifndef MODULE_CONSTRAINTS_H
#define MODULE_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class ModuleConstraintSetter: public ConstraintSetter {
public:
    ModuleConstraintSetter(const vector<PathEdge>& edges, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _edges(edges) {}

    void set_module_constraints(const ArcVariables& D, const ArcVariables& X);

private:
    const vector<PathEdge>& _edges;
};

#endif
