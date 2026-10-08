#ifndef MODULE_CONSTRAINTS_H
#define MODULE_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class ModuleConstraintSetter: public ConstraintSetter {
public:
    ModuleConstraintSetter(const vector<PathEdge>& edges, const vector<segment>& segments, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _edges(edges), _segments(segments) {}

    void set_module_constraints(const VariableMatrix& D, const IloNumVarArray& X);

private:
    const vector<PathEdge>& _edges;
    const vector<segment>& _segments;
};

#endif
