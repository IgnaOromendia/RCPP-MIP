#ifndef FIX_AND_OPTIMIZE_H
#define FIX_AND_OPTIMIZE_H

#include "../model/RCPPSolver.h"
#include "SelectionStrategy.h"

class FixAndOptimize {
public:
    FixAndOptimize(const SuperGraph& super_graph, int vehicles, int reachablity)
        : _solver(super_graph, vehicles), _super_graph(super_graph), _reachablity(reachablity) {};
    ~FixAndOptimize();

    SolveResult solve(SelectionStrategy strategy, int k = 1);

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;
    int _reachablity;

    vector<EdgeKey> top_k_neighborhood(int k, const Solution& solution);
    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k = 1);
    void select_candidates(const Solution &solution, vector<int>& candidates, vector<double>& weights);
};


#endif
