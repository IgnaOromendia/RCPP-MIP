#include <model/ClusterRCPPSolver.h>
#include <constraints/FlowConstraintSetter.h>
#include <constraints/PathConstraintSetter.h>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

ClusterRCPPSolver::ClusterRCPPSolver(const SuperGraph& super_graph, const ClusterBoundary& boundary, ModelOptions options)
    : RCPPSolver(super_graph, 1, options), _boundary(boundary) {

    for (const BoundaryGate& gate : _boundary.gates)
        _max_boundary_multiplicity = max({_max_boundary_multiplicity, gate.entries, gate.exits});
    
}

IloNum ClusterRCPPSolver::deposit_traversal_upper_bound() const {
    return static_cast<IloNum>(_max_boundary_multiplicity);
}

void ClusterRCPPSolver::set_boundary_constraints() {
    map<int, pair<long long, long long>> required_boundary_counts;
    set<int> boundary_nodes;

    for (const BoundaryGate& gate : _boundary.gates) {
        required_boundary_counts[gate.global_node] = {gate.entries, gate.exits};
        boundary_nodes.insert(gate.global_node);
    }

    constexpr int vehicle = 1;

    for (int node : boundary_nodes) {
        const auto& [entry_count, exit_count] = required_boundary_counts.at(node);

        IloExpr entries(_env), exits(_env);
        int entry_terms = 0;
        int exit_terms = 0;

        for (const SuperArc* arc : _super_graph.super_arcs_adj_depo_node()) {
            if (_super_graph.original_node(arc->to) == node) {
                entries += _YDK[arc->id][vehicle];
                ++entry_terms;
            }
        }

        for (const SuperArc* arc : _super_graph.super_arcs_adj_node_depo()) {
            if (_super_graph.original_node(arc->from) == node) {
                exits += _YKD[arc->id][vehicle];
                ++exit_terms;
            }
        }
        
        const string suffix = to_string(node + 1) + "_" + to_string(vehicle);
        add_constraint(entry_count, entries, entry_count, "Entrada_cluster_" + suffix);
        add_constraint(exit_count, exits, exit_count, "Salida_cluster_" + suffix);
        entries.end();
        exits.end();
    }
}

void ClusterRCPPSolver::generate_constraints() {
    PathConstraintSetter path(_super_graph, _trucks, _env, _model);
    path.set_service_constraint(_X);
    path.set_continuity_constraint(_X, _Y, _YDK, _YKD);
    set_boundary_constraints();

    FlowConstraintSetter flow(_super_graph, _trucks, _env, _model);
    flow.set_deposit_flow_constraint(_X, _FDK);
    flow.set_flow_conservation_constraint(_X, _F, _FDK);
    flow.set_flow_bounds_constraint(_X, _Y, _F, _FDK, _YDK, _options.capacity);
}
