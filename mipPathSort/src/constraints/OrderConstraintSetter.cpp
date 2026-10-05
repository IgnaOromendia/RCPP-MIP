#include <constraints/OrderConstraintSetter.h>

void OrderConstraintSetter::set_pass_over_position_constraint() {
    for (size_t e = 0; e < _edges.size(); e++) {
        for (int k = 0; k < _pass_count[e]; k++) {
            IloExpr expre(_env);
            string name = "Sum_Z_" + to_string(e) + "_" + to_string(k) + "_eq_1";
            
            for (int t = 0; t < _K; t++)
                expre += _Z[e][k][t];

            add_constraint(1, expre, 1, name);
            expre.end();
        }
    }
}

void OrderConstraintSetter::set_position_over_pass_constraint() {
    for (int t = 0; t < _K; t++) {
        IloExpr expre(_env);
        string name = "Sum_sum_Z_" + to_string(t);

        for (size_t e = 0; e < _edges.size(); e++) 
            for (int k = 0; k < _pass_count[e]; k++) 
                expre += _Z[e][k][t];
            
        add_constraint(1, expre, 1, name);
        expre.end();
    }
}

void OrderConstraintSetter::set_continuity_constraint(int n) {
    for (int v = 0; v < n; v++) {
        for (int t = 0; t < _K - 1; t++) {
            IloExpr expre(_env);
            string name = "Continuity_v_" + to_string(v) + "_t_" + to_string(t);

            for (size_t e = 0; e < _edges.size(); e++) {
                if (_edges[e].from != v and _edges[e].to != v) continue;

                if (_edges[e].to == v)
                    for (int k = 0; k < _pass_count[e]; k++)
                        expre += _Z[e][k][t];

                if (_edges[e].from == v)
                    for (int k = 0; k < _pass_count[e]; k++)
                        expre -= _Z[e][k][t+1];
            }

            add_constraint(0, expre, 0, name);
            expre.end();
        }
    }
}

void OrderConstraintSetter::set_circuit_constraint(int deposit) {
    IloExpr expre(_env);
    string name = "Circuit_v_" + to_string(deposit);

    for (size_t e = 0; e < _edges.size(); e++) {
        if (_edges[e].to == deposit)
            for (int k = 0; k < _pass_count[e]; k++)
                expre += _Z[e][k][_K-1];

        if (_edges[e].from == deposit)
            for (int k = 0; k < _pass_count[e]; k++)
                expre -= _Z[e][k][0];
    }

    add_constraint(0, expre, 0, name);
    expre.end();
}

void OrderConstraintSetter::set_deposit_constraint(int deposit) {
    IloExpr expre(_env);
    string name = "Deposit_Z_" + to_string(deposit);

    for (size_t e = 0; e < _edges.size(); e++) {
        if (_edges[e].from != deposit and _edges[e].to != deposit) continue;

        if (_edges[e].from == deposit)
            for (int k = 0; k < _pass_count[e]; k++)
                expre += _Z[e][k][0];

    }

    add_constraint(1, expre, 1, name);
    expre.end();
}
