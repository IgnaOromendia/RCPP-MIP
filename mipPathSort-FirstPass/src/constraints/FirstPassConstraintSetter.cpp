#include <constraints/FirstPassConstraintSetter.h>
#include <iostream>

void FirstPassConstraintSetter::set_seen_continuity_constraint() {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; ++e) {
        if (_edges[e].times() == 1) continue;
        for (int t = 1; t < _K; ++t) {
            IloExpr expression(_env);
            const string name = "Seen_continuity_" + to_string(e) + "_" +
                to_string(t);
            expression = _A[e][t] - _A[e][t - 1];
            constraints_added += add_constraint(0, expression, IloInfinity, name);
            expression.end();
        }
    }

    std::cout << "set_seen_continuity_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void FirstPassConstraintSetter::set_seen_presence_constraint(const VariableMatrix& Z) {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; ++e) {
        if (_edges[e].times() == 1) continue;
        for (int t = 0; t < _K; ++t) {
            IloExpr expression(_env);
            const string name = "Seen_presence_" + to_string(e) + "_" +
                to_string(t);
            expression = _A[e][t] - Z[e][t];
            constraints_added += add_constraint(0, expression, IloInfinity, name);
            expression.end();
        }
    }

    std::cout << "set_seen_presence_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void FirstPassConstraintSetter::set_seen_activation_constraint(const VariableMatrix& Z) {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; ++e) {
        if (_edges[e].times() == 1) continue;
        for (int t = 0; t < _K; ++t) {
            IloExpr expression(_env);
            const string name = "Seen_activation_" + to_string(e) + "_" +
                to_string(t);
            expression = _A[e][t];
            if (t > 0)
                expression -= _A[e][t - 1];
            expression -= Z[e][t];
            constraints_added += add_constraint(-IloInfinity, expression, 0, name);
            expression.end();
        }
    }

    std::cout << "set_seen_activation_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
