#ifndef ORDER_CONSTRAINTS_H
#define ORDER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class OrderConstraintSetter: public ConstraintSetter {
public:
    OrderConstraintSetter(const ArcVariables& Z, const vector<PathEdge>& edges,
                          const vector<int>& depo_dist, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _edges(edges), _K(K), _Z(Z), _depo_dist(depo_dist) {}

    void set_passes_amount_constraint();
    void set_position_over_pass_constraint();
    void set_continuity_constraint(int n);
    void set_depo_return_constraint(int deposit);
    void set_depo_arrival_constraint(int deposit);

private:
    const vector<PathEdge>& _edges;
    const int _K;
    const ArcVariables& _Z;
    const vector<int> &_depo_dist;
};

#endif
