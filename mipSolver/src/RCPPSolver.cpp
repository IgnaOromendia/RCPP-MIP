#include "../lib/RCPPSolver.h"
#include <stdexcept>
#include <cmath>
#include <limits>
#include <utility>
#include <algorithm>
#include <exception>

RCPPSolver::RCPPSolver(const SuperGraph& super_graph, int vehicles, ModelOptions options)
	: _environment(), _env(_environment.get()), _model(_env), _solver(_env), _options(options), _super_graph(super_graph) {
    _options.validate();
    if (vehicles <= 0 || vehicles == std::numeric_limits<int>::max())
        throw std::invalid_argument("Cantidad de vehiculos fuera de rango");
    if (_super_graph.deposit() < 0) throw std::invalid_argument("Supergrafo sin construir");
    for (const auto& arc : _super_graph.arcs()) {
        if (arc.zone < -1 || arc.zone > vehicles)
            throw std::invalid_argument("Zona del supergrafo fuera de rango");
    }
    _trucks = vehicles + 1; // Vehicle zero remains unused by the formulation.
    _solver.setOut(_env.getNullStream());
}

void RCPPSolver::generate_MIP() {
	_solve_result = {};
	// Variables
	generar_variables();

	// Constraints
	set_service_constraint();

	set_continuity_constraint();

	set_depoist_departure_constraint();
	set_depoist_arrival_constraint();

	set_deposit_flow_constraint();
	set_flow_conservation_constraint();
	set_flow_bounds_constraint();
}

// Variables
void RCPPSolver::set_variable_3D(NumVarMatrix3& V, string var_name, int from, int to, int truck) {
	string name = var_name + "_" + to_string(from + 1) + "_" + to_string(to + 1) + "_" + to_string(truck);
	V[from][to][truck].setName(name.c_str());
}

void RCPPSolver::set_variable_depo_in(NumVarMatrix& V, string var_name, int arc, int truck) {
	string name = var_name + "_D_" + to_string(arc + 1) + "_" + to_string(truck);
	V[arc][truck].setName(name.c_str());
}

void RCPPSolver::set_variable_depo_out(NumVarMatrix& V, string var_name, int arc, int truck) {
	string name = var_name + "_" + to_string(arc + 1) + "_D_" + to_string(truck);
	V[arc][truck].setName(name.c_str());
}

void RCPPSolver::generar_variables() {
	// El eje ij fue satisfecho en el viaje p
	this->_X = NumVarMatrix3(this->_env, this->_super_graph.nodes_amount());

	// Cantidad de veces que el eje ij fue recorrido sin recoger en el viaje p
	this->_Y = NumVarMatrix3(this->_env, this->_super_graph.nodes_amount());

	// Flujo del eje ij en el viaje p
	this->_F = NumVarMatrix3(this->_env, this->_super_graph.nodes_amount());

	for(int i = 0; i < this->_super_graph.nodes_amount(); i++) {
		this->_X[i] = NumVarMatrix(this->_env, this->_super_graph.nodes_amount());
		this->_Y[i] = NumVarMatrix(this->_env, this->_super_graph.nodes_amount());
		this->_F[i] = NumVarMatrix(this->_env, this->_super_graph.nodes_amount());
	}

	// Adyacentes al depósito
	this->_YDK = NumVarMatrix(this->_env, this->_super_graph.nodes_amount());
	this->_FDK = NumVarMatrix(this->_env, this->_super_graph.nodes_amount());
	this->_YKD = NumVarMatrix(this->_env, this->_super_graph.nodes_amount());

	for (int i = 0; i < this->_super_graph.nodes_amount(); i++) {
		this->_YDK[i] = IloNumVarArray(this->_env, this->_trucks, 0, 1, ILOINT);
		this->_YKD[i] = IloNumVarArray(this->_env, this->_trucks, 0, 1, ILOINT);
		this->_FDK[i] = IloNumVarArray(this->_env, this->_trucks, 0, this->_options.capacity, ILOFLOAT);
	}

	for(const SuperArc& arc: this->_super_graph.arcs()) {
		if (arc.edge_id == -2) continue;

		this->_X[arc.from][arc.to] = IloNumVarArray(this->_env, this->_trucks, 0, 1, ILOINT);
		this->_Y[arc.from][arc.to] = IloNumVarArray(this->_env, this->_trucks, 0, this->_options.max_traversals, ILOINT);
        this->_F[arc.from][arc.to] = IloNumVarArray(this->_env, this->_trucks, 0, this->_options.capacity, ILOFLOAT);

		for(int p = 1; p < this->_trucks; p++) {
			// Aristas
			if (arc.requested) this->set_variable_3D(this->_X, "X", arc.from, arc.to, p);
			this->set_variable_3D(this->_Y, "Y", arc.from, arc.to, p);
			this->set_variable_3D(this->_F, "F", arc.from, arc.to, p);	

			// Adyacentes al depósito (entrada)
			if (this->_super_graph.is_adj_depo_node(arc.from)) {
				this->set_variable_depo_in(this->_YDK, "Y", arc.from, p);
				this->set_variable_depo_in(this->_FDK, "F", arc.from, p);
			}

			// Adyacentes al depósito (salida)
			if (this->_super_graph.is_adj_node_depo(arc.to)) {
				this->set_variable_depo_out(this->_YKD, "Y", arc.to, p);
			}
 		}
	}
}

// Restricciones
void RCPPSolver::add_constraint(IloNum lhs, IloExpr& expre, IloNum rhs, string name) {
	if (not expre.getLinearIterator().ok()) return;
	this->_model.add(IloRange(this->_env, lhs, expre, rhs, name.c_str()));
}

void RCPPSolver::set_service_constraint() {
	for (const SuperArc& arc: this->_super_graph.arcs()) {
		if (arc.edge_id == -2) continue;

		// la segunda condición prohibe agregar la restricción 2 veces
		if (arc.requested and arc.pair < arc.id) {
			IloExpr expre(this->_env);
			string name = "Servicio_" + to_string(arc.from+1) + "_" + to_string(arc.to+1);

			int p = arc.zone > 0 ? arc.zone : 1;
			int limit = arc.zone > 0 ? arc.zone + 1 : this->_trucks;

			for(; p < limit; p++) {
				expre += this->_X[arc.from][arc.to][p];

				// Tiene pareja <--> es arista
				if (arc.pair != -1) {
					const SuperArc* pair = this->_super_graph.super_arc_with_id(arc.pair);
					expre += this->_X[pair->from][pair->to][p];
				}
					
			}
			
			this->add_constraint(1, expre, 1, name);
			expre.end();
		}
	}
}

void RCPPSolver::set_continuity_constraint() {
	for (int p = 1; p < this->_trucks; p++) {
		for (int v = 0; v < this->_super_graph.nodes_amount(); v++) {
			IloExpr expre(this->_env);
			string name = "Cont_" + to_string(v+1) + "_" + to_string(p);

			if (this->_super_graph.is_adj_depo_node(v)) expre -= this->_YDK[v][p];
			if (this->_super_graph.is_adj_node_depo(v)) expre += this->_YKD[v][p];
			
			for (const SuperArc* arc: this->_super_graph.super_arcs_for_node_in(v)) {		
				if (arc->edge_id == -2) continue;	
				expre += this->_Y[v][arc->to][p];

				if (arc->requested and (p == arc->zone or arc->zone == -1)) 
					expre += this->_X[v][arc->to][p];
			}

			for (const SuperArc* arc: this->_super_graph.super_arcs_for_node_out(v)) {
				if (arc->edge_id == -2) continue;
				expre -= this->_Y[arc->from][v][p];

				if (arc->requested and (p == arc->zone or arc->zone == -1)) 
					expre -= this->_X[arc->from][v][p];
			}

			this->add_constraint(0, expre, 0, name);
			expre.end();
		}
	}
}

void RCPPSolver::set_depoist_arrival_constraint() {
	for(int p = 1; p < this->_trucks; p++) {
		IloExpr expre(this->_env);
		string name = "Node_depo_" + to_string(p);

		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_node_depo()) 
			expre += this->_YKD[arc->to][p];
		
			
		
		this->add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}

void RCPPSolver::set_depoist_departure_constraint() {
	for(int p = 1; p < this->_trucks; p++) {
		IloExpr expre(this->_env);
		string name = "Depo_node_" + to_string(p);

		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_depo_node()) 
			expre += this->_YDK[arc->from][p];
		
		this->add_constraint(-IloInfinity, expre, 1, name);
		expre.end();
	}
}

void RCPPSolver::set_deposit_flow_constraint() {
	for(int p = 1; p< this->_trucks; p++) {
		IloExpr expre(this->_env);
		string name = "Flujo_D_" + to_string(p);

		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_depo_node()) 
			expre += this->_FDK[arc->from][p];

		for (const SuperArc& arc: this->_super_graph.arcs()) {				
			if (not arc.requested) continue;
			expre -= arc.demand * this->_X[arc.from][arc.to][p];
		}

		this->add_constraint(0, expre, 0, name);
		expre.end();
	}
}

void RCPPSolver::set_flow_conservation_constraint() {
	for (int p = 1; p < this->_trucks; p++) {
		for (int v = 0; v < this->_super_graph.nodes_amount(); v++) {
			IloExpr expre(this->_env);
			string name = "Flujo_" + to_string(v+1) + "_" + to_string(p);

			if (this->_super_graph.is_adj_depo_node(v))
				expre += this->_FDK[v][p];
			
			for (const SuperArc* arc: this->_super_graph.super_arcs_for_node_in(v)) {
				if (arc->edge_id == -2) continue;
				expre -= this->_F[v][arc->to][p];
			}

			for (const SuperArc* arc: this->_super_graph.super_arcs_for_node_out(v)) {
				if (arc->edge_id == -2) continue;
				expre += this->_F[arc->from][v][p];
				if (arc->requested and (p == arc->zone or arc->zone == -1)) expre -= arc->demand * this->_X[arc->from][v][p];
			}

			this->add_constraint(0, expre, 0, name);
			expre.end();
		}
	}
}

void RCPPSolver::set_flow_bounds_constraint() {
	for (const SuperArc& arc : this->_super_graph.arcs()) {
		if (arc.edge_id == -2) continue;
		for (int p = 1; p < this->_trucks; p++) {
			IloExpr expre(this->_env);
			string name = "CotaF_" + to_string(arc.from+1) + "_" + to_string(arc.to+1) + "_" + to_string(p);

			expre += this->_F[arc.from][arc.to][p] - this->_options.capacity * this->_Y[arc.from][arc.to][p];

			if (arc.requested and (arc.zone == p or arc.zone == -1)) 
				expre -= this->_options.capacity * this->_X[arc.from][arc.to][p];

			this->add_constraint(-IloInfinity, expre, 0, name);
			expre.end();
		}
	}

	for (int p = 1; p < this->_trucks; p++) {
		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_depo_node())  {
			IloExpr expre(this->_env);
			string name = "CotaF_D_" + to_string(arc->from+1) + "_" + to_string(p);

			expre += this->_FDK[arc->from][p] - this->_options.capacity * this->_YDK[arc->from][p];

			this->add_constraint(-IloInfinity, expre, 0, name);
			expre.end();
		}
	}
}

// Objective function
void RCPPSolver::set_time_objective() {
	_solve_result = {};
	IloExpr obj(this->_env);

	for(int p = 1; p < this->_trucks; p++) {
		for (const SuperArc& arc: this->_super_graph.arcs()) {
			if (arc.edge_id == -2) continue;
			obj += arc.cost * this->_Y[arc.from][arc.to][p];
			if(arc.requested and (arc.zone == p or arc.zone == -1)) 
				obj += arc.cost * this->_X[arc.from][arc.to][p];
		}
	}

	this->_model.add(IloMinimize(this->_env, obj));
	obj.end();
}

// Params
void RCPPSolver::set_CPLEX_params(double gapTolerance) {
	this->_solver.setParam(IloCplex::EpGap, gapTolerance);
	_solver.setParam(IloCplex::Param::Emphasis::MIP, 1); // factibilidad
}

// Solve and export
SolveResult RCPPSolver::solve(double gapTolerance) {
	_solve_result = {};
	this->set_CPLEX_params(gapTolerance);	
	this->_solver.extract(this->_model);
	const bool found_solution = this->_solver.solve();
	const IloAlgorithm::Status status = this->_solver.getStatus();
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

SolveResult RCPPSolver::solve_neighborhood(const Solution &incumbent, const std::vector<pair<int, int>> &free_edges, double gapTolerance) {
	_solve_result = SolveResult();

	vector<ArcValue<pair<IloNum, IloNum>>> serviceOriginalBounds, traversalOriginalBounds, depositOriginalBounds;
	SolveResult candidate;
	std::exception_ptr failure;

	try {
		for (const ArcValue<long long>& arc: incumbent.service) {
			if (std::find(free_edges.begin(), free_edges.end(), make_pair(arc.from, arc.to)) != free_edges.end()) continue;
			IloNum lb = _X[arc.from][arc.to][arc.vehicle].getLB();
			IloNum up = _X[arc.from][arc.to][arc.vehicle].getUB();
			serviceOriginalBounds.push_back({arc.from, arc.to, arc.vehicle, make_pair(lb, up)});
			_X[arc.from][arc.to][arc.vehicle].setBounds(arc.value, arc.value);
		}

		for (const ArcValue<long long>& arc: incumbent.traversals) {
			if (std::find(free_edges.begin(), free_edges.end(), make_pair(arc.from, arc.to)) != free_edges.end()) continue;
			IloNum lb = _Y[arc.from][arc.to][arc.vehicle].getLB();
			IloNum up = _Y[arc.from][arc.to][arc.vehicle].getUB();
			traversalOriginalBounds.push_back({arc.from, arc.to, arc.vehicle, make_pair(lb, up)});
			_Y[arc.from][arc.to][arc.vehicle].setBounds(arc.value, arc.value);
		}

		for (const ArcValue<long long>& arc: incumbent.deposit_traversals) {
			// En Solution, el depósito se representa con -1.
			if (std::find(free_edges.begin(), free_edges.end(), make_pair(arc.from, arc.to)) != free_edges.end()) continue;
			if (arc.from == -1) {
				IloNum lb = _YDK[arc.to][arc.vehicle].getLB();
				IloNum up = _YDK[arc.to][arc.vehicle].getUB();
				depositOriginalBounds.push_back({arc.from, arc.to, arc.vehicle, make_pair(lb, up)});
				_YDK[arc.to][arc.vehicle].setBounds(arc.value, arc.value);
			} else {
				IloNum lb = _YKD[arc.from][arc.vehicle].getLB();
				IloNum up = _YKD[arc.from][arc.vehicle].getUB();
				depositOriginalBounds.push_back({arc.from, arc.to, arc.vehicle, make_pair(lb, up)});
				_YKD[arc.from][arc.vehicle].setBounds(arc.value, arc.value);
			}
		}

		candidate = solve(gapTolerance);
	} catch (...) {
		failure = std::current_exception();
	}

	// Restaurar las cotas también si falló la fijación o la resolución.
	_solve_result = SolveResult();
	for (const auto& arc: serviceOriginalBounds)
		_X[arc.from][arc.to][arc.vehicle].setBounds(arc.value.first, arc.value.second);
	for (const auto& arc: traversalOriginalBounds)
		_Y[arc.from][arc.to][arc.vehicle].setBounds(arc.value.first, arc.value.second);
	for (const auto& arc: depositOriginalBounds) {
		if (arc.from == -1)
			_YDK[arc.to][arc.vehicle].setBounds(arc.value.first, arc.value.second);
		else
			_YKD[arc.from][arc.vehicle].setBounds(arc.value.first, arc.value.second);
	}

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
    solution.objective = _solver.getObjValue();
    for (int p = 1; p < _trucks; ++p) {
        for (const auto& arc : _super_graph.arcs()) {
            if (arc.requested) {
                solution.service.push_back({arc.from, arc.to, p,
                    std::llround(_solver.getValue(_X[arc.from][arc.to][p]))});
            }
            if (arc.edge_id == -2) continue;
            solution.traversals.push_back({arc.from, arc.to, p,
                std::llround(_solver.getValue(_Y[arc.from][arc.to][p]))});
            solution.flow.push_back({arc.from, arc.to, p,
                _solver.getValue(_F[arc.from][arc.to][p])});
        }
        for (const auto* arc : _super_graph.super_arcs_adj_depo_node()) {
            solution.deposit_traversals.push_back({-1, arc->from, p,
                std::llround(_solver.getValue(_YDK[arc->from][p]))});
            solution.deposit_flow.push_back({-1, arc->from, p,
                _solver.getValue(_FDK[arc->from][p])});
        }
        for (const auto* arc : _super_graph.super_arcs_adj_node_depo()) {
            solution.deposit_traversals.push_back({arc->to, -1, p,
                std::llround(_solver.getValue(_YKD[arc->to][p]))});
        }
    }
    return solution;
}
