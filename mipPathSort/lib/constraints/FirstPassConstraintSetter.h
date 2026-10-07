#ifndef FIRST_PASS_CONSTRAINTS_H
#define FIRST_PASS_CONSTRAINTS_H

#include <ConstraintSetter.hpp>

class FirstPassConstraintSetter: public ConstraintSetter {
public:
    FirstPassConstraintSetter(const ArcVariables& A, int edge_amount, int K,
                              IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _A(A), _edge_amount(edge_amount),
          _K(K) {}

    void set_unique_first_pass_constraint();
    void set_first_pass_presence_constraint(const ArcVariables& Z);
    void set_no_pass_before_first_constraint(const ArcVariables& Z);

private:
    const ArcVariables& _A;
    const int _edge_amount;
    const int _K;
};

#endif
