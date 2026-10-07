#ifndef FIRST_PASS_CONSTRAINTS_H
#define FIRST_PASS_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class FirstPassConstraintSetter: public ConstraintSetter {
public:
    FirstPassConstraintSetter(const ArcVariables& A, const vector<PathEdge>& edges, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _A(A), _edge_amount(edges.size()),
          _K(K), _edges(edges) {}

    void set_seen_continuity_constraint();
    void set_seen_presence_constraint(const ArcVariables& Z);
    void set_seen_activation_constraint(const ArcVariables& Z);

private:
    const ArcVariables& _A;
    const int _edge_amount;
    const int _K;
    const vector<PathEdge>& _edges;
};

#endif
