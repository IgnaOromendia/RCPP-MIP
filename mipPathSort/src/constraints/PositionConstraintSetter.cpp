#include <constraints/PositionConstraintSetter.h>

void PositionConstraintSetter::set_position_constraint(const ArrayArcVariables& Z) {
    for (int e = 0; e < _edge_amount; e++) {
        for (int k = 0; k < _pass_count[e]; k++) {
            IloExpr expre(_env);
            string name = "Position_" + to_string(e) + "_" + to_string(k);

            expre = _X[e][k];

            for (int t = 0; t < _K; t++)
                expre -= (t + 1) * Z[e][k][t];
            
            add_constraint(0, expre, 0, name);
            expre.end();
        }
    }
}

void PositionConstraintSetter::set_order_constraint() {
    for (int e = 0; e < _edge_amount; e++) {
        for (int k = 0; k < _pass_count[e] - 1; k++) {
            IloExpr expre(_env);
            string name = "Pass_order_" + to_string(e) + "_" + to_string(k);
            expre = _X[e][k+1] - (_X[e][k] + 1);
            add_constraint(0, expre, IloInfinity, name);
            expre.end();
        }  
    }
}
