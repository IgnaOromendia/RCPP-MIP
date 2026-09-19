#include "../lib/RCPPSolver.h"

RCPPSolver::RCPPSolver(string file_name, string turn_file_name) {
	vector<Turn> turns, illegal_turns;

	this->_env 	= IloEnv();
	this->_solver = IloCplex(this->_env);
	this->_model 	= IloModel(this->_env);
	this->_solver.setOut(this->_env.getNullStream());

	read_input_graph(file_name);
	read_input_turns(turn_file_name, turns, illegal_turns);

	// Modificamos el grafo para que no haga turns en U ni turns prohibidos
	this->_super_graph = SuperGraph(&this->_graph, turns, illegal_turns);
}

RCPPSolver::~RCPPSolver() {}

void RCPPSolver::read_input_graph(string file_name) {
	ifstream f(file_name.c_str());

	if(f.fail()){
		cout << "Error en archivo " << file_name << "\n";
		exit(0);
	}

	int n, edges, arcs, ignore;

	f >> this->_trucks >> n >> ignore >> edges >> arcs;

	this->_trucks++; // Contemplamos la zona 0 como no obligatoria

	f.close();

	_graph = Graph(file_name);
}

void RCPPSolver::read_input_turns(string file_name, vector<Turn>& turns, vector<Turn>& illegal_turns) {
	ifstream f(file_name.c_str());

	if(f.fail()){
		cout << "Error en archivo " << file_name << endl;
		exit(0);
	}

	int n_turns, n_illegal_turns;
	f >> n_turns >> n_illegal_turns;

	turns.reserve(n_turns);
	illegal_turns.reserve(n_illegal_turns);

	int v,w,u;

	for(int i = 0; i < n_turns; i++) {
		f >> v >> w >> u;
		turns.emplace_back(v-1,w-1,u-1);
	}

	for(int i = 0; i < n_illegal_turns; i++) {
		f >> v >> w >> u;
		illegal_turns.emplace_back(v-1,w-1,u-1);
	}

	sort(turns.begin(), turns.end());
	sort(illegal_turns.begin(), illegal_turns.end());

    f.close();
}

void RCPPSolver::generate_MIP() {
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
		this->_FDK[i] = IloNumVarArray(this->_env, this->_trucks, 0, this->capacity, ILOFLOAT);
	}

	for(const SuperArc& arc: this->_super_graph.arcs()) {
		if (arc.edge_id == -2) continue;

		this->_X[arc.from][arc.to] = IloNumVarArray(this->_env, this->_trucks, 0, 1, ILOINT);
		this->_Y[arc.from][arc.to] = IloNumVarArray(this->_env, this->_trucks, 0, this->capacity, ILOINT);
        this->_F[arc.from][arc.to] = IloNumVarArray(this->_env, this->_trucks, 0, this->capacity, ILOFLOAT);

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

			expre += this->_F[arc.from][arc.to][p] - this->capacity * this->_Y[arc.from][arc.to][p];

			if (arc.requested and (arc.zone == p or arc.zone == -1)) 
				expre -= this->capacity * this->_X[arc.from][arc.to][p];

			this->add_constraint(-IloInfinity, expre, 0, name);
			expre.end();
		}
	}

	for (int p = 1; p < this->_trucks; p++) {
		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_depo_node())  {
			IloExpr expre(this->_env);
			string name = "CotaF_D_" + to_string(arc->from+1) + "_" + to_string(p);

			expre += this->_FDK[arc->from][p] - this->capacity * this->_YDK[arc->from][p];

			this->add_constraint(-IloInfinity, expre, 0, name);
			expre.end();
		}
	}
}

// Objective function
void RCPPSolver::set_time_objective() {
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
void RCPPSolver::set_CPLEX_params(double gapTolerance, int cutsMode) {
	this->_solver.setParam(IloCplex::EpGap, gapTolerance);

	_solver.setParam(IloCplex::Param::MIP::Cuts::Cliques, cutsMode);
	_solver.setParam(IloCplex::Param::MIP::Cuts::Covers, cutsMode);
	_solver.setParam(IloCplex::Param::MIP::Cuts::FlowCovers, cutsMode);

	_solver.setParam(IloCplex::Param::Emphasis::MIP, 1); // factibilidad
}

// Solve and export
void RCPPSolver::solve(double gapTolerance, int cutsMode) {
	this->set_CPLEX_params(gapTolerance, cutsMode);	
	this->_solver.extract(this->_model);
	// this->_solver.exportModel(this->model_file_name.c_str());
	if (!this->_solver.solve()) {
		cout << "No se encontro solucion. Status: " << this->_solver.getStatus() << endl;
		return;
	}

	int status = this->_solver.getStatus();
 
	double obj_value = this->_solver.getObjValue();
	double best_bound = this->_solver.getBestObjValue();
	double gap = this->_solver.getMIPRelativeGap();

	cout << "Funcion objetivo: " << obj_value << " (" << status << ")" << endl;
}

// Solution
bool RCPPSolver::is_feasible() const {
	return this->_solver.getStatus() != 3;
}

double RCPPSolver::get_obj_value() const {
	return this->_solver.getObjValue();
}

// Export solution
void RCPPSolver::export_solution() {
	int status = this->_solver.getStatus();
 
	double obj_value = this->_solver.getObjValue();

	if (status == 3) return;

	ofstream f(this->output_file_name.c_str());

	if(f.fail()){
		cout << "Error en archivo de salida " << this->output_file_name << endl;
		exit(0);
	}

	f << "OBJ: " << obj_value << "\n";

	f << "\n---- X ----\n";

	for(int p = 1; p < this->_trucks; p++) {
		for (const SuperArc& arc: this->_super_graph.arcs()) {
			if (not arc.requested) continue;
			int value = this->_solver.getValue(this->_X[arc.from][arc.to][p]) > this->TOLERANCE;
			f << "X_" << arc.from+1 << "_" << arc.to+1 << "_" << p << " = " << value << "\n";
		}
	}

	f << "\n---- Y ----\n";

	for(int p = 1; p < this->_trucks; p++) {
		for (const SuperArc& arc: this->_super_graph.arcs()) {
			if (arc.edge_id == -2) continue;
			int value = this->_solver.getValue(this->_Y[arc.from][arc.to][p]);
			f << "Y_" << arc.from+1 << "_" << arc.to+1 << "_" << p << " = " << value << "\n";
		}
	}

	f << "\n---- YDK & YKD ----\n";

	for(int p = 1; p < this->_trucks; p++) {
		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_depo_node()) {
			int value = this->_solver.getValue(this->_YDK[arc->from][p]);
			f << "Y_D_" << arc->from+1 << "_" << p << " = " << value << "\n";
		}

		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_node_depo()) {
			int value = this->_solver.getValue(this->_YKD[arc->to][p]);
			f << "Y_" << arc->to+1 << "_D_" << p << " = " << value << "\n";
		}
	}

	f << "\n---- F ----\n";

	for(int p = 1; p < this->_trucks; p++) {
		for (const SuperArc& arc: this->_super_graph.arcs()) {
			if (arc.edge_id == -2) continue;
			double value = this->_solver.getValue(this->_F[arc.from][arc.to][p]);
			f << "F_" << arc.from+1 << "_" << arc.to+1 << "_" << p << " = " << value << "\n";
		}
	}

	f << "\n---- FDK ----\n";

	for(int p = 1; p < this->_trucks; p++) {
		for (const SuperArc* arc: this->_super_graph.super_arcs_adj_depo_node()) {
			double value = this->_solver.getValue(this->_FDK[arc->from][p]);
			f << "F_D_" << arc->from+1 << "_" << p << " = " << value << "\n";
		}
	}

	f.close();
}
