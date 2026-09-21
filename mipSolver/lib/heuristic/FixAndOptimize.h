#ifndef FIX_AND_OPTIMIZE_H
#define FIX_AND_OPTIMIZE_H

#include "../model/RCPPSolver.h"
#include "SelectionStrategy.h"

class FixAndOptimize {
public:
    FixAndOptimize(const SuperGraph& super_graph, int vehicles, int reachablity);
    ~FixAndOptimize();

    SolveResult solve(SelectionStrategy strategy, int k = 1);

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;
    int _reachablity;
    int _last_deadhead_arc = -1;

    vector<int> select_top_k_deadhead_arc(int k, const Solution& solution);
    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k = 1);
};


#endif
