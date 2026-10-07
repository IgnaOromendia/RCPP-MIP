#include <constraints/PositionConstraintSetter.h>
#include <iostream>

void PositionConstraintSetter::set_position_constraint(const ArcVariables& A, const ArcVariables& Z) {
    int constraints_added = 0;
    for (int e = 0; e < _edge_amount; e++) {
        IloExpr expre(_env);
        string name = "First_position_" + to_string(e);

        expre = _X[e];

        if (_edges[e].times() == 1) {
            for (int t = 0; t < _K; t++)
                expre -= (t + 1) * Z[e][t];
            constraints_added += add_constraint(0, expre, 0, name);
        } else {
            for (int t = 0; t < _K; t++)
                expre += A[e][t];
            constraints_added += add_constraint(_K + 1, expre, _K + 1, name);
        }
        expre.end();
    }

    std::cout << "set_position_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
