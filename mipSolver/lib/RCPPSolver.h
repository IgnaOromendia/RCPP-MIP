#ifndef RCPPSOLVER_H
#define RCPPSOLVER_H

#include <ilcplex/ilocplex.h>
#include <vector>
#include "SuperGraph.h"
#include "ModelOptions.h"
#include "Solution.h"

using namespace std;

typedef IloArray<IloNumVarArray> NumVarMatrix;
typedef IloArray<IloArray<IloNumVarArray> > NumVarMatrix3;
typedef pair<int,int> pii;

struct SolveResult {
	bool has_solution = false;
	IloAlgorithm::Status status = IloAlgorithm::Unknown;
};

class RCPPSolver{
	public:
		RCPPSolver(SuperGraph super_graph, int vehicles, ModelOptions options = {});
		~RCPPSolver() = default;
		RCPPSolver(const RCPPSolver&) = delete;
		RCPPSolver& operator=(const RCPPSolver&) = delete;
		RCPPSolver(RCPPSolver&&) = delete;
		RCPPSolver& operator=(RCPPSolver&&) = delete;

		void generate_MIP();
		void set_time_objective();
		SolveResult solve(double gapTolerance);
		Solution extract_solution() const;

		// Testing
		bool is_feasible() const;
		double get_obj_value() const;

	private:
		// Allows integration tests to set limits and check ownership traits.
		friend struct RCPPSolverTestAccess;

		// Owns the environment even if construction of a later member fails.
		class Environment {
		public:
			Environment() = default;
			~Environment() noexcept { _handle.end(); }
			Environment(const Environment&) = delete;
			Environment& operator=(const Environment&) = delete;
			Environment(Environment&&) = delete;
			Environment& operator=(Environment&&) = delete;
			IloEnv& get() noexcept { return _handle; }
		private:
			IloEnv _handle;
		};

		// Declaration order matters: the owner is destroyed last, after all
		// Concert handles. _env only borrows its handle and never calls end().
		Environment _environment;
		IloEnv& _env;
		IloModel _model;
		IloCplex _solver;

		void require_solution() const;
		SolveResult _solve_result;

		// Variables
		void generar_variables();

		// Constraints
		void set_service_constraint();
		void set_continuity_constraint();
		void set_depoist_arrival_constraint();
		void set_depoist_departure_constraint();
		void set_deposit_flow_constraint();
		void set_flow_conservation_constraint();
		void set_flow_bounds_constraint();

		// Parameters
		void set_CPLEX_params(double gapTolerance);

		// Auxiliars
		void add_constraint(IloNum lhs, IloExpr& expre, IloNum rhs, string name);
		void set_variable_3D(NumVarMatrix3& V, string var_name, int from, int to, int truck);
		void set_variable_depo_in(NumVarMatrix& V, string var_name, int node, int truck);
		void set_variable_depo_out(NumVarMatrix& V, string var_name, int node, int truck);
		
        const ModelOptions _options;
        int _trucks = 0;
        SuperGraph _super_graph;

		NumVarMatrix3 _X;
		NumVarMatrix3 _Y;
		NumVarMatrix3 _F;
		NumVarMatrix _YKD;
		NumVarMatrix _YDK;
		NumVarMatrix _FDK;
};

#endif
