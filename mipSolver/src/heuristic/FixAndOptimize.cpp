#include "../../lib/heuristic/FixAndOptimize.h"

FixAndOptimize::FixAndOptimize(const SuperGraph& super_graph, int vehicles): _solver(super_graph, vehicles), _super_graph(super_graph) {
    _solver.generate_MIP();
    _solver.set_time_objective();
}

FixAndOptimize::~FixAndOptimize(){}

SolveResult FixAndOptimize::solve() {
    SolveResult best = _solver.solve(0.1);

    if (!best.has_solution)
        return best;

    const int maxIterations = 100;
    const int maxWithoutImprovement = 20;
    const double eps = 1e-6;

    int withoutImprovement = 0;

    for (int i = 0; i < maxIterations && withoutImprovement < maxWithoutImprovement; i++) {
        const double previous = best.get_obj_value();
        best = fix_and_optimize(best);

        if (previous - best.get_obj_value() > eps)
            withoutImprovement = 0;
        else
            ++withoutImprovement;
    }

    return best;
}

SolveResult FixAndOptimize::fix_and_optimize(const SolveResult& S, double gapTolerance) {
    if (!S.has_solution)
        return S;

    // Seleccionar nodos
    vector<pair<int, int>> free_nodes = _super_graph.edge_subset(5);

    // Fijar variables
    SolveResult candidate = _solver.solve_neighborhood(S.extract_solution(), free_nodes, gapTolerance);

    if (candidate.has_solution and candidate.get_obj_value() < S.get_obj_value()) {
        return candidate;
    }
    
    return S;
}
