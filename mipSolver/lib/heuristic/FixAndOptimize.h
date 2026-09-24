#ifndef FIX_AND_OPTIMIZE_H
#define FIX_AND_OPTIMIZE_H

#include "../model/RCPPSolver.h"
#include "SelectionStrategy.h"

class FixAndOptimize {
public:
    FixAndOptimize(const SuperGraph& super_graph, int vehicles, int reachablity);
    ~FixAndOptimize();

    SolveResult solve(SelectionStrategy strategy);

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;
    int _reachablity;

    vector<int> _penalty;

    edgeKeySet random_neighborhood();
    edgeKeySet top_k_neighborhood(int k, const Solution& solution, int penalty);
    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k, int penalty);
    void select_candidates(const Solution &solution, vector<int>& candidates, vector<double>& weights);
};


#endif
