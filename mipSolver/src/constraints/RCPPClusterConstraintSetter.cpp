#include "../../lib/constraints/RCPPClusterConstraintSetter.h"

void RCPPClusterConstraintSetter::set_cluster_crossing_constraint(const VariableMatrix& Y, const vector<bool>& frontier_edge) {
    for (int p = 0; p < _truck_limit; p++) {
        IloExpr expre(_env);

        for (const SuperArc& arc: _super_graph.arcs()) {
            if (not frontier_edge[arc.edge_id]) continue;

            expre += Y[arc.id][p];
        }

    }
}
