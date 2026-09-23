#include "../../lib/heuristic/FixAndOptimize.h"
#include <algorithm>

FixAndOptimize::~FixAndOptimize(){}

SolveResult FixAndOptimize::solve(SelectionStrategy strategy, int k) {
    double gapTolerance = 0.2;

    _solver.generate_MIP();
    _solver.set_time_objective();
    SolveResult best = _solver.solve(gapTolerance);

    if (!best.has_solution)
        return best;

    const int maxIterations = 200;
    const int maxWithoutImprovement = 20;
    const double eps = 1e-6;

    int withoutImprovement = 0;

    for (int i = 0; i < maxIterations && withoutImprovement < maxWithoutImprovement; i++) {
        const double previous = best.get_obj_value();
        best = fix_and_optimize(best, gapTolerance, strategy, k);

        if (withoutImprovement == 10 and gapTolerance > 0.05)
            gapTolerance /= 2;

        if (previous - best.get_obj_value() > eps)
            withoutImprovement = 0;
        else
            ++withoutImprovement;
    }

    return best;
}

vector<EdgeKey> FixAndOptimize::top_k_neighborhood(int k, const Solution& solution) {
    vector<double> weights(_super_graph.arcs_amount(), 0.0);
    const double _weight_threshold = 0.5;

    for (const auto& traversal : solution.traversals) {
        const SuperArc& arc = *_super_graph.super_arc_with_id(traversal.id);
        if (arc.edge_id < 0 || traversal.value <= 0) continue;
        double Y_value = traversal.value;
        weights[arc.id] += arc.cost * Y_value;
    }

    vector<int> candidates;
    for (int i = 0; i < _super_graph.arcs_amount(); i++)
        if (weights[i] > 0.0)
            candidates.push_back(i);

    sort(candidates.begin(), candidates.end(),
        [&weights](int lhs, int rhs) {
            if (weights[lhs] != weights[rhs])
                return weights[lhs] > weights[rhs];
            return lhs < rhs;
        });

    vector<EdgeKey> arcs;

    int selected_edges = 0;
    double last_weight = -1;

    for (size_t i = 0; i < candidates.size() and selected_edges < k; i++) {
        const SuperArc* arc = _super_graph.super_arc_with_id(candidates[i]);

        if (last_weight > 0) {
            double diff = last_weight - weights[candidates[i]];
            if (diff > last_weight * _weight_threshold)
                break;
        }

        if (std::find(arcs.begin(), arcs.end(), EdgeKey(arc->from, arc->to)) != arcs.end())
            continue;

        if (arc->pair != -1) {
            const SuperArc* pair = _super_graph.super_arc_with_id(arc->pair);
            if (std::find(arcs.begin(), arcs.end(), EdgeKey(pair->from, pair->to)) != arcs.end())
                continue;
        }

        selected_edges++;

        last_weight = weights[candidates[i]];
        vector<EdgeKey> neighborhood = _super_graph.bfs_tree(arc->from, _reachablity);
        arcs.insert(arcs.end(), neighborhood.begin(), neighborhood.end());
    }

    return arcs;
}

SolveResult FixAndOptimize::fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k){
    if (!S.has_solution)
        return S;

    Solution solution = S.extract_solution();

    vector<EdgeKey> free_edges;
    
    if (strategy != SelectionStrategy::Random)
        free_edges = top_k_neighborhood(k, solution);

    std::sort(free_edges.begin(), free_edges.end());
    free_edges.erase(
        std::unique(free_edges.begin(), free_edges.end()),
        free_edges.end());

    if (free_edges.empty())
        free_edges = _super_graph.random_edge_neighborhood(_reachablity);

    // Fijar variables
    SolveResult candidate = _solver.solve_neighborhood(solution, free_edges, gapTolerance);

    if (candidate.has_solution and candidate.get_obj_value() < S.get_obj_value()) {
        return candidate;
    }
    
    return S;
}
