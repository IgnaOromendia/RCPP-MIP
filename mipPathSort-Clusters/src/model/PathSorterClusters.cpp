#include <model/PathSorterClusters.h>
#include <heuristic/ClusterGuidedHierholzer.h>
#include <heuristic/WarmStart.h>
#include <utility>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>

PathSorterCluster::PathSorterCluster(): PathSolver() {}

PathSorterCluster::PathSorterCluster(ClusterPathSortInstance instance, int hierholzer_lookahead, int hierholzer_branch_width):
    PathSolver(std::move(instance.path)), _cluster_count(instance.cluster_count),
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
}

void PathSorterCluster::generate_constraints() {
    OrderConstraintSetter order_constraint_setter(_Z, _instance.edges, _instance.deposit, _env, _model);
    order_constraint_setter.set_suc_pred_constraint(_segments);

    PositionConstraintSetter position_constraint_setter(_X, _Z, _K, _instance.edges, _segments, _env, _model);
    position_constraint_setter.set_deposit_constraint(_instance.deposit);
    position_constraint_setter.set_position_order_constraint(_instance.deposit);
    position_constraint_setter.set_passes_order_constraint(_segment_map);
}

void PathSorterCluster::set_objective() {
    IloExpr objective(_env);

    for (int r = 0; r < _K; r++) {
        const int cluster_r = _edge_clusters[_segments[r].first];

        for (int s = 0; s < _K; s++) {
            if (not is_transition(r, s)) continue;

            const int cluster_s = _edge_clusters[_segments[s].first];

            if (cluster_r != cluster_s)
                objective += _Z[r][s];
        }
    }

    add_minimization_objective(objective);
    objective.end();
}

void PathSorterCluster::generate_warm_start() {
    WarmStart(*this).generate();
}
