#include <model/PathSorterClusters.h>
#include <stdexcept>
#include <utility>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>
#include <constraints/ClusterConstraintSetter.h>

PathSorterCluster::PathSorterCluster(): PathSolver() {}

PathSorterCluster::PathSorterCluster(ClusterPathSortInstance instance):
    PathSolver(std::move(instance.path)), _cluster_count(instance.cluster_count),
    _edges_by_cluster(std::move(instance.edges_by_cluster)),
    _edge_clusters(std::move(instance.edge_clusters)) {
    if (_cluster_count <= 0)
        throw std::invalid_argument("La cantidad de clusters debe ser positiva");
    if (_edge_clusters.size() != _instance.edges.size())
        throw std::invalid_argument(
            "La cantidad de clusters no coincide con la cantidad de aristas");
    for (int cluster : _edge_clusters)
        if (cluster < 0 || cluster >= _cluster_count)
            throw std::invalid_argument(
                "Cada arista debe tener un cluster dentro de la cantidad declarada");
}

PathSorterCluster::~PathSorterCluster() = default;

int PathSorterCluster::cluster_count() const noexcept {
    return _cluster_count;
}

const std::vector<int>& PathSorterCluster::edge_clusters() const noexcept {
    return _edge_clusters;
}

void PathSorterCluster::generate_variables() {
    const int edge_count = _instance.edges.size();
    const int segment_count = _segments.size();

    cout << "|E| = " << edge_count << " K = " << _K << " C = " << _cluster_count << "\n";

    _X = create_variable_array(_K, 1, _K, ILOINT);
    _Z = create_varaible_matrix(_K);
    _O = create_varaible_matrix(_cluster_count);
    _Q = create_varaible_matrix(_cluster_count);

    for (int r = 0; r < segment_count; ++r) {
        _Z[r] = create_variable_array(_K, 0, 0, ILOBOOL);

        const auto [e, k] = _segments[r];
        set_position_variable(e, k); // X

        for (int s = 0; s < segment_count; ++s) {
            if (is_transition(r, s)) {
                set_variable_bounds(_Z[r][s], 0, 1);
                set_order_variable(_Z, "Z", r, s);
            }
        }
    }

    for(int c = 0; c < _cluster_count; c++) {
        _O[c] = create_variable_array(_cluster_count, 0, 1, ILOBOOL);
        _Q[c] = create_variable_array(edge_count, 0, 1, ILOBOOL);

        for (int e = 0; e < edge_count; ++e)
            set_cluster_segment_variable(c, e);

        for (int d = 0; d < _cluster_count; d++)
            set_cluster_order_variable(c, d);

    }
}

void PathSorterCluster::generate_constraints() {
    ClusterConstraintSetter cluster_constraint_setter(_O, _Q, _X, _cluster_count, _K, _env, _model);
    cluster_constraint_setter.set_cluster_constraint(_segment_map, _edges_by_cluster);

    OrderConstraintSetter order_constraint_setter(_Z, _instance.edges, _segments, _instance.deposit, _env, _model);
    order_constraint_setter.set_suc_pred_constraint();

    PositionConstraintSetter position_constraint_setter(_X, _Z, _K, _instance.edges, _segments, _env, _model);
    position_constraint_setter.set_deposit_constraint(_instance.deposit);
    position_constraint_setter.set_position_order_constraint(_instance.deposit);
    position_constraint_setter.set_passes_order_constraint(_segment_map);
}

void PathSorterCluster::set_objective() {
    IloExpr objective(_env);

    for (const auto& [c, c_edges] : _edges_by_cluster) {
        for (const auto& [d, d_edges] : _edges_by_cluster) {
            if (c >= d) continue;

            const double cluster_sizes = c_edges.size() + d_edges.size();
            const double pair_weight = 1.0 / cluster_sizes;

            for (int f : d_edges)
                objective += pair_weight * _Q[c][f];
                
            for (int e : c_edges)
                objective += pair_weight * _Q[d][e];
        }
    }

    add_minimization_objective(objective);
    objective.end();
}

void PathSorterCluster::set_cluster_segment_variable(int c, int e) {
    const string name = "Q_" + to_string(c + 1) + "_" + to_string(e + 1);
    set_variable_name(_Q[c][e], name);
}

void PathSorterCluster::set_cluster_order_variable(int c, int d) {
    const string name = "O_" + to_string(c + 1) + "_" + to_string(d + 1);
    set_variable_name(_O[c][d], name);
}
