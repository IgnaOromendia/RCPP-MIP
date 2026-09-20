#ifndef FIX_AND_OPTIMIZE_H
#define FIX_AND_OPTIMIZE_H

#include "../model/RCPPSolver.h"
#include <random>

enum class SelectionStrategy {
    Random,
    DeadheadCost,
};

class FixAndOptimize {
public:
    FixAndOptimize(const SuperGraph& super_graph, int vehicles, int reachablity);
    ~FixAndOptimize();

    SolveResult solve(SelectionStrategy strategy);

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;
    int _reachablity;
    std::mt19937 _generator;
    int _last_deadhead_arc = -1;

    int select_deadhead_arc(const Solution& solution);
    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy);
};


#endif
