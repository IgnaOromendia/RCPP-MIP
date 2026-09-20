#include "../../lib/constraints/PathConstraintSetter.h"

void PathConstraintSetter::set_service_constraint(const ArcVariables& X) {
    for (const SuperArc& arc: _super_graph.arcs()) {
		if (arc.edge_id == -2) continue;

		// la segunda condición prohibe agregar la restricción 2 veces
		if (arc.requested and arc.pair < arc.id) {
			IloExpr expre(_env);
			string name = "Servicio_" + to_string(arc.from+1) + "_" + to_string(arc.to+1);

			int p = arc.zone > 0 ? arc.zone : 1;
			int limit = arc.zone > 0 ? arc.zone + 1 : _truck_limit;

			for(; p < limit; p++) {
				expre += X[arc.id][p];

				// Tiene pareja <--> es arista
				if (arc.pair != -1) {
					const SuperArc* pair = _super_graph.super_arc_with_id(arc.pair);
					expre += X[pair->id][p];
				}
					
			}
			
			add_constraint(1, expre, 1, name);
            
			expre.end();
		}
	}

}

void PathConstraintSetter::set_continuity_constraint(const ArcVariables& X, const ArcVariables& Y, const ArcVariables& YDK, const ArcVariables& YKD) {
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

void PathConstraintSetter::set_deposit_arrival_constraint(const ArcVariables& YKD) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Node_depo_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_node_depo()) 
			expre += YKD[arc->id][p];
		
			
		
		add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}

void PathConstraintSetter::set_deposit_departure_constraint(const ArcVariables& YDK) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Depo_node_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_depo_node()) 
			expre += YDK[arc->id][p];
		
		add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}