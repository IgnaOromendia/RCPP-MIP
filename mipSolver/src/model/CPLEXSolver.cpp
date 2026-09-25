#include "../../lib/model/CPLEXSolver.h"

CPLEXSolver::CPLEXSolver(): _environment(), _env(_environment.get()), _model(_env), _solver(_env) {
    _solver.setOut(_env.getNullStream());
}

void CPLEXSolver::set_emphasis(int value) {
    _solver.setParam(IloCplex::Param::Emphasis::MIP, value); 
}

void CPLEXSolver::set_time_limit(double limit) {
    _solver.setParam(IloCplex::Param::TimeLimit, limit);
}

IloNumVarArray CPLEXSolver::create_variable_array(IloInt size, IloNum lb, IloNum ub, IloNumVar::Type type) {
    return IloNumVarArray(_env, size, lb, ub, type);
}

ArcVariables CPLEXSolver::create_arc_variable(IloInt size) {
    return ArcVariables(_env, size);
}

void CPLEXSolver::set_variable_name(IloNumVar variable, const std::string& name) {
    variable.setName(name.c_str());
}

std::pair<IloNum, IloNum> CPLEXSolver::get_variable_bounds(IloNumVar variable) const {
    return {variable.getLB(), variable.getUB()};
}

void CPLEXSolver::set_variable_bounds(IloNumVar variable, IloNum lb, IloNum ub) {
    variable.setBounds(lb, ub);
}

void CPLEXSolver::fix_variable(IloNumVar variable, IloNum value) {
    set_variable_bounds(variable, value, value);
}

void CPLEXSolver::fix_and_save_bounds(IloNumVar variable, IloNum value, std::vector<VariableBounds>& original_bounds) {
    const auto bounds = get_variable_bounds(variable);
    original_bounds.push_back({variable, bounds.first, bounds.second});
    fix_variable(variable, value);
}

void CPLEXSolver::restore_bounds(const std::vector<VariableBounds>& original_bounds) {
    for (const auto& bounds : original_bounds)
        set_variable_bounds(bounds.variable, bounds.lower, bounds.upper);
}

IloExpr CPLEXSolver::create_expression() {
    return IloExpr(_env);
}

void CPLEXSolver::add_constraint(IloNum lhs, IloExpr& expression, IloNum rhs, const std::string& name) {
    if (not expression.getLinearIterator().ok()) return;
    _model.add(IloRange(_env, lhs, expression, rhs, name.c_str()));
}

void CPLEXSolver::set_objective(const IloExpr& expression) {
    _model.add(IloMinimize(_env, expression));
}

void CPLEXSolver::set_CPLEX_params(double gapTolerance) {
    _solver.setParam(IloCplex::EpGap, gapTolerance);
    set_emphasis(1); // factibilidad
    set_time_limit(300);
}

bool CPLEXSolver::solve_model(double gapTolerance) {
    set_CPLEX_params(gapTolerance);
    _solver.extract(_model);
    return _solver.solve();
}

IloAlgorithm::Status CPLEXSolver::get_status() const {
    return _solver.getStatus();
}

IloNum CPLEXSolver::get_value(IloNumVar variable) const {
    return _solver.getValue(variable);
}

IloNum CPLEXSolver::get_objective_value() const {
    return _solver.getObjValue();
}
