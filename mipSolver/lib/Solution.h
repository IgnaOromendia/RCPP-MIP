#ifndef SOLUTION_H
#define SOLUTION_H

#include <vector>

template<class Value>
struct ArcValue {
    // Zero-based virtual nodes; -1 denotes the deposit. Vehicles are one-based.
    int from, to, vehicle;
    Value value;
};

// A detached snapshot, usable after the solver and its CPLEX environment die.
struct Solution {
    double objective = 0;
    std::vector<ArcValue<long long>> service, traversals, deposit_traversals;
    std::vector<ArcValue<double>> flow, deposit_flow;
};

#endif
