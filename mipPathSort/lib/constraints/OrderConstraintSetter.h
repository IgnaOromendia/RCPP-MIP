#ifndef ORDER_CONSTRAINTS_H
#define ORDER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>

class OrderConstraintSetter: public ConstraintSetter {
public:
    OrderConstraintSetter(const VariableArray& Z, const vector<PathEdge>& edges,
                          const vector<segment>& segments, int deposit,
                          IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _edges(edges), _Z(Z),
          _segments(segments), _deposit(deposit) {}

    void set_suc_pred_constraint();

private:
    const vector<PathEdge>& _edges;
    const VariableArray& _Z;
    const vector<segment>& _segments;
    int _deposit;
};

#endif
