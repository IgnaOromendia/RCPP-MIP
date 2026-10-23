#include <model/ClusterRCPPSolver.h>
#include <constraints/FlowConstraintSetter.h>
#include <constraints/PathConstraintSetter.h>
#include <stdexcept>

ClusterRCPPSolver::ClusterRCPPSolver(const SuperGraph& super_graph, const ClusterBoundary& boundary, ModelOptions options)
    : RCPPSolver(super_graph, static_cast<int>(boundary.optimized_visit_count()), options), _boundary(boundary) {}

void ClusterRCPPSolver::set_boundary_constraints() {
    // Cada vehiculo local logico es una visita del circuito global.
    // Fijar ambos extremos del vehiculo conserva la pareja entrada/salida.
    int vehicle = 0;
    for (const ClusterVisit& visit : _boundary.visits) {
        if (visit.is_passthrough()) continue;
        ++vehicle;

        IloExpr all_entries(_env), selected_entries(_env);
        IloExpr all_exits(_env), selected_exits(_env);

        bool has_selected_entry = false;
        bool has_selected_exit = false;

        // Entry to node
        for (const SuperArc* arc : _super_graph.super_arcs_adj_depo_node()) {
            all_entries += _YDK[arc->id][vehicle];
            if (_super_graph.original_node(arc->to) == visit.entry.global_node) {
                selected_entries += _YDK[arc->id][vehicle];
                has_selected_entry = true;
            }
        }

        // Node to exit
        for (const SuperArc* arc : _super_graph.super_arcs_adj_node_depo()) {
            all_exits += _YKD[arc->id][vehicle];
            if (_super_graph.original_node(arc->from) ==
                    visit.exit.global_node) {
                selected_exits += _YKD[arc->id][vehicle];
                has_selected_exit = true;
            }
        }

        if (!has_selected_entry || !has_selected_exit)
            throw invalid_argument("Una visita no tiene conectores locales para sus extremos");

        const string suffix = to_string(visit.id) + "_" + to_string(vehicle);
        
        add_constraint(1, all_entries, 1, "Entrada_total_visita_" + suffix);
        add_constraint(1, selected_entries, 1, "Entrada_visita_" + suffix);

        add_constraint(1, all_exits, 1, "Salida_total_visita_" + suffix);
        add_constraint(1, selected_exits, 1, "Salida_visita_" + suffix);

        all_entries.end();
        selected_entries.end();

        all_exits.end();
        selected_exits.end();
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
