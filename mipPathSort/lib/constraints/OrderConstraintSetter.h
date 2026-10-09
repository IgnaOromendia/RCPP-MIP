#ifndef ORDER_CONSTRAINTS_H
#define ORDER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>
#include <map>

class OrderConstraintSetter: public ConstraintSetter {
public:
    OrderConstraintSetter(const VariableMatrix& Z, const vector<PathEdge>& edges, int deposit,
                          IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _edges(edges), _Z(Z), _deposit(deposit) {}

    void set_suc_pred_constraint(const vector<segment> &segments);

private:
    const vector<PathEdge>& _edges;
    const VariableMatrix& _Z;
    int _deposit;
};

#endif
