#include "../../lib/heuristic/FixAndOptimize.h"
#include <algorithm>

FixAndOptimize::FixAndOptimize(const SuperGraph &super_graph, int vehicles)
    : _solver(super_graph, vehicles), _super_graph(super_graph) {
    _penalty.assign(super_graph.arcs_amount(), 0);
}

FixAndOptimize::~FixAndOptimize() {}

SolveResult FixAndOptimize::solve(SelectionStrategy strategy) {
    int k = 6;
    const int k_delta = 2;
    const int k_min = 2;
    const int k_max = 10;

    double gapTolerance = 0.2;
    const double gapInitial = 0.05;

    int penalty = 1;
    const int penalty_delta = 1;    
    const int penalty_min = 1;
    const int penalty_max = 3;

    int reachability = 15;
    const int reach_delta = 5;
    const int reach_min = 5;
    const int reach_max = 25;

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
        best = fix_and_optimize(best, gapTolerance, strategy, k, penalty, reachability);

        // Updates penalites
        for(int i = 0; i < _super_graph.arcs_amount(); i++)
            _penalty[i] = max(0, _penalty[i] - 1);

        if (withoutImprovement == 5 and gapTolerance > 0.05) {
            gapTolerance /= 2;
            k = min(k_max, k + k_delta);
            penalty = max(penalty_min, penalty - penalty_delta);
            reachability = max(reach_min, reachability - reach_delta);
        }

        if (previous - best.get_obj_value() > eps) {
            withoutImprovement = 0;
            k = max(k_min, k - k_delta);
            penalty = min(penalty_max, penalty + penalty_delta);
            reachability = min(reach_max, reachability + reach_delta);
            gapTolerance = gapInitial;
        } 
        else {
            ++withoutImprovement;
        }
            
    }

    return best;
}

edgeKeySet FixAndOptimize::random_neighborhood(int reachability) {
    vector<EdgeKey> edges = _super_graph.random_edge_neighborhood(reachability);
    return edgeKeySet(edges.begin(), edges.end());
}

edgeKeySet FixAndOptimize::top_k_neighborhood(int k, const Solution &solution, int penalty, int reachability) {
    const double weight_threshold = 0.5;

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
            if (diff > last_weight * weight_threshold)
                break;
        }

        if (arcs.count(EdgeKey(arc->from, arc->to)) != 0)
            continue;

        selected_edges++;

        _penalty[candidates[i]] = penalty;

        last_weight = weights[candidates[i]];
        vector<EdgeKey> neighborhood = _super_graph.bfs_tree(arc->from, reachability);
        arcs.insert(neighborhood.begin(), neighborhood.end());
    }

    return arcs;
}

SolveResult FixAndOptimize::fix_and_optimize(const SolveResult& S, double gapTolerance, SelectionStrategy strategy, int k, int penalty, int reachability){
    if (!S.has_solution)
        return S;

    Solution solution = S.extract_solution();

    edgeKeySet free_edges = strategy == SelectionStrategy::Random ? random_neighborhood(reachability) : top_k_neighborhood(k, solution, penalty, reachability);

    if (free_edges.empty()) free_edges = random_neighborhood(reachability);
    
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
        if (arc.edge_id < 0 or traversal.value <= 0) continue;

        if (_penalty[arc.id] > 0) continue;

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
