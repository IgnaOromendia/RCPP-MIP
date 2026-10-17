#include <model/BestClusterPathSolver.h>
#include <set>
#include <stdexcept>

BestClusterPathSolver::BestClusterPathSolver(
    const SuperGraph& super_graph,
    int vehicles,
    const std::map<int, std::vector<EdgeKey>>& in_out_edges_by_cluster,
    ModelOptions options)
    : RCPPSolver(super_graph, vehicles, options) {
    std::map<EdgeKey, int> arc_id_by_edge;
    for (const SuperArc& arc : super_graph.arcs())
        arc_id_by_edge.emplace(EdgeKey(arc.from, arc.to), arc.id);

    std::set<int> seen_arc_ids;
    for (const auto& [cluster, edges] : in_out_edges_by_cluster) {
        if (cluster <= 0)
            throw std::invalid_argument("El cluster de una arista in/out debe ser positivo");
        if (edges.empty())
            throw std::invalid_argument("Un cluster in/out no puede estar vacio");
        auto& ids = _in_out_arc_ids_by_cluster[cluster];
        ids.reserve(edges.size());
        for (const EdgeKey& edge : edges) {
            const auto found = arc_id_by_edge.find(edge);
            if (found == arc_id_by_edge.end())
                throw std::invalid_argument(
                    "Una arista in/out no pertenece al supergrafo");
            const SuperArc& arc = *super_graph.super_arc_with_id(found->second);
            if (arc.edge_id < 0)
                throw std::invalid_argument(
                    "Una arista in/out no puede ser un giro o conector de deposito");
            if (!seen_arc_ids.insert(arc.id).second)
                throw std::invalid_argument(
                    "Una arista in/out aparece en mas de un cluster");
            ids.push_back(arc.id);
        }
    }
    if (_in_out_arc_ids_by_cluster.empty())
        throw std::invalid_argument("La instancia no contiene aristas in/out");
}

const std::map<int, std::vector<int>>&
BestClusterPathSolver::in_out_arc_ids_by_cluster() const noexcept {
    return _in_out_arc_ids_by_cluster;
}

void BestClusterPathSolver::set_objective() {
    IloExpr objective = create_expression();
    for (int vehicle = 1; vehicle < _trucks; ++vehicle) {
        for (const auto& [cluster, arc_ids] : _in_out_arc_ids_by_cluster) {
            (void) cluster;
            for (int arc_id : arc_ids)
                objective += _Y[arc_id][vehicle];
        }
    }
    add_minimization_objective(objective);
    objective.end();
}
