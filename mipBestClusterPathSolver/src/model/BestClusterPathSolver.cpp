#include <model/BestClusterPathSolver.h>
#include <stdexcept>

BestClusterPathSolver::BestClusterPathSolver(
    const SuperGraph& super_graph,
    const map<int, vector<EdgeKey>>& in_out_edges_by_cluster,
    ModelOptions options)
    : RCPPSolver(super_graph, 1, options) {
        
    map<EdgeKey, int> arc_id_by_edge;

    for (const SuperArc& arc : super_graph.arcs()) 
        if (arc.edge_id >= 0)
            arc_id_by_edge.emplace(EdgeKey(arc.from, arc.to), arc.id);

    for (const auto& [cluster, edges] : in_out_edges_by_cluster) {
        vector<int>& ids = _in_out_arc_ids_by_cluster[cluster];
        ids.reserve(edges.size());
        for (const EdgeKey& edge : edges)
            ids.push_back(arc_id_by_edge.at(edge));
    }
}

const map<int, vector<int>>&
BestClusterPathSolver::in_out_arc_ids_by_cluster() const noexcept {
    return _in_out_arc_ids_by_cluster;
}

void BestClusterPathSolver::set_objective() {
    IloExpr objective = create_expression();
    for (int vehicle = 1; vehicle < _trucks; ++vehicle) {
        for (const auto& [_, arc_ids] : _in_out_arc_ids_by_cluster) {
            for (int arc_id : arc_ids)
                objective += _Y[arc_id][vehicle];
        }
    }
    add_minimization_objective(objective);
    objective.end();
}
