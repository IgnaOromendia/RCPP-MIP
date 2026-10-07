#include <constraints/PositionConstraintSetter.h>
#include <iostream>
#include <stdexcept>

void PositionConstraintSetter::set_deposit_constraint(int deposit) {
    int departure = -1;
    int arrival = -1;

    for (int r = 0; r < static_cast<int>(_segments.size()); ++r) {
        const auto [e, pass] = _segments[r];
        const PathEdge& edge = _edges[e];

        if (edge.original_edge_id == -2 && edge.from == deposit) {
            if (departure != -1)
                throw std::invalid_argument(
                    "Hay mas de una pasada que sale del deposito");
            departure = r;
        }

        if (edge.original_edge_id == -2 && edge.to == deposit) {
            if (arrival != -1)
                throw std::invalid_argument(
                    "Hay mas de una pasada que llega al deposito");
            arrival = r;
        }
    }

    if (departure == -1 || arrival == -1)
        throw std::invalid_argument("La ruta debe salir y regresar al deposito");

    IloExpr departure_expr(_env);
    departure_expr += _X[departure];
    add_constraint(1, departure_expr, 1, "Deposit_departure_position");
    departure_expr.end();

    IloExpr arrival_expr(_env);
    arrival_expr += _X[arrival];
    add_constraint(_K, arrival_expr, _K, "Deposit_arrival_position");
    arrival_expr.end();
}

void PositionConstraintSetter::set_position_order_constraint(int deposit) {
    int constraints_added = 0;
    for (int r = 0; r < static_cast<int>(_segments.size()); ++r) {
        const auto [e, pass] = _segments[r];

        for (int s = 0; s < static_cast<int>(_segments.size()); ++s) {
            const auto [f, next_pass] = _segments[s];
            if (_edges[e].to == _edges[f].from && _edges[e].to != deposit) {
                // X[s] >= X[r] + 1 - K(1 - Z[r][s])
                IloExpr lower(_env);
                lower = _X[s] - _X[r] - 1 + _K * (1 - _Z[r][s]);
                constraints_added += add_constraint(
                    0, lower, IloInfinity,
                    "Succ_lower_" + to_string(r) + "_" + to_string(s));
                lower.end();

                // X[s] <= X[r] + 1 + K(1 - Z[r][s])
                IloExpr upper(_env);
                upper = _X[r] + 1 + _K * (1 - _Z[r][s]) - _X[s];
                constraints_added += add_constraint(
                    0, upper, IloInfinity,
                    "Succ_upper_" + to_string(r) + "_" + to_string(s));
                upper.end();
            }
        }
    }

    std::cout << "set_position_order_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}

void PositionConstraintSetter::set_passes_order_constraint(const map<segment, int>& segment_map) {
    int constraints_added = 0;
    for (int e = 0; e < static_cast<int>(_edges.size()); ++e) {
        for (int k = 0; k < _edges[e].times() - 1; ++k) {
            IloExpr expre(_env);
            string name = "Pass_order_" + to_string(e) + "_" + to_string(k);
            int r = segment_map.at({e,k});
            int rp = segment_map.at({e,k+1});
            expre = _X[rp] - _X[r] - 1;
            constraints_added += add_constraint(0, expre, IloInfinity, name);
            expre.end();
        }
    }

    std::cout << "set_passes_order_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
