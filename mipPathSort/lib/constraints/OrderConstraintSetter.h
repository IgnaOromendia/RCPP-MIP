#ifndef ORDER_CONSTRAINTS_H
#define ORDER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class OrderConstraintSetter: public ConstraintSetter {
public:
    OrderConstraintSetter(const ArrayArcVariables& Z, const vector<PathEdge>& edges, const vector<int>& pass_count, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _edges(edges), _pass_count(pass_count), _K(K), _Z(Z) {}

    void set_pass_over_position_constraint();
    void set_position_over_pass_constraint();
    void set_continuity_constraint(int n);
    void set_circuit_constraint(int deposit);
    void set_deposit_constraint(int deposit);

private:
    const vector<PathEdge>& _edges;
    const int _K;
    const ArrayArcVariables& _Z;
    const vector<int>& _pass_count;
};

#endif
