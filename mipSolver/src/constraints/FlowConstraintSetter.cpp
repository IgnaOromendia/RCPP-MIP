#include "../../lib/constraints/FlowConstraintSetter.h"

void FlowConstraintSetter::set_deposit_flow_constraint(const NumVarMatrix3& X, const NumVarMatrix& FDK) {
	for(int p = 1; p < _truck_limit; p++) {
		IloExpr expre(_env);
		string name = "Flujo_D_" + to_string(p);

		for (const SuperArc* arc: _super_graph.super_arcs_adj_depo_node()) 
			expre += FDK[arc->from][p];

		for (const SuperArc& arc: _super_graph.arcs()) {				
			if (not arc.requested) continue;
			expre -= arc.demand * X[arc.from][arc.to][p];
		}

		add_constraint(0, expre, 0, name);
		expre.end();
	}
}

void FlowConstraintSetter::set_flow_conservation_constraint(const NumVarMatrix3& X, const NumVarMatrix3& F, const NumVarMatrix& FDK) {
	for (int p = 1; p < _truck_limit; p++) {
		for (int v = 0; v < _super_graph.nodes_amount(); v++) {
			IloExpr expre(_env);
			string name = "Flujo_" + to_string(v+1) + "_" + to_string(p);

			if (_super_graph.is_adj_depo_node(v))
				expre += FDK[v][p];
			
			for (const SuperArc* arc: _super_graph.super_arcs_for_node_in(v)) {
				if (arc->edge_id == -2) continue;
				expre -= F[v][arc->to][p];
			}

			for (const SuperArc* arc: _super_graph.super_arcs_for_node_out(v)) {
				if (arc->edge_id == -2) continue;
				expre += F[arc->from][v][p];
				if (arc->requested and (p == arc->zone or arc->zone == -1)) expre -= arc->demand * X[arc->from][v][p];
			}

			add_constraint(0, expre, 0, name);
			expre.end();
		}
	}
}

void FlowConstraintSetter::set_flow_bounds_constraint(const NumVarMatrix3& X, const NumVarMatrix3& Y, const NumVarMatrix3& F, const NumVarMatrix& FDK, const NumVarMatrix& YDK, double capacity) {
	for (const SuperArc& arc : _super_graph.arcs()) {
		if (arc.edge_id == -2) continue;
		for (int p = 1; p < _truck_limit; p++) {
			IloExpr expre(_env);
			string name = "CotaF_" + to_string(arc.from+1) + "_" + to_string(arc.to+1) + "_" + to_string(p);

			expre += F[arc.from][arc.to][p] - capacity * Y[arc.from][arc.to][p];

			if (arc.requested and (arc.zone == p or arc.zone == -1)) 
				expre -= capacity * X[arc.from][arc.to][p];

			add_constraint(-IloInfinity, expre, 0, name);
			expre.end();
		}
	}

	for (int p = 1; p < _truck_limit; p++) {
		for (const SuperArc* arc: _super_graph.super_arcs_adj_depo_node())  {
			IloExpr expre(_env);
			string name = "CotaF_D_" + to_string(arc->from+1) + "_" + to_string(p);

			expre += FDK[arc->from][p] - capacity * YDK[arc->from][p];

			add_constraint(-IloInfinity, expre, 0, name);
			expre.end();
		}
	}
}