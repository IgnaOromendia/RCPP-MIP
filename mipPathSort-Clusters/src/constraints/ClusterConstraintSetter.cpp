#include <constraints/ClusterConstraintSetter.h>

void ClusterConstraintSetter::set_cluster_constraint() {
    int constraints_added = 0;
    for (int c = 0; c < _cluster_count; c++) {
        if (_edges_by_cluster.count(c) == 0) continue;
        const vector<int>& c_edges = _edges_by_cluster.at(c);

        for (int d = c + 1; d < _cluster_count; d++) {
            if (_edges_by_cluster.count(d) == 0) continue;
            const vector<int>& d_edges = _edges_by_cluster.at(d);

            for (int f: d_edges) {
                const int rf = _segment_map.at({f, 0});

                IloExpr expre(_env);
                expre = _K * _Q[c][f] - _K * _O[c][d] - _L[c] + _X[rf];
                constraints_added += add_constraint(
                    -_K, expre, IloInfinity,
                    "Cluster_3_" + std::to_string(c) + "_" +
                    std::to_string(d) + "_" + std::to_string(f));
                expre.end();
            }

            for (int e: c_edges) {
                const int re = _segment_map.at({e, 0});

                IloExpr expre(_env);
                expre = _K * _Q[d][e] + _K * _O[c][d] - _L[d] + _X[re];
                constraints_added += add_constraint(
                    0, expre, IloInfinity,
                    "Cluster_4_" + std::to_string(c) + "_" +
                    std::to_string(d) + "_" + std::to_string(e));
                expre.end();
            }
        }
    }
    std::cout << "set_cluster_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void ClusterConstraintSetter::set_cluster_max_position_constraint() {
    int constraints_added = 0;

    for (int c = 0; c < _cluster_count; c++) {
        if (_edges_by_cluster.count(c) == 0) continue;
        const vector<int>& edges = _edges_by_cluster.at(c);

        for(int e: edges) {
            IloExpr expre(_env);
            expre = _L[c] - _X[_segment_map.at({e, 0})];
            constraints_added += add_constraint(
                0, expre, IloInfinity,
                "Cluster_2_" + to_string(c) + "_" + to_string(e));
            expre.end();
        }

    }
    std::cout << "set_cluster_max_position_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
