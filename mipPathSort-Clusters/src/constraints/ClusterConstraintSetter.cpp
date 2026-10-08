#include <constraints/ClusterConstraintSetter.h>

void ClusterConstraintSetter::set_cluster_constraint(const map<segment, int> &segment_map, const map<int, vector<int>> &edges_by_cluster) {
    for (int c = 0; c < _cluster_count; c++) {
        for (int d = c + 1; d < _cluster_count; d++) {
            const vector<int>& c_edges = edges_by_cluster.at(c);
            const vector<int>& d_edges = edges_by_cluster.at(d);

            for (int f: d_edges) {
                const int rf = segment_map.at({f, 0});

                for (int a: c_edges) {
                    const int ra = segment_map.at({a, 0});

                    IloExpr expre(_env);
                    expre = _K * (_Q[c][f] + 1 - _O[c][d]) - _X[ra] + _X[rf];
                    add_constraint(0, expre, IloInfinity, "Cluster_2_" + std::to_string(c) + "_" + std::to_string(d) + "_" + std::to_string(a) + "_" + std::to_string(f));
                    expre.end();
                }
            }

            for (int e: c_edges) {
                const int re = segment_map.at({e, 0});

                for (int b: d_edges) {
                    const int rb = segment_map.at({b, 0});

                    IloExpr expre(_env);
                    expre = _K * (_Q[d][e] + _O[c][d]) - _X[rb] + _X[re];
                    add_constraint(0, expre, IloInfinity, "Cluster_3_" + std::to_string(c) + "_" + std::to_string(d) + "_" + std::to_string(b) + "_" + std::to_string(e));
                    expre.end();
                }
            }
        }
    }
}
