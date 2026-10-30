#include <model/BestClusterPathSolver.h>
#include <stdexcept>

BestClusterPathSolver::BestClusterPathSolver(const SuperGraph& super_graph, const set<int>& neutral_edges, ModelOptions options)
    : RCPPSolver(super_graph, 1, options) {
        
    _neutral_arc_ids.reserve(neutral_edges.size());

    for(const SuperArc& arc: super_graph.arcs()) {
        if (neutral_edges.count(arc.edge_id) == 0) continue;
        _neutral_arc_ids.push_back(arc.id);
    }
}


void BestClusterPathSolver::set_objective() {
    IloExpr objective = create_expression();
    for (int vehicle = 1; vehicle < _trucks; ++vehicle) {
        for (const int arc_id : _neutral_arc_ids) {
            objective += _Y[arc_id][vehicle];
        }
    }
    add_minimization_objective(objective);
    objective.end();
}
