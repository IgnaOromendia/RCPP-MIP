#include "TestInstance.h"
#include "lib/constraints/FlowConstraintSetter.h"
#include "lib/constraints/PathConstraintSetter.h"
#include <map>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// Compare the actual Concert rows, including all coefficients: an unexpected
// vehicle, duplicate service row or missing connector must not pass unnoticed.
using Terms = std::map<IloInt, double>;
struct Row {
    double lower, upper;
    Terms terms;
    void add(IloNumVar variable, double coefficient) {
        if (coefficient != 0) terms[variable.getId()] += coefficient;
    }
};
using Rows = std::map<std::string, Row>;

void expect_rows(IloModel model, Rows expected) {
    for (IloModel::Iterator it(model); it.ok(); ++it) {
        auto* implementation = dynamic_cast<IloRangeI*>((*it).getImpl());
        check(implementation != nullptr, "Setter added an object other than a range");
        IloRange range(implementation);
        const std::string name = range.getName() ? range.getName() : "<unnamed>";
        auto found = expected.find(name);
        check(found != expected.end(), "Unexpected or duplicate row: " + name);
        check(range.getLB() == found->second.lower && range.getUB() == found->second.upper,
              "Incorrect bounds: " + name);
        Terms actual;
        for (IloExpr::LinearIterator term = range.getLinearIterator(); term.ok(); ++term)
            if (term.getCoef() != 0) actual[term.getVar().getId()] += term.getCoef();
        check(actual == found->second.terms, "Incorrect variables or coefficients: " + name);
        expected.erase(found);
    }
    check(expected.empty(), "Missing row: " + (expected.empty() ? "" : expected.begin()->first));
}

struct Environment {
    IloEnv env;
    ~Environment() { env.end(); }
};

struct Variables {
    ArcVariables X, Y, F;
    ArcVariables YDK, YKD, FDK;
    Variables(IloEnv env, const SuperGraph& graph)
        : X(env, graph.arcs_amount()), Y(env, graph.arcs_amount()),
          F(env, graph.arcs_amount()), YDK(env, graph.arcs_amount()),
          YKD(env, graph.arcs_amount()), FDK(env, graph.arcs_amount()) {
        for (const auto& arc : graph.arcs()) {
            // Index 0 exists as a sentinel: expected rows must never contain it.
            X[arc.id] = IloNumVarArray(env, 3, 0, 1, ILOINT);
            Y[arc.id] = IloNumVarArray(env, 3, 0, IloInfinity, ILOINT);
            F[arc.id] = IloNumVarArray(env, 3, 0, IloInfinity);
            YDK[arc.id] = IloNumVarArray(env, 3, 0, 1, ILOINT);
            YKD[arc.id] = IloNumVarArray(env, 3, 0, 1, ILOINT);
            FDK[arc.id] = IloNumVarArray(env, 3, 0, IloInfinity);
        }
    }
};

std::string suffix(int node, int vehicle) {
    return std::to_string(node + 1) + "_" + std::to_string(vehicle);
}
std::string arc_suffix(const SuperArc& arc) {
    return std::to_string(arc.from + 1) + "_" + std::to_string(arc.to + 1);
}

void test_base(IloEnv& env) {
    IloModel model(env);
    ConstraintSetter setter(env, model);
    IloNumVar x(env, 0, 1);
    IloExpr expression(env);
    expression += 2.5 * x;
    setter.add_constraint(-3, expression, 7, "custom");
    expression.end(); // The row must own its expression after the caller ends it.
    Row row{-3, 7, {}};
    row.add(x, 2.5);
    expect_rows(model, {{"custom", row}});
    IloExpr empty(env);
    setter.add_constraint(0, empty, 0, "empty");
    empty.end();
    expect_rows(model, {{"custom", row}});
}

void test_service(IloEnv& env, const SuperGraph& graph, const Variables& v) {
    IloModel model(env);
    PathConstraintSetter(graph, 3, env, model).set_service_constraint(v.X);
    // Input: one shared undirected edge, one directed arc assigned to vehicle 2,
    // one shared directed arc with zero demand, and one non-required directed arc.
    const auto& arcs = graph.arcs();
    check(arcs[0].pair == 1 && arcs[1].pair == 0 && arcs[2].pair == -1,
          "Unexpected fixture topology");
    Row undirected{1, 1, {}}, assigned{1, 1, {}}, zero_demand{1, 1, {}};
    for (int p : {1, 2}) {
        undirected.add(v.X[arcs[0].id][p], 1);
        undirected.add(v.X[arcs[1].id][p], 1);
        zero_demand.add(v.X[arcs[3].id][p], 1);
    }
    assigned.add(v.X[arcs[2].id][2], 1);
    expect_rows(model, {{"Servicio_" + arc_suffix(arcs[1]), undirected},
                        {"Servicio_" + arc_suffix(arcs[2]), assigned},
                        {"Servicio_" + arc_suffix(arcs[3]), zero_demand}});
}

void test_balances(IloEnv& env, const SuperGraph& graph, const Variables& v) {
    IloModel continuity(env), conservation(env), deposit(env), arrival(env), departure(env);
    PathConstraintSetter path(graph, 3, env, continuity);
    path.set_continuity_constraint(v.X, v.Y, v.YDK, v.YKD);
    FlowConstraintSetter(graph, 3, env, conservation)
        .set_flow_conservation_constraint(v.X, v.F, v.FDK);
    FlowConstraintSetter(graph, 3, env, deposit).set_deposit_flow_constraint(v.X, v.FDK);
    PathConstraintSetter(graph, 3, env, arrival).set_deposit_arrival_constraint(v.YKD);
    PathConstraintSetter(graph, 3, env, departure).set_deposit_departure_constraint(v.YDK);
    Rows route_rows, flow_rows, deposit_rows, arrival_rows, departure_rows;
    for (int p : {1, 2}) {
        std::vector<Row> route(graph.nodes_amount(), Row{0, 0, {}});
        std::vector<Row> flow(graph.nodes_amount(), Row{0, 0, {}});
        Row load{0, 0, {}}, arrive{-IloInfinity, 1, {}}, depart{-IloInfinity, 1, {}};
        // Build the oracle by edge incidence, independently of the graph's
        // in/out adjacency helpers (whose names currently have inverted meaning).
        for (const auto& arc : graph.arcs()) {
            const int a = arc.from, b = arc.to;
            if (a == graph.deposit()) {
                route[b].add(v.YDK[arc.id][p], -1);
                flow[b].add(v.FDK[arc.id][p], 1);
                load.add(v.FDK[arc.id][p], 1);
                depart.add(v.YDK[arc.id][p], 1);
            } else if (b == graph.deposit()) {
                route[a].add(v.YKD[arc.id][p], 1);
                arrive.add(v.YKD[arc.id][p], 1);
            } else {
                route[a].add(v.Y[arc.id][p], 1);
                route[b].add(v.Y[arc.id][p], -1);
                flow[a].add(v.F[arc.id][p], -1);
                flow[b].add(v.F[arc.id][p], 1);
                if (arc.requested) load.add(v.X[arc.id][p], -arc.demand);
                if (arc.zone == -1 || arc.zone == p) {
                    route[a].add(v.X[arc.id][p], 1);
                    route[b].add(v.X[arc.id][p], -1);
                    flow[b].add(v.X[arc.id][p], -arc.demand);
                }
            }
        }
        for (int n = 0; n < graph.nodes_amount(); ++n) {
            route_rows.emplace("Cont_" + suffix(n, p), route[n]);
            flow_rows.emplace("Flujo_" + suffix(n, p), flow[n]);
        }
        deposit_rows.emplace("Flujo_D_" + std::to_string(p), load);
        arrival_rows.emplace("Node_depo_" + std::to_string(p), arrive);
        departure_rows.emplace("Depo_node_" + std::to_string(p), depart);
    }
    expect_rows(continuity, route_rows);
    expect_rows(conservation, flow_rows);
    expect_rows(deposit, deposit_rows);
    expect_rows(arrival, arrival_rows);
    expect_rows(departure, departure_rows);
}

void test_capacity(IloEnv& env, const SuperGraph& graph, const Variables& v) {
    // Fractional and non-default capacities detect truncation and hardcoded 10000.
    for (double capacity : {0.25, 17.0}) {
        IloModel model(env);
        FlowConstraintSetter(graph, 3, env, model)
            .set_flow_bounds_constraint(v.X, v.Y, v.F, v.FDK, v.YDK, capacity);
        Rows expected;
        for (const auto& arc : graph.arcs()) {
            for (int p : {1, 2}) {
                Row row{-IloInfinity, 0, {}};
                if (arc.from == graph.deposit()) {
                    row.add(v.FDK[arc.id][p], 1);
                    row.add(v.YDK[arc.id][p], -capacity);
                    expected.emplace("CotaF_D_" + suffix(arc.to, p), row);
                } else if (arc.to != graph.deposit()) {
                    row.add(v.F[arc.id][p], 1);
                    row.add(v.Y[arc.id][p], -capacity);
                    if (arc.zone == -1 || arc.zone == p)
                        row.add(v.X[arc.id][p], -capacity);
                    expected.emplace("CotaF_" + arc_suffix(arc) + "_" + std::to_string(p), row);
                }
            }
        }
        expect_rows(model, expected);
    }
}

void test_empty(IloEnv& env) {
    Instance instance;
    instance.vehicles = 2;
    instance.nodes = 1;
    const auto graph = test_super_graph(instance);
    Variables v(env, graph);
    IloModel model(env);
    PathConstraintSetter path(graph, 3, env, model);
    FlowConstraintSetter flow(graph, 3, env, model);
    path.set_service_constraint(v.X);
    path.set_continuity_constraint(v.X, v.Y, v.YDK, v.YKD);
    path.set_deposit_arrival_constraint(v.YKD);
    path.set_deposit_departure_constraint(v.YDK);
    flow.set_deposit_flow_constraint(v.X, v.FDK);
    flow.set_flow_conservation_constraint(v.X, v.F, v.FDK);
    flow.set_flow_bounds_constraint(v.X, v.Y, v.F, v.FDK, v.YDK, 17);
    expect_rows(model, {});
}
} // namespace

int main() {
    try {
        Environment owner;
        test_base(owner.env);
        Instance instance;
        instance.vehicles = 2;
        instance.nodes = 4;
        instance.deposit_nodes = {0, 2};
        instance.edges = {{0, 1, -1, 2, 2.5}};
        instance.arcs = {{1, 2, 2, 3, 4}, {2, 3, -1, 5, 0}, {3, 0, 0, 7, 0}};
        const auto graph = test_super_graph(instance);
        Variables variables(owner.env, graph);
        test_service(owner.env, graph, variables);
        test_balances(owner.env, graph, variables);
        test_capacity(owner.env, graph, variables);
        test_empty(owner.env);
        std::cout << "PASS: all constraint setters\n";
        return 0;
    } catch (const IloException& error) {
        std::cerr << "CPLEX test failure: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
    }
    return 1;
}
