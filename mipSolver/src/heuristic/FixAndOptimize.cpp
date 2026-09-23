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

edgeKeySet FixAndOptimize::random_neighborhood() {
    vector<EdgeKey> edges = _super_graph.random_edge_neighborhood(_reachablity);
    return edgeKeySet(edges.begin(), edges.end());
}

edgeKeySet FixAndOptimize::top_k_neighborhood(int k, const Solution &solution) {
    const double _weight_threshold = 0.5;

    vector<double> weights;    
    vector<int> candidates;
    select_candidates(solution, candidates, weights);

    edgeKeySet arcs;

    int selected_edges = 0;
    double last_weight = -1;

    for (size_t i = 0; i < candidates.size() and selected_edges < k; i++) {
        const SuperArc* arc = _super_graph.super_arc_with_id(candidates[i]);

        if (last_weight > 0) {
            double diff = last_weight - weights[candidates[i]];
            if (diff > last_weight * _weight_threshold)
                break;
        }

        if (arcs.count(EdgeKey(arc->from, arc->to)) != 0)
            continue;

        selected_edges++;

        last_weight = weights[candidates[i]];
        vector<EdgeKey> neighborhood = _super_graph.bfs_tree(arc->from, _reachablity);
        arcs.insert(neighborhood.begin(), neighborhood.end());
    }

    return arcs;
}

SolveResult FixAndOptimize::fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k){
    if (!S.has_solution)
        return S;

    Solution solution = S.extract_solution();

    edgeKeySet free_edges = strategy == SelectionStrategy::Random ? random_neighborhood() : top_k_neighborhood(k, solution);

    if (free_edges.empty()) free_edges = random_neighborhood();
    
    // Fijar variables y resolver
    SolveResult candidate = _solver.solve_neighborhood(solution, free_edges, gapTolerance);

    if (candidate.has_solution and candidate.get_obj_value() < S.get_obj_value()) 
        return candidate;
    
    return S;
}

void FixAndOptimize::select_candidates(const Solution &solution, vector<int> &candidates, vector<double> &weights) {
    weights.assign(_super_graph.arcs_amount(), 0.0);
    
    // Calculates weights and adds candiadtes
    for (const auto& traversal : solution.traversals) {
        const SuperArc& arc = *_super_graph.super_arc_with_id(traversal.id);
        if (arc.edge_id < 0 || traversal.value <= 0) continue;

        double Y_value = traversal.value;
        weights[arc.id] += arc.cost * Y_value;

        candidates.push_back(arc.id);
    }

    // Sort candidates by weight
    sort(candidates.begin(), candidates.end(),
        [&weights](int lhs, int rhs) {
            if (weights[lhs] != weights[rhs])
                return weights[lhs] > weights[rhs];
            return lhs < rhs;
        });
}
