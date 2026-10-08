#include <model/PathSorterFirstPass.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <constraints/ModuleConstraintSetter.h>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>
#include <stdexcept>

PathSorterFirstPass::PathSorterFirstPass(): PathSorterFirstPass(PathSortInstance{}) {}

PathSorterFirstPass::PathSorterFirstPass(PathSortInstance instance): _instance(std::move(instance)) {
    build_segments();
    calculate_deposit_distances();
}

PathSorterFirstPass::~PathSorterFirstPass() = default;

const std::vector<PathEdge>& PathSorterFirstPass::edges() const noexcept {
    return _instance.edges;
}

int PathSorterFirstPass::total_passes() const noexcept {
    return _K;
}

CPLEXSolveResult PathSorterFirstPass::solve(double gapTolerance) {
    _has_solution = false;
    const CPLEXSolveResult result = CPLEXSolver::solve(gapTolerance);
    _has_solution = result.has_solution;
    return result;
}

double PathSorterFirstPass::minimum_distance() const {
    if (!_has_solution)
        throw std::logic_error("No hay una solucion disponible para consultar la distancia minima.");
    return get_objective_value();
}

std::vector<OrderedPass> PathSorterFirstPass::extract_order() const {
    if (!_has_solution)
        throw std::logic_error("No hay una solucion disponible para exportar el orden.");

    std::vector<OrderedPass> result(static_cast<std::size_t>(_K));
    std::vector<bool> occupied(static_cast<std::size_t>(_K), false);
    for (int r = 0; r < _K; ++r) {
        const double raw_position = get_value(_X[r]);
        const long long position = std::llround(raw_position);
        if (std::abs(raw_position - static_cast<double>(position)) > 1e-5 ||
            position < 1 || position > _K)
            throw std::logic_error("La solucion contiene una posicion invalida.");
        if (occupied[static_cast<std::size_t>(position - 1)])
            throw std::logic_error("La solucion asigna mas de una pasada a la misma posicion.");

        const auto [edge, pass] = _segments[r];
        occupied[static_cast<std::size_t>(position - 1)] = true;
        result[static_cast<std::size_t>(position - 1)] = {
            static_cast<int>(position), pass + 1, _instance.edges[edge]
        };
    }

    if (std::find(occupied.begin(), occupied.end(), false) != occupied.end())
        throw std::logic_error("La solucion no asigna una pasada a cada posicion.");

    return result;
}

void PathSorterFirstPass::generate_variables() {
    const int edge_count = _instance.edges.size();
    const int segment_count = _segments.size();

    cout << "|E| = " << edge_count << " K = " << _K << "\n";

    _X = create_variable_array(_K, 1, _K, ILOINT);
    _D = create_arc_variable(_K);
    _Z = create_arc_variable(_K);

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

    set_module_objective();
}

void PathSorterFirstPass::invalidate_result() {
    _has_solution = false;
}

void PathSorterFirstPass::set_module_objective() {
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
    set_objective(objective);
    objective.end();
}

void PathSorterFirstPass::set_position_variable(int e, int k) {
    const PathEdge& edge = _instance.edges[e];
    const int r = _segment_map.at({e, k});
    const string name = "X_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" +
        to_string(k + 1);
    set_variable_name(_X[r], name);
}

void PathSorterFirstPass::set_distance_variable(int r, int s) {
    const string name = "D_" + to_string(r) + "_" + to_string(s);
    set_variable_name(_D[r][s], name);
}

void PathSorterFirstPass::set_order_variable(ArcVariables& V, const string& variable_name, int r, int s) {
    const PathEdge& edge = _instance.edges[_segments[r].first];
    const string name = variable_name + "_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" + to_string(s);
    set_variable_name(V[r][s], name);
}

void PathSorterFirstPass::build_segments() {
    for (int e = 0; e < static_cast<int>(_instance.edges.size()); ++e) {
        const PathEdge& edge = _instance.edges[e];
        const int times = edge.times();
        for (int k = 0; k < times; ++k) {
            _segment_map[{e, k}] = _segments.size();
            _segments.emplace_back(e, k);
            ++_K;
        }
    }
    if (_segments.empty()) throw std::invalid_argument("No hay pasadas para ordenar");
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

bool PathSorterFirstPass::is_transition(int r, int s) const {
    const PathEdge& from = _instance.edges[_segments[r].first];
    const PathEdge& to = _instance.edges[_segments[s].first];
    const bool arrival = from.original_edge_id == -2 &&
                         from.to == _instance.deposit;
    const bool departure = to.original_edge_id == -2 &&
                           to.from == _instance.deposit;
    return from.to == to.from && !arrival && !departure;
}
