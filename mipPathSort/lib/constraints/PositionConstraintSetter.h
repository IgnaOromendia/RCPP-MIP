#ifndef POSITION_CONSTRAINTS_H
#define POSITION_CONSTRAINTS_H

#include <ConstraintSetter.hpp>

class PositionConstraintSetter: public ConstraintSetter {
public:
    PositionConstraintSetter(const ArcVariables& X, const vector<int>& pass_count, int edge_amount, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _pass_count(pass_count), _X(X), _K(K), _edge_amount(edge_amount) {}

    void set_position_constraint(const ArrayArcVariables& Z);
    void set_order_constraint();

private:

    const int _edge_amount, _K;
    const ArcVariables& _X;
    const vector<int>& _pass_count;

};

#endif