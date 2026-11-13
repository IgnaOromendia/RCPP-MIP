#include "../../lib/constraints/PathConstraintSetter.h"

void PathConstraintSetter::set_service_constraint(const VariableMatrix& X) {
    std::map<int, std::vector<const SuperArc*>> orientations_by_service;
    for (const SuperArc& arc : _super_graph.arcs())
        if (arc.requested) orientations_by_service[arc.requested_idx].push_back(&arc);

    for (const auto& group : orientations_by_service) {
        const auto& orientations = group.second;
        IloExpr expression(_env);
        for (const SuperArc* arc : orientations) {
            const int first = arc->zone > 0 ? arc->zone : 1;
            const int limit = arc->zone > 0 ? arc->zone + 1 : _truck_limit;
            for (int vehicle = first; vehicle < limit; ++vehicle)
                expression += X[arc->id][vehicle];
        }
        const SuperArc& representative = *orientations.back();
        add_constraint(1, expression, 1, "Servicio_" + to_string(representative.from + 1) +
                       "_" + to_string(representative.to + 1));
        expression.end();
    }
}

void PathConstraintSetter::set_continuity_constraint(const VariableMatrix& X, const VariableMatrix& Y, const VariableMatrix& YDK, const VariableMatrix& YKD) {
    for (int p = 1; p < _truck_limit; p++) {
		for (int v = 0; v < _super_graph.nodes_amount(); v++) {
			IloExpr expre(_env);
			string name = "Cont_" + to_string(v+1) + "_" + to_string(p);
		
			for (const SuperArc* arc: _super_graph.super_arcs_from(v)) {		
				if (arc->edge_id == -2) {
					expre += YKD[arc->id][p];
					continue;
				}

				expre += Y[arc->id][p];

				if (arc->requested and (p == arc->zone or arc->zone == -1)) 
					expre += X[arc->id][p];
			}

			for (const SuperArc* arc: _super_graph.super_arcs_to(v)) {
				if (arc->edge_id == -2) {
					expre -= YDK[arc->id][p];
					continue;
				}

				expre -= Y[arc->id][p];

				if (arc->requested and (p == arc->zone or arc->zone == -1)) 
					expre -= X[arc->id][p];
			}

			add_constraint(0, expre, 0, name);
			expre.end();
		}
	}
}

void PathConstraintSetter::set_deposit_arrival_constraint(const VariableMatrix& YKD) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Node_depo_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_node_depo()) 
			expre += YKD[arc->id][p];
		
			
		
		add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}

void PathConstraintSetter::set_deposit_departure_constraint(const VariableMatrix& YDK) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Depo_node_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_depo_node()) 
			expre += YDK[arc->id][p];
		
		add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}