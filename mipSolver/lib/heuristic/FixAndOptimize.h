#ifndef FIX_AND_OPTIMIZE_H
#define FIX_AND_OPTIMIZE_H

#include "../model/RCPPSolver.h"
#include "SelectionStrategy.h"

class FixAndOptimize {
public:
    FixAndOptimize(const SuperGraph& super_graph, int vehicles);
    ~FixAndOptimize();

    SolveResult solve(SelectionStrategy strategy);

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;

    vector<int> _penalty;

    edgeKeySet random_neighborhood(int reachability);
    edgeKeySet top_k_neighborhood(int k, const Solution& solution, int penalty, int reachability);
    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k, int penalty, int reachability);
    void select_candidates(const Solution &solution, vector<int>& candidates, vector<double>& weights);
};


#endif
