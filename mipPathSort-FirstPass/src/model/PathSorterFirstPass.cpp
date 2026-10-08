#include <model/PathSorterFirstPass.h>
#include <limits>
#include <queue>
#include <utility>
#include <constraints/ModuleConstraintSetter.h>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>
#include <stdexcept>

PathSorterFirstPass::PathSorterFirstPass(): PathSorterFirstPass(PathSortInstance{}) {}

PathSorterFirstPass::PathSorterFirstPass(PathSortInstance instance):
    PathSolver(std::move(instance)) {
    calculate_deposit_distances();
}

PathSorterFirstPass::~PathSorterFirstPass() = default;

double PathSorterFirstPass::minimum_distance() const {
    if (!_has_solution)
        throw std::logic_error("No hay una solucion disponible para consultar la distancia minima.");
    return get_objective_value();
}

void PathSorterFirstPass::generate_variables() {
    const int edge_count = _instance.edges.size();
    const int segment_count = _segments.size();

    cout << "|E| = " << edge_count << " K = " << _K << "\n";

    _X = create_variable_array(_K, 1, _K, ILOINT);
    _D = create_varaible_matrix(_K);
    _Z = create_varaible_matrix(_K);

    for (int r = 0; r < segment_count; ++r) {
        _D[r] = create_variable_array(_K, 0, _K, ILOINT);
        _Z[r] = create_variable_array(_K, 0, 0, ILOBOOL);

        const auto [e, k] = _segments[r];
        set_position_variable(e, k); // X

        for (int s = 0; s < segment_count; ++s) {
            if (is_transition(r, s)) {
                set_variable_bounds(_Z[r][s], 0, 1);
                set_order_variable(_Z, "Z", r, s);
            }
            set_distance_variable(r, s); // D
        }
    }
}

void PathSorterFirstPass::generate_constraints() {
    ModuleConstraintSetter module_constraint_setter(_instance.edges, _segments, _env, _model);
    module_constraint_setter.set_module_constraints(_D, _X);

    OrderConstraintSetter order_constraint_setter(_Z, _instance.edges, _segments, _instance.deposit, _env, _model);
    order_constraint_setter.set_suc_pred_constraint();

    PositionConstraintSetter position_constraint_setter(_X, _Z, _K, _instance.edges, _segments, _env, _model);
    position_constraint_setter.set_deposit_constraint(_instance.deposit);
    position_constraint_setter.set_position_order_constraint(_instance.deposit);
    position_constraint_setter.set_passes_order_constraint(_segment_map);
}

void PathSorterFirstPass::set_objective() {
    IloExpr objective(_env);
    for (int r = 0; r < _K; ++r) {
        const auto [e, k] = _segments[r];
        const int v = _instance.edges[e].from;
        if (k != 0) continue;
        for (int s = 0; s < _K; ++s) {
            const auto [f, kf] = _segments[s];
            if (kf == 0 and _instance.edges[e].to == _instance.edges[f].from)
                objective += _dist[v] * _D[r][s];
        }
    }
    add_minimization_objective(objective);
    objective.end();
}

void PathSorterFirstPass::set_distance_variable(int r, int s) {
    const string name = "D_" + to_string(r) + "_" + to_string(s);
    set_variable_name(_D[r][s], name);
}

void PathSorterFirstPass::calculate_deposit_distances() {
    _dist.assign(_instance.adj.size(), -1);
    if (_instance.deposit < 0 ||
        _instance.deposit >= static_cast<int>(_instance.adj.size()))
        return;

    std::queue<int> pending;
    _dist[_instance.deposit] = 0;
    pending.push(_instance.deposit);

    while (!pending.empty()) {
        const int u = pending.front();
        pending.pop();

        for (const auto& adjacent : _instance.adj[u]) {
            const int v = adjacent.first;
            if (_dist[v] != -1) continue;
            _dist[v] = _dist[u] + 1;
            pending.push(v);
        }
    }
}
