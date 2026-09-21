#include "../../lib/heuristic/FixAndOptimize.h"
#include <algorithm>

FixAndOptimize::FixAndOptimize(const SuperGraph& super_graph, int vehicles, int reachablity)
    : _solver(super_graph, vehicles), _super_graph(super_graph), _reachablity(reachablity) {
    _solver.generate_MIP();
    _solver.set_time_objective();
}

FixAndOptimize::~FixAndOptimize(){}

vector<int> FixAndOptimize::select_top_k_deadhead_arc(int k, const Solution& solution) {
    vector<double> weights(_super_graph.arcs_amount(), 0.0);

    for (const auto& traversal : solution.traversals) {
        const SuperArc& arc = *_super_graph.super_arc_with_id(traversal.id);
        if (arc.edge_id < 0 || traversal.value <= 0) continue;
        double Y_value = traversal.value;
        weights[arc.id] += arc.cost * Y_value;
    }

    vector<int> candidates;
    for (int i = 0; i < _super_graph.arcs_amount(); i++) {
        if (weights[i] > 0.0)
            candidates.push_back(i);
    }

    const std::size_t count = std::min<std::size_t>(k, candidates.size());

    std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end(),
        [&weights](int lhs, int rhs) {
            if (weights[lhs] != weights[rhs])
                return weights[lhs] > weights[rhs];
            return lhs < rhs;
        });

    candidates.resize(count);
    return candidates;
}

SolveResult FixAndOptimize::solve(SelectionStrategy strategy, int k) {
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
        best = fix_and_optimize(best, gapTolerance, strategy, k);

        if (previous - best.get_obj_value() > eps)
            withoutImprovement = 0;
        else
            ++withoutImprovement;
    }

    return best;
}

SolveResult FixAndOptimize::fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k){
    if (!S.has_solution)
        return S;

    Solution solution = S.extract_solution();

    vector<pair<int, int>> free_edges;
    vector<int> selected; 
    
    if (strategy != SelectionStrategy::Random)
        selected = select_top_k_deadhead_arc(k, solution);

    for (int arc_id: selected) {
        const SuperArc* arc = _super_graph.super_arc_with_id(arc_id);
        vector<pair<int, int>> neighborhood = _super_graph.edge_subset(arc->from, _reachablity);
        free_edges.insert(free_edges.end(), neighborhood.begin(), neighborhood.end());
    }

    std::sort(free_edges.begin(), free_edges.end());
    free_edges.erase(
        std::unique(free_edges.begin(), free_edges.end()),
        free_edges.end());

    if (free_edges.empty())
        free_edges = _super_graph.random_edge_subset(_reachablity);

    // Fijar variables
    SolveResult candidate = _solver.solve_neighborhood(solution, free_edges, gapTolerance);

    if (candidate.has_solution and candidate.get_obj_value() < S.get_obj_value()) {
        return candidate;
    }
    
    return S;
}
