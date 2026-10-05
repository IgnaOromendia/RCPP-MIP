#include <constraints/ModuleConstraintSetter.h>

void ModuleConstraintSetter::set_module_constraints(const ArcVariables& D, const ArcVariables& X) {
    for (std::size_t e = 0; e < _edges.size(); ++e) {
        for (std::size_t f = 0; f < _edges.size(); ++f) {
            if (_edges[e].vehicle != _edges[f].vehicle ||
                _edges[e].to != _edges[f].from)
                continue;

            IloExpr expre1(_env);
            string name = "Mod_" + to_string(e + 1) + "_" + to_string(f + 1) + "_Pos";
            expre1 = D[e][f] - X[e][0] + X[f][0];
            add_constraint(0, expre1, IloInfinity, name);
            expre1.end();

            IloExpr expre2(_env);
            name = "Mod_" + to_string(e + 1) + "_" + to_string(f + 1) + "_Neg";
            expre2 = D[e][f] - X[f][0] + X[e][0];
            add_constraint(0, expre2, IloInfinity, name);
            expre2.end();
        }
    }
}
