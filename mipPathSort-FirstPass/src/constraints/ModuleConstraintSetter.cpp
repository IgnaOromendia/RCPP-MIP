#include <constraints/ModuleConstraintSetter.h>
#include <iostream>

void ModuleConstraintSetter::set_module_constraints(const VariableMatrix& D, const IloNumVarArray& X) {
    int constraints_added = 0;
    const int segments_count = static_cast<int>(_segments.size());

    for (int r = 0; r < segments_count; r++) {
        const auto [e, ke] = _segments[r];
        if (ke != 0) continue;
        for (int s = 0; s < segments_count; s++) {
            const auto [f, kf] = _segments[s];
            if (kf != 0 || _edges[e].to != _edges[f].from)
                continue;

            IloExpr expre1(_env);
            string name = "Mod_" + to_string(r) + "_" + to_string(s) + "_Pos";
            expre1 = D[r][s] - X[r] + X[s];
            constraints_added += add_constraint(0, expre1, IloInfinity, name);
            expre1.end();

            IloExpr expre2(_env);
            name = "Mod_" + to_string(r) + "_" + to_string(s) + "_Neg";
            expre2 = D[r][s] - X[s] + X[r];
            constraints_added += add_constraint(0, expre2, IloInfinity, name);
            expre2.end();
        }
    }

    std::cout << "set_module_constraints agrego " << constraints_added
              << " restricciones" << std::endl;
}
