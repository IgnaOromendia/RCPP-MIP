#include <constraints/OrderConstraintSetter.h>
#include <iostream>
#include <stdexcept>

void OrderConstraintSetter::set_suc_pred_constraint(const vector<segment> &segments) {
    int constraints_added = 0;
    const int segments_count = segments.size();

    for (int r = 0; r < segments_count; ++r) {
        const auto [e, pass] = segments[r];
        const PathEdge& edge = _edges[e];
        const bool departure = edge.original_edge_id == -2 && edge.from == _deposit;
        const bool arrival = edge.original_edge_id == -2 && edge.to == _deposit;

        IloExpr expre_suc(_env);
        IloExpr expre_pred(_env);

        for (int s = 0; s < segments_count; ++s) {
            const auto [f, next_pass] = segments[s];
            const PathEdge& next = _edges[f];

            const bool next_departure = next.original_edge_id == -2 &&
                                        next.from == _deposit;
            const bool next_arrival = next.original_edge_id == -2 &&
                                      next.to == _deposit;
            if (!arrival && !next_departure && edge.to == next.from)
                expre_suc += _Z[r][s];
            if (!departure && !next_arrival && next.to == edge.from)
                expre_pred += _Z[s][r];
        }

        if (!arrival) {
            if (!expre_suc.getLinearIterator().ok()) {
                expre_suc.end();
                expre_pred.end();
                throw std::invalid_argument("Una pasada no tiene sucesora compatible");
            }
            constraints_added += add_constraint(1, expre_suc, 1, "Suc_" + to_string(r));
        }
        if (!departure) {
            if (!expre_pred.getLinearIterator().ok()) {
                expre_suc.end();
                expre_pred.end();
                throw std::invalid_argument("Una pasada no tiene predecesora compatible");
            }
            constraints_added += add_constraint(1, expre_pred, 1, "Pred_" + to_string(r));
        }

        expre_suc.end();
        expre_pred.end();
    }

    std::cout << "set_suc_pred_constraint agrego " << constraints_added
              << " restricciones" << std::endl;
}
