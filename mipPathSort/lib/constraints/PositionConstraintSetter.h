#ifndef POSITION_CONSTRAINTS_H
#define POSITION_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class PositionConstraintSetter: public ConstraintSetter {
public:
    PositionConstraintSetter(const IloNumVarArray& X, const vector<PathEdge>& edges, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _X(X), _edge_amount(edges.size()), _K(K), _edges(edges) {}

    void set_position_constraint(const ArcVariables& A, const ArcVariables& Z);

private:
    const IloNumVarArray& _X;
    const int _edge_amount;
    const int _K;
    const vector<PathEdge>& _edges;
};

#endif
