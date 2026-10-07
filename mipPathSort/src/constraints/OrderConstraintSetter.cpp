#include <constraints/OrderConstraintSetter.h>
#include <iostream>

void OrderConstraintSetter::set_passes_amount_constraint() {
    int constraints_added = 0;
    for (size_t e = 0; e < _edges.size(); e++) {
        IloExpr expre(_env);
        string name = "Pass_count_Z_" + to_string(e);

        for (int t = 0; t < _K; t++)
            expre += _Z[e][t];

        constraints_added += add_constraint(_pass_count[e], expre, _pass_count[e], name);
        expre.end();
    }

    std::cout << "set_passes_amount_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void OrderConstraintSetter::set_position_over_pass_constraint() {
    int constraints_added = 0;
    for (int t = 0; t < _K; t++) {
        IloExpr expre(_env);
        string name = "Sum_sum_Z_" + to_string(t);

        for (size_t e = 0; e < _edges.size(); e++)
            expre += _Z[e][t];
            
        constraints_added += add_constraint(1, expre, 1, name);
        expre.end();
    }

    std::cout << "set_position_over_pass_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void OrderConstraintSetter::set_continuity_constraint(int n) {
    int constraints_added = 0;
    for (int v = 0; v < n; v++) {
        for (int t = max(_depo_dist[v] - 1, 0); t < _K - 1; t++) {
            IloExpr expre(_env);
            string name = "Continuity_v_" + to_string(v) + "_t_" + to_string(t);

            for (size_t e = 0; e < _edges.size(); e++) {
                if (_edges[e].from != v and _edges[e].to != v) continue;

                if (_edges[e].to == v)
                    expre += _Z[e][t];

                if (_edges[e].from == v)
                    expre -= _Z[e][t+1];
            }

            constraints_added += add_constraint(0, expre, 0, name);
            expre.end();
        }
    }

    std::cout << "set_continuity_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void OrderConstraintSetter::set_depo_return_constraint(int deposit) {
    int constraints_added = 0;
    IloExpr expre(_env);
    string name = "Return_to_deposit_Z_" + to_string(deposit);

    for (size_t e = 0; e < _edges.size(); e++)
        if (_edges[e].to == deposit)
            expre += _Z[e][_K-1];

    constraints_added += add_constraint(1, expre, 1, name);
    expre.end();

    std::cout << "set_depo_return_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void OrderConstraintSetter::set_depo_arrival_constraint(int deposit) {
    int constraints_added = 0;
    IloExpr expre(_env);
    string name = "Deposit_Z_" + to_string(deposit);

    for (size_t e = 0; e < _edges.size(); e++) {
        if (_edges[e].from != deposit and _edges[e].to != deposit) continue;

        if (_edges[e].from == deposit)
            expre += _Z[e][0];

    }

    constraints_added += add_constraint(1, expre, 1, name);
    expre.end();

    std::cout << "set_depo_arrival_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
