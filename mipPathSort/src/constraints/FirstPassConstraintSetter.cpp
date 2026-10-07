#include <constraints/FirstPassConstraintSetter.h>
#include <iostream>

void FirstPassConstraintSetter::set_unique_first_pass_constraint() {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; ++e) {
        IloExpr expression(_env);
        const string name = "Unique_first_pass_A_" + to_string(e);

        for (int t = 0; t < _K; ++t)
            expression += _A[e][t];

        constraints_added += add_constraint(1, expression, 1, name);
        expression.end();
    }

    std::cout << "set_unique_first_pass_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void FirstPassConstraintSetter::set_first_pass_presence_constraint(const ArcVariables& Z) {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; ++e) {
        for (int t = 0; t < _K; ++t) {
            IloExpr expression(_env);
            const string name = "First_pass_presence_" + to_string(e) + "_" +
                to_string(t);
            expression = Z[e][t] - _A[e][t];
            constraints_added += add_constraint(0, expression, IloInfinity, name);
            expression.end();
        }
    }

    std::cout << "set_first_pass_presence_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void FirstPassConstraintSetter::set_no_pass_before_first_constraint(const ArcVariables& Z) {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; ++e) {
        for (int t = 0; t < _K; ++t) {
            IloExpr expression(_env);
            const string name = "No_pass_before_first_" + to_string(e) + "_" +
                to_string(t);
            for (int i = 0; i <= t; ++i)
                expression += _A[e][i];
            expression -= Z[e][t];
            constraints_added += add_constraint(0, expression, IloInfinity, name);
            expression.end();
        }
    }

    std::cout << "set_no_pass_before_first_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
