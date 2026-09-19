#ifndef FIX_AND_OPTIMIZE_H
#define FIX_AND_OPTIMIZE_H

#include "../model/RCPPSolver.h"

class FixAndOptimize {
public:
    FixAndOptimize(const SuperGraph& super_graph, int vehicles);
    ~FixAndOptimize();

    SolveResult solve();

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;

    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance = 0);
};


#endif