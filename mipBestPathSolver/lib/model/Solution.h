#ifndef SOLUTION_H
#define SOLUTION_H

#include "../graph/Edges.h"
#include <vector>

template<class Value>
struct ArcValue {
    // Zero-based virtual nodes; -1 denotes the deposit. Vehicles are one-based.
    int id, from, to, vehicle;
    Value value;

    ArcValue(int id, int from, int to, int vehicle, Value value): id(id), from(from), to(to), vehicle(vehicle), value(value) {}
    ArcValue(const SuperArc& arc, Value value): id(arc.id), from(arc.from), to(arc.to), vehicle(1), value(value) {}
};

// A detached snapshot, usable after the solver and its CPLEX environment die.
struct Solution {
    double objective = 0;
    std::vector<ArcValue<long long>> service, traversals, deposit_traversals;
    std::vector<ArcValue<double>> flow, deposit_flow;
};

#endif
