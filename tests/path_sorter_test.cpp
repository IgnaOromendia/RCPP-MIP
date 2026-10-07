#include <model/PathSorter.h>
#include <constraints/FirstPassConstraintSetter.h>
#include <constraints/ModuleConstraintSetter.h>
#include <constraints/OrderConstraintSetter.h>
#include <constraints/PositionConstraintSetter.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

using Terms = std::map<IloInt, double>;
using Rows = std::map<std::string, Terms>;

Terms range_terms(const IloRange& range) {
    Terms result;
    for (IloExpr::LinearIterator term = range.getLinearIterator(); term.ok(); ++term)
        if (term.getCoef() != 0)
            result[term.getVar().getId()] += term.getCoef();
    return result;
}

void check_rows(const IloModel& model, Rows expected, IloNum lower, IloNum upper) {
    for (IloModel::Iterator it(model); it.ok(); ++it) {
        auto* implementation = dynamic_cast<IloRangeI*>((*it).getImpl());
        check(implementation != nullptr, "Constraint setter added a non-range object");
        IloRange range(implementation);
        const std::string name = range.getName() ? range.getName() : "<unnamed>";
        const auto found = expected.find(name);
        check(found != expected.end(), "Unexpected or duplicate row: " + name);
        check(range.getLB() == lower && range.getUB() == upper,
              "Incorrect bounds: " + name);
        check(range_terms(range) == found->second, "Incorrect terms: " + name);
        expected.erase(found);
    }
    check(expected.empty(), "Missing constraint row");
}

void check_module_constraints() {
    IloEnv env;
    try {
        IloModel model(env);
        const std::vector<PathEdge> edges = {
            {3, 0, 4, 5, 1, 1, 0},
            {7, 1, 5, 9, 1, 0, 1},
            {11, 2, 5, 9, 2, 1, 0},
            {15, -2, 9, 4, 1, 0, 1}
        };
        IloNumVarArray X(env, edges.size(), 1, 4, ILOINT);
        ArcVariables D(env, edges.size());
        for (std::size_t e = 0; e < edges.size(); ++e) {
            D[e] = IloNumVarArray(env, edges.size(), 0, 4, ILOINT);
        }

        ModuleConstraintSetter setter(edges, env, model);
        setter.set_module_constraints(D, X);

        Rows expected;
        const auto add_pair = [&](std::size_t e, std::size_t f) {
            expected["Mod_" + std::to_string(e + 1) + "_" +
                     std::to_string(f + 1) + "_Pos"] = {
                {D[e][f].getId(), 1}, {X[e].getId(), -1}, {X[f].getId(), 1}
            };
            expected["Mod_" + std::to_string(e + 1) + "_" +
                     std::to_string(f + 1) + "_Neg"] = {
                {D[e][f].getId(), 1}, {X[e].getId(), 1}, {X[f].getId(), -1}
            };
        };
        add_pair(0, 1);
        add_pair(1, 3);
        add_pair(3, 0);

        check_rows(model, std::move(expected), 0, IloInfinity);
        env.end();
    } catch (...) {
        env.end();
        throw;
    }
}

void check_order_constraints() {
    IloEnv env;
    try {
        const std::vector<PathEdge> edges = {
            {3, -2, 3, 0, 1, 1, 0},
            {7, 0, 0, 1, 1, 1, 1},
            {11, -2, 1, 3, 1, 0, 1}
        };
        const std::vector<int> pass_count = {2, 2, 2};
        const int K = 6;
        const int node_count = 4;
        const int deposit = 3;
        ArcVariables Z(env, edges.size());
        for (std::size_t e = 0; e < edges.size(); ++e)
            Z[e] = IloNumVarArray(env, K, 0, 1, ILOBOOL);

        {
            IloModel model(env);
            OrderConstraintSetter setter(Z, edges, pass_count, K, env, model);
            setter.set_passes_amount_constraint();
            Rows expected;
            for (std::size_t e = 0; e < edges.size(); ++e) {
                Terms terms;
                for (int t = 0; t < K; ++t) terms[Z[e][t].getId()] = 1;
                expected["Pass_count_Z_" + std::to_string(e)] = std::move(terms);
            }
            check_rows(model, std::move(expected), 2, 2);
        }

        {
            IloModel model(env);
            OrderConstraintSetter setter(Z, edges, pass_count, K, env, model);
            setter.set_position_over_pass_constraint();
            Rows expected;
            for (int t = 0; t < K; ++t) {
                Terms terms;
                for (std::size_t e = 0; e < edges.size(); ++e)
                    terms[Z[e][t].getId()] = 1;
                expected["Sum_sum_Z_" + std::to_string(t)] = std::move(terms);
            }
            check_rows(model, std::move(expected), 1, 1);
        }

        {
            IloModel model(env);
            OrderConstraintSetter setter(Z, edges, pass_count, K, env, model);
            setter.set_continuity_constraint(node_count);
            Rows expected;
            for (int v : {0, 1, 3}) {
                for (int t = 0; t < K - 1; ++t) {
                    Terms terms;
                    for (std::size_t e = 0; e < edges.size(); ++e) {
                        if (edges[e].to == v)
                            terms[Z[e][t].getId()] += 1;
                        if (edges[e].from == v)
                            terms[Z[e][t + 1].getId()] -= 1;
                    }
                    expected["Continuity_v_" + std::to_string(v) + "_t_" +
                             std::to_string(t)] = std::move(terms);
                }
            }
            check_rows(model, std::move(expected), 0, 0);
        }

        {
            IloModel model(env);
            OrderConstraintSetter setter(Z, edges, pass_count, K, env, model);
            setter.set_depo_return_constraint(deposit);
            Rows expected = {{
                "Return_to_deposit_Z_3",
                {{Z[2][K - 1].getId(), 1}}
            }};
            check_rows(model, std::move(expected), 1, 1);
        }

        {
            IloModel model(env);
            OrderConstraintSetter setter(Z, edges, pass_count, K, env, model);
            setter.set_depo_arrival_constraint(deposit);
            Rows expected = {{
                "Deposit_Z_3",
                {{Z[0][0].getId(), 1}}
            }};
            check_rows(model, std::move(expected), 1, 1);
        }

        env.end();
    } catch (...) {
        env.end();
        throw;
    }
}

void check_first_pass_constraints() {
    IloEnv env;
    try {
        const int edge_count = 2;
        const int K = 3;
        ArcVariables A(env, edge_count);
        ArcVariables Z(env, edge_count);
        for (int e = 0; e < edge_count; ++e) {
            A[e] = IloNumVarArray(env, K, 0, 1, ILOBOOL);
            Z[e] = IloNumVarArray(env, K, 0, 1, ILOBOOL);
        }

        {
            IloModel model(env);
            FirstPassConstraintSetter setter(A, edge_count, K, env, model);
            setter.set_unique_first_pass_constraint();
            Rows expected;
            for (int e = 0; e < edge_count; ++e) {
                Terms terms;
                for (int t = 0; t < K; ++t) terms[A[e][t].getId()] = 1;
                expected["Unique_first_pass_A_" + std::to_string(e)] =
                    std::move(terms);
            }
            check_rows(model, std::move(expected), 1, 1);
        }

        {
            IloModel model(env);
            FirstPassConstraintSetter setter(A, edge_count, K, env, model);
            setter.set_first_pass_presence_constraint(Z);
            Rows expected;
            for (int e = 0; e < edge_count; ++e)
                for (int t = 0; t < K; ++t)
                    expected["First_pass_presence_" + std::to_string(e) + "_" +
                             std::to_string(t)] = {
                        {Z[e][t].getId(), 1}, {A[e][t].getId(), -1}
                    };
            check_rows(model, std::move(expected), 0, IloInfinity);
        }

        {
            IloModel model(env);
            FirstPassConstraintSetter setter(A, edge_count, K, env, model);
            setter.set_no_pass_before_first_constraint(Z);
            Rows expected;
            for (int e = 0; e < edge_count; ++e) {
                for (int t = 0; t < K; ++t) {
                    Terms terms = {{Z[e][t].getId(), -1}};
                    for (int i = 0; i <= t; ++i)
                        terms[A[e][i].getId()] = 1;
                    expected["No_pass_before_first_" + std::to_string(e) + "_" +
                             std::to_string(t)] = std::move(terms);
                }
            }
            check_rows(model, std::move(expected), 0, IloInfinity);
        }

        env.end();
    } catch (...) {
        env.end();
        throw;
    }
}

void check_position_constraints() {
    IloEnv env;
    try {
        const int edge_count = 2;
        const int K = 3;
        IloNumVarArray X(env, edge_count, 1, K, ILOINT);
        ArcVariables A(env, edge_count);

        for (int e = 0; e < edge_count; ++e)
            A[e] = IloNumVarArray(env, K, 0, 1, ILOBOOL);

        {
            IloModel model(env);
            PositionConstraintSetter setter(X, edge_count, K, env, model);
            setter.set_position_constraint(A);

            Rows expected;
            for (int e = 0; e < edge_count; ++e) {
                Terms terms = {{X[e].getId(), 1}};
                for (int t = 0; t < K; ++t)
                    terms[A[e][t].getId()] = -(t + 1);
                expected["First_position_" + std::to_string(e)] =
                    std::move(terms);
            }
            check_rows(model, std::move(expected), 0, 0);
        }

        env.end();
    } catch (...) {
        env.end();
        throw;
    }
}

int main() {
    try {
        check_module_constraints();
        check_order_constraints();
        check_first_pass_constraints();
        check_position_constraints();

        PathSortInstance instance;
        instance.vehicles = 2;
        instance.edges = {
            {3, 0, 4, 5, 1, 1, 2},
            {7, -1, 5, 8, 2, 0, 1}
        };

        PathSorter sorter(std::move(instance));
        check(sorter.edges().size() == 2, "PathSorter must retain every input edge");
        check(sorter.edges()[0].super_arc_id == 3 &&
              sorter.edges()[0].original_edge_id == 0 &&
              sorter.edges()[0].vehicle == 1 &&
              sorter.edges()[0].times() == 3,
              "PathSorter must consume the already parsed multiplicities");
        check(sorter.edges()[1].original_edge_id == -1 && sorter.edges()[1].times() == 1,
              "PathSorter must retain connector edges");
        check(sorter.pass_counts() == std::vector<int>({3, 1}),
              "PathSorter must define m_e from the incumbent traversal counts");
        check(sorter.total_passes() == 4,
              "PathSorter must define K as the sum of every m_e");

        // Sparse super-arc ids must not be used as indexes into the two local edges.
        sorter.generate_MIP();

        PathSorter empty;
        check(empty.edges().empty(), "Default PathSorter must have no input edges");
        check(empty.pass_counts().empty() && empty.total_passes() == 0,
              "An empty sorter must define an empty set of passes");
        empty.generate_MIP();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
