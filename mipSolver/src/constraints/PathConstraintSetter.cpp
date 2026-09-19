#include "../../lib/constraints/PathConstraintSetter.h"

void PathConstraintSetter::set_service_constraint(const NumVarMatrix3& X) {
    for (const SuperArc& arc: _super_graph.arcs()) {
		if (arc.edge_id == -2) continue;

		// la segunda condición prohibe agregar la restricción 2 veces
		if (arc.requested and arc.pair < arc.id) {
			IloExpr expre(_env);
			string name = "Servicio_" + to_string(arc.from+1) + "_" + to_string(arc.to+1);

			int p = arc.zone > 0 ? arc.zone : 1;
			int limit = arc.zone > 0 ? arc.zone + 1 : _truck_limit;

			for(; p < limit; p++) {
				expre += X[arc.from][arc.to][p];

				// Tiene pareja <--> es arista
				if (arc.pair != -1) {
					const SuperArc* pair = _super_graph.super_arc_with_id(arc.pair);
					expre += X[pair->from][pair->to][p];
				}
					
			}
			
			add_constraint(1, expre, 1, name);
            
			expre.end();
		}
	}

}

void PathConstraintSetter::set_continuity_constraint(const NumVarMatrix3& X, const NumVarMatrix3& Y, const NumVarMatrix& YDK, const NumVarMatrix& YKD) {
    for (int p = 1; p < _truck_limit; p++) {
		for (int v = 0; v < _super_graph.nodes_amount(); v++) {
			IloExpr expre(_env);
			string name = "Cont_" + to_string(v+1) + "_" + to_string(p);

			if (_super_graph.is_adj_depo_node(v)) expre -= YDK[v][p];
			if (_super_graph.is_adj_node_depo(v)) expre += YKD[v][p];
			
			for (const SuperArc* arc: _super_graph.super_arcs_for_node_in(v)) {		
				if (arc->edge_id == -2) continue;	
				expre += Y[v][arc->to][p];

				if (arc->requested and (p == arc->zone or arc->zone == -1)) 
					expre += X[v][arc->to][p];
			}

			for (const SuperArc* arc: _super_graph.super_arcs_for_node_out(v)) {
				if (arc->edge_id == -2) continue;
				expre -= Y[arc->from][v][p];

				if (arc->requested and (p == arc->zone or arc->zone == -1)) 
					expre -= X[arc->from][v][p];
			}

			add_constraint(0, expre, 0, name);
			expre.end();
		}
	}
}

void PathConstraintSetter::set_depoist_arrival_constraint(const NumVarMatrix& YKD) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Node_depo_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_node_depo()) 
			expre += YKD[arc->to][p];
		
			
		
		add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}

void PathConstraintSetter::set_depoist_departure_constraint(const NumVarMatrix& YDK) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Depo_node_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_depo_node()) 
			expre += YDK[arc->from][p];
		
		add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}