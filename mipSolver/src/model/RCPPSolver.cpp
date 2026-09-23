#include "../../lib/model/RCPPSolver.h"
#include "../../lib/constraints/PathConstraintSetter.h"
#include "../../lib/constraints/FlowConstraintSetter.h"
#include <stdexcept>
#include <cmath>
#include <limits>
#include <utility>
#include <algorithm>
#include <exception>

RCPPSolver::RCPPSolver(const SuperGraph& super_graph, int vehicles, ModelOptions options)
	: CPLEXSolver(), _options(options), _super_graph(super_graph) {
    _options.validate();
    if (vehicles <= 0 || vehicles == std::numeric_limits<int>::max())
        throw std::invalid_argument("Cantidad de vehiculos fuera de rango");
    if (_super_graph.deposit() < 0) throw std::invalid_argument("Supergrafo sin construir");
    for (const auto& arc : _super_graph.arcs()) {
        if (arc.zone < -1 || arc.zone > vehicles)
            throw std::invalid_argument("Zona del supergrafo fuera de rango");
    }
    _trucks = vehicles + 1; // Vehicle zero remains unused by the formulation.
}

void RCPPSolver::generate_MIP() {
	_solve_result = {};
	// Variables
	generate_variables();

	// Constraints
	PathConstraintSetter path_constriant_setter(_super_graph, _trucks, _env, _model);
	path_constriant_setter.set_service_constraint(_X);
	path_constriant_setter.set_continuity_constraint(_X, _Y, _YDK, _YKD);
	path_constriant_setter.set_deposit_arrival_constraint(_YKD);
	path_constriant_setter.set_deposit_departure_constraint(_YDK);

	FlowConstraintSetter flow_constraint_setter(_super_graph, _trucks, _env, _model);
	flow_constraint_setter.set_deposit_flow_constraint(_X, _FDK);
	flow_constraint_setter.set_flow_conservation_constraint(_X, _F, _FDK);
	flow_constraint_setter.set_flow_bounds_constraint(_X, _Y, _F, _FDK, _YDK, _options.capacity);
}

// Variables
void RCPPSolver::set_arc_variable(ArcVariables& V, const SuperArc& arc, string var_name, int truck) {
	string name = var_name + "_" + to_string(arc.from + 1) + "_" + to_string(arc.to + 1) + "_" + to_string(truck);
	set_variable_name(V[arc.id][truck], name);
}

void RCPPSolver::set_variable_depo_in(ArcVariables& V, const SuperArc& arc, string var_name, int truck) {
	string name = var_name + "_D_" + to_string(arc.to + 1) + "_" + to_string(truck);
	set_variable_name(V[arc.id][truck], name);
}

void RCPPSolver::set_variable_depo_out(ArcVariables& V, const SuperArc& arc, string var_name, int truck) {
	string name = var_name + "_" + to_string(arc.from + 1) + "_D_" + to_string(truck);
	set_variable_name(V[arc.id][truck], name);
}

void RCPPSolver::generate_variables() {
	const int arc_amount = _super_graph.arcs_amount();

	// El eje ij fue satisfecho en el viaje p
	_X = create_arc_variable(arc_amount);

	// Cantidad de veces que el eje ij fue recorrido sin recoger en el viaje p
	_Y = create_arc_variable(arc_amount);

	// Flujo del eje ij en el viaje p
	_F = create_arc_variable(arc_amount);

	// Adyacentes al depósito
	_YDK = create_arc_variable(arc_amount);
	_FDK = create_arc_variable(arc_amount);
	_YKD = create_arc_variable(arc_amount);

	for (const SuperArc& arc: _super_graph.arcs()) {
		if (arc.edge_id == -2) {
			if (arc.from == _super_graph.deposit()) {
				_YDK[arc.id] = create_variable_array(_trucks, 0, 1, ILOINT);
				_FDK[arc.id] = create_variable_array(_trucks, 0, _options.capacity, ILOFLOAT);

				for(int p = 1; p < _trucks; p++) {
					set_variable_depo_in(_YDK, arc, "Y", p);
					set_variable_depo_in(_FDK, arc, "F", p);
				}

			} else {
				_YKD[arc.id] = create_variable_array(_trucks, 0, 1, ILOINT);
				for(int p = 1; p < _trucks; p++)
					set_variable_depo_out(_YKD, arc, "Y", p);
			}

			continue;
		}

		if (arc.requested) _X[arc.id] = create_variable_array(_trucks, 0, 1, ILOINT);
		_Y[arc.id] = create_variable_array(_trucks, 0, _options.max_traversals, ILOINT);
		_F[arc.id] = create_variable_array(_trucks, 0, _options.capacity, ILOFLOAT);
	

		for(int p = 1; p < _trucks; p++) {
			if (arc.requested) set_arc_variable(_X, arc, "X", p);
			set_arc_variable(_Y, arc, "Y", p);
			set_arc_variable(_F, arc, "F", p);	
		}

	}
}

// Objective function
void RCPPSolver::set_time_objective() {
	_solve_result = {};
	IloExpr obj = create_expression();

	for(int p = 1; p < _trucks; p++) {
		for (const SuperArc& arc: _super_graph.arcs()) {
			if (arc.edge_id == -2) continue;
			obj += arc.cost * _Y[arc.id][p];
			if (arc.requested and (arc.zone == p or arc.zone == -1)) 
				obj += arc.cost * _X[arc.id][p];
		}
	}

	set_objective(obj);
	obj.end();
}

// Solve and export
SolveResult RCPPSolver::solve(double gapTolerance) {
	_solve_result = {};
	const bool found_solution = solve_model(gapTolerance);
	const IloAlgorithm::Status status = get_status();
	SolveResult result;
	result.status = status;
	if (found_solution &&
		(status == IloAlgorithm::Feasible || status == IloAlgorithm::Optimal)) {
		result._solution = capture_solution();
		result.has_solution = true;
	}
	_solve_result = std::move(result);
	return _solve_result;
}

void RCPPSolver::fix_incumbent_3D_variables(const vector<ArcValue<long long>>& arcs, ArcVariables& V, const edgeKeySet& free_edges, std::vector<VariableBounds>& original_bounds) {
	for (const ArcValue<long long>& value: arcs) {
		const SuperArc& arc = *_super_graph.super_arc_with_id(value.id);
		bool is_free = free_edges.count(EdgeKey(arc.from, arc.to)) != 0;
		
		if (!is_free && arc.pair != -1) {
			const SuperArc& reverse = *_super_graph.super_arc_with_id(arc.pair);
			is_free = free_edges.count(EdgeKey(reverse.from, reverse.to)) != 0;
		}

		if (is_free) continue;
		fix_and_save_bounds(V[value.id][value.vehicle], value.value, original_bounds);
	}
}

void RCPPSolver::fix_incumbent_depo_variables(const vector<ArcValue<long long>> &arcs, ArcVariables &VD, ArcVariables &DV, const edgeKeySet &free_edges, std::vector<VariableBounds> &original_bounds) {
	for (const ArcValue<long long>& arc: arcs) {
		if (free_edges.count(EdgeKey(arc.from, arc.to)) != 0) continue;
		// En Solution, el depósito se representa con -1.
		IloNumVar variable = arc.from == -1 ? DV[arc.id][arc.vehicle] : VD[arc.id][arc.vehicle];
		fix_and_save_bounds(variable, arc.value, original_bounds);
	}
}

SolveResult RCPPSolver::solve_neighborhood(const Solution &incumbent, const edgeKeySet &free_edges, double gapTolerance) {
	_solve_result = SolveResult();

	vector<VariableBounds> original_bounds;
	SolveResult candidate;
	std::exception_ptr failure;

	try {
		fix_incumbent_3D_variables(incumbent.service, _X, free_edges, original_bounds);
		fix_incumbent_3D_variables(incumbent.traversals, _Y, free_edges, original_bounds);
		fix_incumbent_depo_variables(incumbent.deposit_traversals, _YKD, _YDK, free_edges, original_bounds);
		candidate = solve(gapTolerance);
	} catch (...) {
		failure = std::current_exception();
	}

	_solve_result = SolveResult();
	restore_bounds(original_bounds);

	if (failure) std::rethrow_exception(failure);
	return candidate;
}

// Solution
bool RCPPSolver::is_feasible() const {
	return _solve_result.has_solution;
}

// Read all Concert values while the solution is available, before any output I/O.
Solution RCPPSolver::capture_solution() const {
    Solution solution;
    solution.objective = get_objective_value();
    for (int p = 1; p < _trucks; ++p) {
        for (const auto& arc : _super_graph.arcs()) {
            if (arc.requested) {
                solution.service.push_back({arc.id, arc.from, arc.to, p, std::llround(get_value(_X[arc.id][p]))});
            }
            if (arc.edge_id == -2) continue;
            solution.traversals.push_back({arc.id, arc.from, arc.to, p, std::llround(get_value(_Y[arc.id][p]))});
            solution.flow.push_back({arc.id, arc.from, arc.to, p, get_value(_F[arc.id][p])});
        }
        for (const auto* arc : _super_graph.super_arcs_adj_depo_node()) {
            solution.deposit_traversals.push_back({arc->id, -1, arc->to, p, std::llround(get_value(_YDK[arc->id][p]))});
            solution.deposit_flow.push_back({arc->id, -1, arc->to, p, get_value(_FDK[arc->id][p])});
        }
        for (const auto* arc : _super_graph.super_arcs_adj_node_depo()) {
            solution.deposit_traversals.push_back({arc->id, arc->from, -1, p, std::llround(get_value(_YKD[arc->id][p]))});
        }
    }
    return solution;
}
