#ifndef FO_SOLVER_H
#define FO_SOLVER_H

#include "RCPPSolver.h"

class FOSolver {
public:
    FOSolver(const SuperGraph& super_graph, int vehicles);
    ~FOSolver();

    SolveResult solve();

private:
    RCPPSolver _solver;
    const SuperGraph& _super_graph;

    SolveResult fix_and_optimize(const SolveResult& S, double gapTolerance = 0);
};


#endif