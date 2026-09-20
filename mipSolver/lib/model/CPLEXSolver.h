#ifndef CPLEXSOLVER_H
#define CPLEXSOLVER_H

#include <ilcplex/ilocplex.h>
#include <string>
#include <utility>
#include <vector>

typedef IloArray<IloNumVarArray> ArcVariables;

class CPLEXSolver {
public:
    CPLEXSolver();
    virtual ~CPLEXSolver() = default;
    CPLEXSolver(const CPLEXSolver&) = delete;
    CPLEXSolver& operator=(const CPLEXSolver&) = delete;
    CPLEXSolver(CPLEXSolver&&) = delete;
    CPLEXSolver& operator=(CPLEXSolver&&) = delete;

protected:
    struct VariableBounds {
        IloNumVar variable;
        IloNum lower;
        IloNum upper;
    };

    IloNumVarArray create_variable_array(IloInt size, IloNum lb, IloNum ub, IloNumVar::Type type);
    ArcVariables create_arc_variable(IloInt size);

    void set_variable_name(IloNumVar variable, const std::string& name);
    std::pair<IloNum, IloNum> get_variable_bounds(IloNumVar variable) const;
    void set_variable_bounds(IloNumVar variable, IloNum lb, IloNum ub);
    void fix_variable(IloNumVar variable, IloNum value);
    void fix_and_save_bounds(IloNumVar variable, IloNum value, std::vector<VariableBounds>& original_bounds);
    void restore_bounds(const std::vector<VariableBounds>& original_bounds);

    IloExpr create_expression();
    void add_constraint(IloNum lhs, IloExpr& expression, IloNum rhs, const std::string& name);
    void set_objective(const IloExpr& expression);
    void set_CPLEX_params(double gapTolerance);
    bool solve_model(double gapTolerance);
    IloAlgorithm::Status get_status() const;
    IloNum get_value(IloNumVar variable) const;
    IloNum get_objective_value() const;

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

    // Destroy the environment last, after the derived class and Concert handles.
    Environment _environment;
    IloEnv& _env;
    IloModel _model;
    IloCplex _solver;
};

#endif
