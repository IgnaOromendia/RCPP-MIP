#ifndef POSITION_CONSTRAINTS_H
#define POSITION_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/PathSortInstance.h>
#include <map>

class PositionConstraintSetter: public ConstraintSetter {
public:
    PositionConstraintSetter(const IloNumVarArray& X, const VariableArray& Z,
        int K, const vector<PathEdge>& edges, const vector<segment>& segments,
        IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _X(X), _Z(Z), _edges(edges),
          _segments(segments), _K(K) {}

    void set_deposit_constraint(int deposit);
    void set_position_order_constraint(int deposit);
    void set_passes_order_constraint(const map<segment, int>& segment_map);

private:
    const IloNumVarArray& _X;
    const VariableArray& _Z;
    const vector<PathEdge>& _edges;
    const vector<segment>& _segments;
    int _K;
};

#endif
