#ifndef CONSTRAINT_SETTER_H
#define CONSTRAINT_SETTER_H

#include <ilcplex/ilocplex.h>

using namespace std;

typedef IloArray<IloNumVarArray> VariableArray;
typedef IloArray<VariableArray> VariableMatrix;

class ConstraintSetter {
public:
    ConstraintSetter(IloEnv& env, IloModel& model): _env(env), _model(model) {};

    bool add_constraint(IloNum lhs, IloExpr& expression, IloNum rhs, const std::string& name) {
        if (not expression.getLinearIterator().ok()) return false;
        _model.add(IloRange(_env, lhs, expression, rhs, name.c_str()));
        return true;
    };

protected:
    IloEnv& _env;
    IloModel& _model;

};

#endif
