#include <model/PathSorter.h>
#include <utility>
#include <constraints/FirstPassConstraintSetter.h>
#include <constraints/ModuleConstraintSetter.h>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>
#include <queue>
#include <stdexcept>

PathSorter::PathSorter(): PathSorter(PathSortInstance{}) {}

PathSorter::PathSorter(PathSortInstance instance): _instance(std::move(instance)) {
    for (const PathEdge& edge : _instance.edges) {
        const int m_e = edge.times();
        _K += m_e;
    }
    calculate_deposit_distances();
}

PathSorter::~PathSorter() = default;

const std::vector<PathEdge>& PathSorter::edges() const noexcept {
    return _instance.edges;
}

int PathSorter::total_passes() const noexcept {
    return _K;
}

CPLEXSolveResult PathSorter::solve(double gapTolerance) {
    _has_solution = false;
    const CPLEXSolveResult result = CPLEXSolver::solve(gapTolerance);
    _has_solution = result.has_solution;
    return result;
}

double PathSorter::minimum_distance() const {
    if (!_has_solution)
        throw std::logic_error("No hay una solucion disponible para consultar la distancia minima.");
    return get_objective_value();
}

std::vector<OrderedPass> PathSorter::extract_order() const {
    if (!_has_solution)
        throw std::logic_error("No hay una solucion disponible para exportar el orden.");

    std::vector<OrderedPass> result;
    result.reserve(static_cast<std::size_t>(_K));
    std::vector<int> appearances(_instance.edges.size(), 0);
    for (int position = 0; position < _K; ++position) {
        int selected_edge = -1;
        for (std::size_t edge = 0; edge < _instance.edges.size(); ++edge) {
            if (get_value(_Z[edge][position]) <= 0.5) continue;
            if (selected_edge != -1)
                throw std::logic_error("La solucion asigna mas de una arista a la misma posicion.");
            selected_edge = static_cast<int>(edge);
        }

        if (selected_edge == -1)
            throw std::logic_error("La solucion no asigna una arista a cada posicion.");

        const std::size_t edge = static_cast<std::size_t>(selected_edge);
        ++appearances[edge];
        result.push_back({position + 1, appearances[edge], _instance.edges[edge]});
    }

    for (std::size_t edge = 0; edge < appearances.size(); ++edge)
        if (appearances[edge] != _instance.edges[edge].times())
            throw std::logic_error("La solucion no respeta la cantidad de pasadas de una arista.");

    return result;
}

void PathSorter::generate_variables() {
    const int edge_count = _instance.edges.size();

    cout << "|E| = " << edge_count << " K = " << _K << "\n";

    _X = create_variable_array(edge_count, 1, _K, ILOINT);
    _A = create_arc_variable(edge_count);
    _D = create_arc_variable(edge_count);
    _Z = create_arc_variable(edge_count);

    for (int e = 0; e < edge_count; ++e) {
        const int times = _instance.edges[e].times();

        set_first_pass_variable(e);

        if(times > 1) _A[e] = create_variable_array(_K, 0, 1, ILOBOOL);
        _D[e] = create_variable_array(edge_count, 0, _K, ILOINT);
        _Z[e] = create_variable_array(_K, 0, 1, ILOBOOL);

        for (int f = 0; f < edge_count; ++f)
            if (_instance.edges[e].to == _instance.edges[f].from)
                set_distance_variable(e, f);

        for (int t = 0; t < _K; ++t) {
            if (times > 1) set_position_variable(_A, "A", e, t);
            set_position_variable(_Z, "Z", e, t);
        }
    }
}

void PathSorter::generate_constraints() {
    ModuleConstraintSetter module_constraint_setter(_instance.edges, _env, _model);
    module_constraint_setter.set_module_constraints(_D, _X);

    OrderConstraintSetter order_constraint_setter(_Z, _instance.edges, _depo_dist, _K, _env, _model);
    order_constraint_setter.set_passes_amount_constraint();
    order_constraint_setter.set_position_over_pass_constraint();
    order_constraint_setter.set_continuity_constraint(_instance.adj.size());
    order_constraint_setter.set_depo_return_constraint(_instance.deposit);
    order_constraint_setter.set_depo_arrival_constraint(_instance.deposit);

    FirstPassConstraintSetter first_pass_constraint_setter(_A, _instance.edges, _K, _env, _model);
    first_pass_constraint_setter.set_seen_continuity_constraint();
    first_pass_constraint_setter.set_seen_presence_constraint(_Z);
    first_pass_constraint_setter.set_seen_activation_constraint(_Z);

    PositionConstraintSetter position_constraint_setter(_X, _instance.edges, _K, _env, _model);
    position_constraint_setter.set_position_constraint(_A, _Z);

    set_module_objective();
}

void PathSorter::invalidate_result() {
    _has_solution = false;
}

void PathSorter::set_module_objective() {
    IloExpr objective(_env);
    for (std::size_t e = 0; e < _instance.edges.size(); ++e) {
        for (std::size_t f = 0; f < _instance.edges.size(); ++f) {
            if (_instance.edges[e].vehicle == _instance.edges[f].vehicle &&
                _instance.edges[e].to == _instance.edges[f].from)
                objective += _D[e][f];
        }
    }
    set_objective(objective);
    objective.end();
}

void PathSorter::set_first_pass_variable(std::size_t edge_index) {
    const PathEdge& edge = _instance.edges[edge_index];
    const string name = "X_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle);
    set_variable_name(_X[edge_index], name);
}

void PathSorter::set_distance_variable(std::size_t from_edge, std::size_t to_edge) {
    const string name = "D_" + to_string(from_edge + 1) + "_" + to_string(to_edge + 1);
    set_variable_name(_D[from_edge][to_edge], name);
}

void PathSorter::set_position_variable(ArcVariables& variables,
                                       const string& variable_name,
                                       std::size_t edge_index, int position) {
    const PathEdge& edge = _instance.edges[edge_index];
    const string name = variable_name + "_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" + to_string(position + 1);
    set_variable_name(variables[edge_index][position], name);
}

void PathSorter::calculate_deposit_distances() {
    const int node_count = _instance.adj.size();
    _depo_dist.assign(node_count, -1);

    std::queue<int> pending;
    _depo_dist[_instance.deposit] = 0;
    pending.push(_instance.deposit);

    while (!pending.empty()) {
        const int from = pending.front();
        pending.pop();

        for (const auto& [to, _] : _instance.adj[from]) {
            if (_depo_dist[to] != -1) continue;

            _depo_dist[to] = _depo_dist[from] + 1;
            pending.push(to);
        }
    }
}
