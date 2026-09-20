#include "../../lib/heuristic/FixAndOptimize.h"
#include <algorithm>

FixAndOptimize::FixAndOptimize(const SuperGraph& super_graph, int vehicles, int reachablity)
    : _solver(super_graph, vehicles), _super_graph(super_graph), _reachablity(reachablity),
      _generator(std::random_device{}()) {
    _solver.generate_MIP();
    _solver.set_time_objective();
}

FixAndOptimize::~FixAndOptimize(){}

int FixAndOptimize::select_deadhead_arc(const Solution& solution) {
    vector<double> weights(_super_graph.arcs_amount(), 0.0);

    for (const auto& traversal : solution.traversals) {
        const SuperArc& arc = *_super_graph.super_arc_with_id(traversal.id);
        if (arc.edge_id < 0 || traversal.value <= 0) continue;
        weights[arc.id] += arc.cost * static_cast<double>(traversal.value);
    }

    bool has_alternative = false;
    for (int arc_id = 0; arc_id < static_cast<int>(weights.size()); ++arc_id) {
        if (arc_id != _last_deadhead_arc && weights[arc_id] > 0.0) {
            has_alternative = true;
            break;
        }
    }
    if (has_alternative && _last_deadhead_arc >= 0 &&
        _last_deadhead_arc < static_cast<int>(weights.size())) {
        weights[_last_deadhead_arc] = 0.0;
    }

    if (std::none_of(weights.begin(), weights.end(),
                     [](double weight) { return weight > 0.0; })) {
        return -1;
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    _last_deadhead_arc = distribution(_generator);
    return _last_deadhead_arc;
}

SolveResult FixAndOptimize::solve(SelectionStrategy strategy) {
    double gapTolerance = 0.1;
    SolveResult best = _solver.solve(gapTolerance);

    if (!best.has_solution)
        return best;

    const int maxIterations = 100;
    const int maxWithoutImprovement = 20;
    const double eps = 1e-6;

    int withoutImprovement = 0;

    for (int i = 0; i < maxIterations && withoutImprovement < maxWithoutImprovement; i++) {
        const double previous = best.get_obj_value();
        best = fix_and_optimize(best, gapTolerance, strategy);

        if (previous - best.get_obj_value() > eps)
            withoutImprovement = 0;
        else
            ++withoutImprovement;
    }

    return best;
}

SolveResult FixAndOptimize::fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy){
    if (!S.has_solution)
        return S;

    Solution solution = S.extract_solution();

    // Seleccionar nodos
    vector<pair<int, int>> free_nodes;

    if (strategy == SelectionStrategy::DeadheadCost) {
        const int selected_arc = select_deadhead_arc(solution);
        if (selected_arc >= 0) {
            const SuperArc* arc = _super_graph.super_arc_with_id(selected_arc);
            free_nodes = _super_graph.edge_subset(arc->from, _reachablity);
        } else {
            free_nodes = _super_graph.random_edge_subset(_reachablity);
        }
    } else {
        free_nodes = _super_graph.random_edge_subset(_reachablity);
    }

    // Fijar variables
    SolveResult candidate = _solver.solve_neighborhood(solution, free_nodes, gapTolerance);

    if (candidate.has_solution and candidate.get_obj_value() < S.get_obj_value()) {
        return candidate;
    }
    
    return S;
}
