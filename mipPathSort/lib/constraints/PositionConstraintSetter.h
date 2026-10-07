#ifndef POSITION_CONSTRAINTS_H
#define POSITION_CONSTRAINTS_H

#include <ConstraintSetter.hpp>

class PositionConstraintSetter: public ConstraintSetter {
public:
    PositionConstraintSetter(const IloNumVarArray& X, int edge_amount, int K,
                             IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _X(X), _edge_amount(edge_amount),
          _K(K) {}

    void set_position_constraint(const ArcVariables& A);

private:
    const IloNumVarArray& _X;
    const int _edge_amount;
    const int _K;
};

#endif
