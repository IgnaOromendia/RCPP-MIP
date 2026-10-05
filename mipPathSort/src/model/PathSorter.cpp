#include <model/PathSorter.h>
#include <utility>
#include <constraints/ModuleConstraintSetter.h>
#include <constraints/OrderConstraintSetter.h>

PathSorter::PathSorter(): PathSorter(PathSortInstance{}) {}

PathSorter::PathSorter(PathSortInstance instance): _instance(std::move(instance)) {
    _pass_count.reserve(_instance.edges.size());
    for (const PathEdge& edge : _instance.edges) {
        const int m_e = edge.times();
        _pass_count.push_back(m_e);
        _K += m_e;
    }
}

PathSorter::~PathSorter() = default;

const std::vector<PathEdge>& PathSorter::edges() const noexcept {
    return _instance.edges;
}

const std::vector<int>& PathSorter::pass_counts() const noexcept {
    return _pass_count;
}

int PathSorter::total_passes() const noexcept {
    return _K;
}

void PathSorter::generate_variables() {
    const int edge_count = _instance.edges.size();

    _X = create_arc_variable(edge_count);
    _D = create_arc_variable(edge_count);
    _Z = create_array_arc_variables(edge_count);

    for (int e = 0; e < edge_count; ++e) {
        const int m_e = _pass_count[e];

        _X[e] = create_variable_array(m_e, 1, _K, ILOINT);
        _D[e] = create_variable_array(edge_count, 0, _K, ILOINT);
        _Z[e] = create_arc_variable(m_e);

        for (int f = 0; f < edge_count; ++f)
            set_distance_variable(e, f);

        for (int k = 0; k < m_e; ++k) {
            set_pass_variable(_X, e, "X", k);

            _Z[e][k] = create_variable_array(_K, 0, 1, ILOINT);

            for (int t = 0; t < _K; ++t)
                set_position_variable(e, k, t);
        }
    }
}

void PathSorter::generate_constraints() {
    ModuleConstraintSetter module_constraint_setter(_instance.edges, _env, _model);
    module_constraint_setter.set_module_constraints(_D, _X);

    OrderConstraintSetter order_constraint_setter(_Z, _instance.edges, _pass_count, _K, _env, _model);
    order_constraint_setter.set_pass_over_position_constraint();
    order_constraint_setter.set_position_over_pass_constraint();
    order_constraint_setter.set_continuity_constraint(_instance.adj.size());
    order_constraint_setter.set_circuit_constraint(_instance.deposit);
    order_constraint_setter.set_deposit_constraint(_instance.deposit);
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

void PathSorter::set_pass_variable(ArcVariables& variables, std::size_t edge_index, const string& variable_name, int pass) {
    const PathEdge& edge = _instance.edges[edge_index];
    const string name = variable_name + "_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" +
        to_string(pass + 1);
    set_variable_name(variables[edge_index][pass], name);
}

void PathSorter::set_distance_variable(std::size_t from_edge, std::size_t to_edge) {
    const string name = "D_" + to_string(from_edge + 1) + "_" + to_string(to_edge + 1);
    set_variable_name(_D[from_edge][to_edge], name);
}

void PathSorter::set_position_variable(std::size_t edge_index, int pass, int position) {
    const PathEdge& edge = _instance.edges[edge_index];
    const string name = "Z_" + to_string(edge.from + 1) + "_" +
        to_string(edge.to + 1) + "_" + to_string(edge.vehicle) + "_" +
        to_string(pass + 1) + "_" + to_string(position + 1);
    set_variable_name(_Z[edge_index][pass][position], name);
}
