#include <model/PathSolver.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

PathSolver::PathSolver() = default;

PathSolver::PathSolver(PathSortInstance instance): _instance(std::move(instance)) {
    build_segments();
}

PathSolver::~PathSolver() = default;

const PathSortInstance& PathSolver::instance() const noexcept {
    return _instance;
}

const std::vector<PathEdge>& PathSolver::edges() const noexcept {
    return _instance.edges;
}

int PathSolver::total_passes() const noexcept {
    return _K;
}

CPLEXSolveResult PathSolver::solve(double gapTolerance) {
    _has_solution = false;
    const CPLEXSolveResult result = CPLEXSolver::solve(gapTolerance);
    _has_solution = result.has_solution;
    return result;
}

std::vector<OrderedPass> PathSolver::extract_order() const {
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

void PathSolver::invalidate_result() {
    _has_solution = false;
}

void PathSolver::set_position_variable(int e, int k) {
    const PathEdge& edge = _instance.edges[e];
    const int r = _segment_map.at({e, k});
    const string name = "X_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" +
        to_string(k + 1);
    set_variable_name(_X[r], name);
}

void PathSolver::set_order_variable(VariableMatrix& V, const string& variable_name, int r, int s) {
    const PathEdge& edge = _instance.edges[_segments[r].first];
    const string name = variable_name + "_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" + to_string(s);
    set_variable_name(V[r][s], name);
}

void PathSolver::build_segments() {
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

bool PathSolver::is_transition(int r, int s) const {
    const PathEdge& from = _instance.edges[_segments[r].first];
    const PathEdge& to = _instance.edges[_segments[s].first];
    const bool arrival = from.original_edge_id == -2 &&
                         from.to == _instance.deposit;
    const bool departure = to.original_edge_id == -2 &&
                           to.from == _instance.deposit;
    return from.to == to.from && !arrival && !departure;
}
