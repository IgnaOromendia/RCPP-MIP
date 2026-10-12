#include <model/PathSorterClusters.h>
#include <heuristic/ClusterGuidedHierholzer.h>
#include <heuristic/WarmStart.h>
#include <utility>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>
#include <constraints/ClusterConstraintSetter.h>

PathSorterCluster::PathSorterCluster(): PathSolver() {}

PathSorterCluster::PathSorterCluster(ClusterPathSortInstance instance, int hierholzer_lookahead, int hierholzer_branch_width):
    PathSolver(std::move(instance.path)), _cluster_count(instance.cluster_count), _edges_by_cluster(std::move(instance.edges_by_cluster)),
    _edge_clusters(std::move(instance.edge_clusters)), _hierholzer_lookahead(hierholzer_lookahead) {
    _warm_start_order = ClusterGuidedHierholzer( _instance, _edge_clusters, _segment_map, _segments, hierholzer_lookahead, hierholzer_branch_width).build();
}

PathSorterCluster::~PathSorterCluster() = default;

int PathSorterCluster::cluster_count() const noexcept {
    return _cluster_count;
}

const vector<int>& PathSorterCluster::edge_clusters() const noexcept {
    return _edge_clusters;
}

vector<OrderedPass> PathSorterCluster::hierholzer_order() const {
    vector<OrderedPass> result;
    result.reserve(_warm_start_order.size());
    int position = 1;
    for (int r : _warm_start_order) {
        const auto [edge, pass] = _segments[r];
        result.push_back({position, pass + 1, _instance.edges[edge], edge});
        ++position;
    }
    return result;
}

bool PathSorterCluster::complete_hierholzer_warm_start() {
    return WarmStart(*this).complete_hierholzer();
}

void PathSorterCluster::generate_variables() {
    const int edge_count = _instance.edges.size();
    const int segment_count = _segments.size();

    cout << "|E| = " << edge_count << " K = " << _K << " C = " << _cluster_count << "\n";

    _X = create_variable_array(_K, 1, _K, ILOINT);
    _Z = create_varaible_matrix(_K);
    _O = create_varaible_matrix(_cluster_count);
    _Q = create_varaible_matrix(_cluster_count);
    _L = create_variable_array(_cluster_count, 1, _K, ILOFLOAT);

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

        set_cluster_max_position_variable(c);

        for (int e = 0; e < edge_count; ++e)
            set_cluster_segment_variable(c, e);

        for (int d = 0; d < _cluster_count; d++)
            set_cluster_order_variable(c, d);

    }
}

void PathSorterCluster::generate_constraints() {
    ClusterConstraintSetter cluster_constraint_setter(
        _O, _Q, _X, _L, _edges_by_cluster, _segment_map,
        _cluster_count, _K, _env, _model);
    cluster_constraint_setter.set_cluster_max_position_constraint();
    cluster_constraint_setter.set_cluster_constraint();

    OrderConstraintSetter order_constraint_setter(_Z, _instance.edges, _instance.deposit, _env, _model);
    order_constraint_setter.set_suc_pred_constraint(_segments);

    PositionConstraintSetter position_constraint_setter(_X, _Z, _K, _instance.edges, _segments, _env, _model);
    position_constraint_setter.set_deposit_constraint(_instance.deposit);
    position_constraint_setter.set_position_order_constraint(_instance.deposit);
    position_constraint_setter.set_passes_order_constraint(_segment_map);
}

void PathSorterCluster::set_objective() {
    IloExpr objective(_env);

    for (const auto& [c, c_edges] : _edges_by_cluster) {
        const double c_size = c_edges.size();
        for (const auto& [d, d_edges] : _edges_by_cluster) {
            if (c >= d) continue;

            const double d_size =  + d_edges.size();

            for (int f : d_edges)
                objective += c_size * _Q[c][f];

            for (int e : c_edges)
                objective += d_size * _Q[d][e];
        }
    }

    add_minimization_objective(objective);
    objective.end();
}

void PathSorterCluster::generate_warm_start() {
    WarmStart(*this).generate();
}

void PathSorterCluster::set_cluster_segment_variable(int c, int e) {
    const string name = "Q_" + to_string(c + 1) + "_" + to_string(e + 1);
    set_variable_name(_Q[c][e], name);
}

void PathSorterCluster::set_cluster_max_position_variable(int c) {
    const string name = "L_" + to_string(c + 1);
    set_variable_name(_L[c], name);
}

void PathSorterCluster::set_cluster_order_variable(int c, int d) {
    const string name = "O_" + to_string(c + 1) + "_" + to_string(d + 1);
    set_variable_name(_O[c][d], name);
}
