#ifndef RCPPSOLVER_H
#define RCPPSOLVER_H

#include "CPLEXSolver.h"
#include <vector>
#include "graph/SuperGraph.h"
#include "ModelOptions.h"
#include "SolveResult.h"

using namespace std;

typedef pair<int,int> pii;

class RCPPSolver : public CPLEXSolver {
	public:
		// Borrows the graph: it must outlive the solver and must not be moved or modified.
		RCPPSolver(const SuperGraph& super_graph, int vehicles, ModelOptions options = {});
		RCPPSolver(const SuperGraph&&, int, ModelOptions = {}) = delete;
		~RCPPSolver() override = default;
		RCPPSolver(const RCPPSolver&) = delete;
		RCPPSolver& operator=(const RCPPSolver&) = delete;
		RCPPSolver(RCPPSolver&&) = delete;
		RCPPSolver& operator=(RCPPSolver&&) = delete;

		void generate_MIP();
		void set_time_objective();
		SolveResult solve(double gapTolerance = 0);
		SolveResult solve_neighborhood(const Solution& incumbent, const std::vector<pair<int, int>>& free_edges, double gapTolerance);

		// Testing
		bool is_feasible() const;

	private:
		// Allows integration tests to set limits and check ownership traits.
		friend struct RCPPSolverTestAccess;

		Solution capture_solution() const;
		SolveResult _solve_result;

		// Variables
		void generar_variables();

		// Auxiliars
		void set_variable_3D(NumVarMatrix3& V, string var_name, int from, int to, int truck);
		void set_variable_depo_in(NumVarMatrix& V, string var_name, int node, int truck);
		void set_variable_depo_out(NumVarMatrix& V, string var_name, int node, int truck);
		void fix_incumbent_3D_variables(const vector<ArcValue<long long>>& arcs, NumVarMatrix3& V, const std::vector<pair<int, int>>& free_edges, std::vector<VariableBounds>& original_bounds);
		void fix_incumbent_depo_variables(const vector<ArcValue<long long>>& arcs, NumVarMatrix& VD, NumVarMatrix& DV, const std::vector<pair<int, int>>& free_edges, std::vector<VariableBounds>& original_bounds);
		
        const ModelOptions _options;
        int _trucks = 0;
        const SuperGraph& _super_graph;

		NumVarMatrix3 _X;
		NumVarMatrix3 _Y;
		NumVarMatrix3 _F;
		NumVarMatrix _YKD;
		NumVarMatrix _YDK;
		NumVarMatrix _FDK;
};

#endif
