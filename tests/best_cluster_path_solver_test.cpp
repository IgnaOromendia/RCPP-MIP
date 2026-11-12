#include <graph/Graph.h>
#include <graph/SuperGraph.h>
#include <io/BestClusterPathInstanceReader.h>
#include <model/BestClusterPathSolver.h>
#include <model/ClusterBoundary.h>
#include <model/ClusterRCPPSolver.h>
#include <model/ClusterSolutionMerger.h>
#include <model/ClusterSubProblemBuilder.h>
#include <model/SolutionEulerCircuit.h>
#include <model/SubtourBreaker.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ThreeClusterInstance.h"

static_assert(std::is_base_of_v<RCPPSolver, BestClusterPathSolver>);
static_assert(std::is_base_of_v<RCPPSolver, ClusterRCPPSolver>);

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action>
void expect_invalid(Action action, const std::string& fragment) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos,
              "Unexpected validation diagnostic");
        return;
    }
    throw std::runtime_error("Expected invalid_argument");
}

Instance read_instance(int vehicles = 1) {
    std::istringstream graph(std::to_string(vehicles) + " 3 2 2 0\n1 3\n1 2 1 7 3\n2 3 1 11 4\n");
    std::istringstream turns("0 0\n");
    std::istringstream clusters("arista cluster origen destino vehiculo servicio recorridos pasadas\n1 1 1 2 0 0 0 0\n2 2 2 3 0 0 0 0\n");
    std::istringstream neutral("node_id reasons\n2 boundary\n");
    return BestClusterPathInstanceReader::read(graph, turns, clusters, &neutral);
}

class InspectableBestClusterPathSolver : public BestClusterPathSolver {
public:
    using BestClusterPathSolver::BestClusterPathSolver;

    double objective_coefficient(IloNumVar variable) const {
        for (IloExpr::LinearIterator term = _solver.getObjective().getLinearIterator();
             term.ok(); ++term) {
            if (term.getVar().getImpl() == variable.getImpl())
                return term.getCoef();
        }
        return 0;
    }

    double y_objective_coefficient(int arc_id, int vehicle) const {
        return objective_coefficient(_Y[arc_id][vehicle]);
    }

    double x_objective_coefficient(int arc_id, int vehicle) const {
        return objective_coefficient(_X[arc_id][vehicle]);
    }
};

class InspectableClusterRCPPSolver : public ClusterRCPPSolver {
public:
    using ClusterRCPPSolver::ClusterRCPPSolver;

    std::pair<IloNum, IloNum> departure_bounds(int arc, int vehicle) const {
        return get_variable_bounds(_YDK[arc][vehicle]);
    }

};

void check_fixed_neutral_portals() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 5;
    input.deposit_nodes = {0, 3};
    input.arcs = {
        {0, 1, 0, 5, 0}, {0, 2, 0, 1, 0},
        {1, 2, 1, 2, 1},
        {2, 3, 0, 5, 0}, {1, 3, 0, 1, 0},
        {2, 4, 0, 1, 0}, {4, 1, 0, 1, 0},
    };
    const Graph graph(input);
    const SuperGraph super_graph(graph, {}, {});
    std::map<int, int> mapping;
    for (int node = 0; node < super_graph.nodes_amount(); ++node)
        mapping[node] = 1000 + 3 * node;
    const int entry = super_graph.super_arc_with_id(0)->from;
    const int exit = super_graph.super_arc_with_id(3)->to;
    ClusterBoundary boundary;
    boundary.cluster = 1;
    boundary.visits = {{1,
        {ClusterPortalKind::NeutralInOut, -1, 0, 1, true, mapping.at(entry)},
        {ClusterPortalKind::NeutralInOut, -1, 3, 1, false, mapping.at(exit)},
        {2}}};

    ClusterRCPPSolver solver(super_graph, boundary, mapping);
    solver.generate_MIP();
    const SolveResult result = solver.solve(0);
    // The fixed streets cost 5 + 2 + 5. Choosing the two alternative streets
    // at the same boundary nodes would cost only 8, including both detours.
    check(result.has_solution && result.status == IloAlgorithm::Optimal &&
              std::abs(result.get_obj_value() - 12) < 1e-9,
          "Neutral portals must retain their physical streets despite cheaper alternatives");
    int departures = 0, arrivals = 0;
    for (const auto& value : result.extract_solution().deposit_traversals) {
        if (value.value <= 0) continue;
        const SuperArc& arc = *super_graph.super_arc_with_id(value.id);
        check(value.value == 1, "A fixed portal must be used exactly once");
        if (value.from == -1) {
            ++departures;
            check(arc.to == entry, "The local route changed its entry street");
        } else {
            ++arrivals;
            check(arc.from == exit, "The local route changed its exit street");
        }
    }
    check(departures == 1 && arrivals == 1, "A fixed visit must have one entry and exit");

    ClusterBoundary depot_boundary = boundary;
    depot_boundary.visits.front().entry.kind = ClusterPortalKind::GlobalDeposit;
    depot_boundary.visits.front().exit.kind = ClusterPortalKind::GlobalDeposit;
    depot_boundary.visits.front().entry.global_virtual_node = -1;
    depot_boundary.visits.front().exit.global_virtual_node = -1;
    ClusterRCPPSolver depot_solver(super_graph, depot_boundary, {});
    depot_solver.generate_MIP();
    const SolveResult depot_result = depot_solver.solve(0);
    check(depot_result.has_solution && depot_result.status == IloAlgorithm::Optimal &&
              std::abs(depot_result.get_obj_value() - 8) < 1e-9,
          "Global-deposit portals must still allow different streets at their boundary nodes");

    ClusterBoundary missing_endpoint = boundary;
    missing_endpoint.visits.front().entry.global_virtual_node = -1;
    expect_invalid([&] {
        ClusterRCPPSolver invalid(super_graph, missing_endpoint, mapping);
        invalid.generate_MIP();
    }, "nodo virtual global");
    auto missing_mapping = mapping;
    missing_mapping.erase(entry);
    expect_invalid([&] {
        ClusterRCPPSolver invalid(super_graph, boundary, missing_mapping);
        invalid.generate_MIP();
    }, "mapeo global");
    ClusterBoundary wrong_endpoint = boundary;
    wrong_endpoint.visits.front().exit.global_virtual_node = 99999;
    expect_invalid([&] {
        ClusterRCPPSolver invalid(super_graph, wrong_endpoint, mapping);
        invalid.generate_MIP();
    }, "no tiene conectores locales");
}

Solution three_cluster_circuit(const SuperGraph& super_graph) {
    auto connection = [&](int from, int to) -> const SuperArc& {
        for (const SuperArc& arc : super_graph.arcs())
            if (arc.from == from && arc.to == to) return arc;
        throw std::runtime_error("Missing connection in exact three-cluster circuit");
    };
    auto physical = [&](int id) -> const SuperArc& {
        for (const SuperArc& arc : super_graph.arcs())
            if (arc.edge_id == id) return arc;
        throw std::runtime_error("Missing physical arc");
    };
    // Start inside c1, visit c3 directly, then c2, and return to c1.
    const std::vector<const SuperArc*> route = {
        &physical(1), &physical(2), &physical(6), &physical(7), &physical(8),
        &physical(3), &physical(4), &physical(5), &physical(0),
    };
    Solution solution;
    const SuperArc& departure = connection(super_graph.deposit(), route.front()->from);
    solution.deposit_traversals.push_back({departure.id, -1, departure.to, 1, 1});
    for (size_t i = 0; i < route.size(); ++i) {
        const SuperArc& arc = *route[i];
        (arc.requested ? solution.service : solution.traversals).emplace_back(arc, 1);
        if (i + 1 < route.size()) {
            const SuperArc& connector = connection(arc.to, route[i + 1]->from);
            solution.traversals.emplace_back(connector, 1);
            if (connector.is_neutral) ++solution.objective;
        }
    }
    const SuperArc& arrival = connection(route.back()->to, super_graph.deposit());
    solution.deposit_traversals.push_back({arrival.id, arrival.from, -1, 1, 1});
    return solution;
}

void check_three_cluster_pipeline(const Instance& input, bool check_changed_portals = false) {
    const NeutralExpandedGraph expanded(input);
    const SuperGraph super_graph(
        expanded, expanded.get_turns(), expanded.get_illegal_turns(),
        expanded.get_illegal_ext_turns());
    BestClusterPathSolver solver(super_graph, expanded.get_neutral_edges());
    solver.generate_MIP();
    const SolveResult result = solver.solve(0);
    check(result.has_solution && result.status == IloAlgorithm::Optimal &&
              std::abs(result.get_obj_value() - 3) < 1e-9,
          "Three directed cycles must need exactly three cluster crossings");

    // Use an exact circuit so segmentation always exercises c1 -> c3.
    const Solution global = three_cluster_circuit(super_graph);
    const SubtourBreaker unchanged(super_graph, expanded, global, solver.get_neutral_arc_ids());
    check(!unchanged.was_repaired() && unchanged.get_connections().empty() && unchanged.get_solution().objective == 3,
          "A connected crossing circuit must pass through repair unchanged");
    ClusterBoundaryExtractor extractor(super_graph, expanded, global);
    const auto boundaries = extractor.get_boundaries();
    check(boundaries.size() == 3 && boundaries.at(1).visits.size() == 2 &&
              boundaries.at(2).visits.size() == 1 && boundaries.at(3).visits.size() == 1,
          "Three-cluster segmentation lost or duplicated visits");
    const ClusterPortal& exit = boundaries.at(1).visits.front().exit;
    const ClusterPortal& entry = boundaries.at(3).visits.front().entry;
    check(exit.kind == ClusterPortalKind::NeutralInOut &&
              entry.kind == ClusterPortalKind::NeutralInOut &&
              exit.global_arc_id == entry.global_arc_id &&
              exit.global_virtual_node == super_graph.super_arc_with_id(exit.global_arc_id)->from &&
              entry.global_virtual_node == super_graph.super_arc_with_id(entry.global_arc_id)->to &&
              exit.count == 1 && entry.count == 1,
          "Direct c1 -> c3 must be one shared crossing, without a c2 visit");

    ClusterSubProblemBuilder builder(expanded, super_graph, boundaries);
    auto problems = builder.get_subproblems();
    check(problems.size() == 3, "Expected three physical subproblems");
    ClusterSolutionMerger merger(
        super_graph, expanded, problems, extractor.get_global_arc_by_edge());
    std::vector<Solution> local_solutions;
    for (const ClusterProblem& problem : problems) {
        const auto physical_count = std::count(input.record_clusters.begin(),
            input.record_clusters.end(), problem.cluster);
        check(problem.instance.edges.empty() && problem.instance.arcs.size() == physical_count,
              "Subproblem lost a physical record or contains synthetic records");
        ClusterRCPPSolver local(*problem.super_graph, problem.boundary,
            problem.mapping.local_to_global_node);
        local.generate_MIP();
        const SolveResult local_result = local.solve(0);
        check(local_result.has_solution && local_result.status == IloAlgorithm::Optimal,
              "Three-cluster local problem must be optimal");
        local_solutions.push_back(local_result.extract_solution());
        merger.add_solution(local_solutions.back());
    }
    const Solution merged = merger.build_solution();
    check(std::abs(merged.objective - 9) < 1e-9,
          "Merged distance must count the nine physical arcs only");
    std::map<int, long long> expected_crossings, merged_crossings;
    for (const auto& traversal : global.traversals)
        if (super_graph.super_arc_with_id(traversal.id)->is_neutral)
            expected_crossings[traversal.id] += traversal.value;
    long long crossings = 0;
    for (const auto& traversal : merged.traversals)
        if (super_graph.super_arc_with_id(traversal.id)->is_neutral) {
            crossings += traversal.value;
            merged_crossings[traversal.id] += traversal.value;
        }
    check(crossings == 3 && merged_crossings == expected_crossings,
          "Merge changed the IDs or multiplicities of the chosen direct crossings");
    const auto circuit = SolutionEulerCircuit(super_graph.nodes_amount(), merged).circuit();
    check(!circuit.empty(), "The merged direct crossings must form a connected circuit");
    const GraphSolution contracted = expanded.contract_solution(
        super_graph.to_graph_solution(merged));
    std::map<int, long long> expected;
    for (int id = 0; id < 9; ++id) expected[id] = 1;
    check(contracted.service == expected && contracted.traversals.empty() &&
              std::abs(contracted.objective - 9) < 1e-9,
          "Contraction must preserve all nine services once and omit synthetic arcs");

    if (check_changed_portals) {
        for (bool entry : {true, false}) {
            auto change_portal = [&] {
                auto changed = local_solutions;
                for (size_t i = 0; i < problems.size(); ++i) {
                    const ClusterProblem& problem = problems[i];
                    int vehicle = 0;
                    for (const ClusterVisit& visit : problem.boundary.visits) {
                        if (visit.is_passthrough()) continue;
                        ++vehicle;
                        const ClusterPortal& portal = entry ? visit.entry : visit.exit;
                        if (portal.kind != ClusterPortalKind::NeutralInOut) continue;
                        auto& values = changed[i].deposit_traversals;
                        const auto selected = std::find_if(values.begin(), values.end(),
                            [&](const auto& value) {
                                return value.value > 0 && value.vehicle == vehicle &&
                                    (value.from == -1) == entry;
                            });
                        check(selected != values.end(), "Missing local portal in merge regression");
                        const auto connectors = entry
                            ? problem.super_graph->super_arcs_adj_depo_node()
                            : problem.super_graph->super_arcs_adj_node_depo();
                        for (const SuperArc* connector : connectors) {
                            const int node = entry ? connector->to : connector->from;
                            if (problem.super_graph->original_node(node) != portal.global_node ||
                                problem.mapping.local_to_global_node.at(node) == portal.global_virtual_node)
                                continue;
                            *selected = {connector->id, entry ? -1 : node,
                                entry ? node : -1, vehicle, 1};
                            return changed;
                        }
                    }
                }
                throw std::runtime_error("Missing alternative street for merge regression");
            };
            const auto changed = change_portal();
            expect_invalid([&] {
                ClusterSolutionMerger invalid(super_graph, expanded, problems,
                    extractor.get_global_arc_by_edge());
                for (const Solution& solution : changed) invalid.add_solution(solution);
                invalid.build_solution();
            }, "cambia la calle");
        }
    }
    std::cout << "PASS three clusters: crossings=3 distance=9 service_records=9\n";
}

void check_boundary_copy_pipeline() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 4;
    input.deposit_nodes = {2};
    input.arcs = {{0, 1, 1, 2, 1}, {1, 2, 1, 3, 1}, {2, 0, 1, 4, 1},
                  {1, 3, 1, 5, 1}, {3, 0, 1, 6, 1}};
    input.record_clusters = {2, 1, 1, 2, 2};
    input.neutral_nodes = {0, 1};
    const NeutralExpandedGraph expanded(input);
    const SuperGraph super(expanded, expanded.get_turns(), expanded.get_illegal_turns(),
                           expanded.get_illegal_ext_turns());
    int copy_id = -1;
    for (int id : expanded.expanded_arc_ids())
        if (id >= 5 && expanded.edge_cluster(id) == 1) copy_id = id;
    check(copy_id >= 0, "Missing shared-street copy in cluster 1");
    InspectableBestClusterPathSolver solver(super, expanded.get_neutral_edges());
    solver.generate_MIP();
    for (const SuperArc& arc : super.arcs())
        if (arc.edge_id == copy_id)
            check(solver.y_objective_coefficient(arc.id, 1) == 0,
                  "Using a physical copy must cost zero crossings");
        else if (arc.is_neutral)
            check(solver.y_objective_coefficient(arc.id, 1) == 1,
                  "Changing clusters must cost one crossing");
    const auto result = solver.solve(0);
    // The depot is inside c1. Required c2 service forces entry and return.
    check(result.has_solution && result.status == IloAlgorithm::Optimal &&
              std::abs(result.get_obj_value() - 2) < 1e-9,
          "Two clusters with an internal depot require exactly two crossings");

    auto physical = [&](int id) -> const SuperArc& {
        for (const SuperArc& arc : super.arcs())
            if (arc.edge_id == id) return arc;
        throw std::runtime_error("Missing boundary route street");
    };
    auto connector = [&](int from, int to) -> const SuperArc& {
        for (const SuperArc& arc : super.arcs())
            if (arc.from == from && arc.to == to) return arc;
        throw std::runtime_error("Missing legal boundary route connector");
    };
    // An exact circuit forces two uses of the copy, with service only on its
    // original in c2. The first c1 visit serves both its records before exiting.
    const std::vector<int> route{2, copy_id, 1, 2, 0, 3, 4, copy_id, 1};
    Solution global;
    std::set<int> serviced;
    const SuperArc& departure = connector(super.deposit(), physical(route.front()).from);
    global.deposit_traversals.push_back({departure.id, -1, departure.to, 1, 1});
    for (size_t i = 0; i < route.size(); ++i) {
        const SuperArc& arc = physical(route[i]);
        auto& values = arc.requested && serviced.insert(arc.edge_id).second
            ? global.service : global.traversals;
        values.emplace_back(arc, 1);
        if (i + 1 < route.size()) {
            const SuperArc& turn = connector(arc.to, physical(route[i + 1]).from);
            global.traversals.emplace_back(turn, 1);
            if (turn.is_neutral) ++global.objective;
        }
    }
    const SuperArc& arrival = connector(physical(route.back()).to, super.deposit());
    global.deposit_traversals.push_back({arrival.id, arrival.from, -1, 1, 1});
    check(global.objective == 2, "Copies must not inflate route crossings");
    ClusterBoundaryExtractor extractor(super, expanded, global);
    const auto boundaries = extractor.get_boundaries();
    ClusterSubProblemBuilder builder(expanded, super, boundaries);
    auto problems = builder.get_subproblems();
    ClusterSolutionMerger merger(super, expanded, problems, extractor.get_global_arc_by_edge());
    for (const ClusterProblem& problem : problems) {
        check(problem.instance.arcs.size() == 3, "Each cluster needs all three interior streets");
        if (problem.cluster == 1) {
            int optional = 0;
            for (const InstanceEdge& arc : problem.instance.arcs)
                if (arc.zone == 0) {
                    ++optional;
                    check(arc.cost == 2 && arc.demand == 0,
                          "Local copy must retain cost and have no demand");
                }
            check(optional == 1, "Only the copied shared street is optional in c1");
        }
        ClusterRCPPSolver local(*problem.super_graph, problem.boundary,
                                problem.mapping.local_to_global_node);
        local.generate_MIP();
        const auto local_result = local.solve(0);
        check(local_result.has_solution && local_result.status == IloAlgorithm::Optimal,
              "Boundary copy subproblem must remain feasible");
        merger.add_solution(local_result.extract_solution());
    }
    const Solution merged = merger.build_solution();
    check(!SolutionEulerCircuit(super.nodes_amount(), merged).circuit().empty(),
          "Boundary copy merge must form a continuous depot circuit");
    long long crossings = 0;
    for (const auto& value : merged.traversals)
        if (super.super_arc_with_id(value.id)->is_neutral) crossings += value.value;
    check(crossings == 2 && merged.objective == 31,
          "Merge must preserve crossings and include the cost of both copies");
    const GraphSolution contracted = expanded.contract_solution(super.to_graph_solution(merged));
    check(contracted.service == std::map<int, long long>{{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}} &&
              contracted.traversals == std::map<int, long long>{{0, 2}, {1, 1}, {2, 1}},
          "Merge must serve once and sum copied traversals into the original");
    const Graph original(input);
    const SuperGraph original_super(original, input.turns, input.illegal_turns);
    const Solution sorter = expanded.contract_solution(merged, super, original_super);
    check(!SolutionEulerCircuit(original_super.nodes_amount(), sorter).circuit().empty(),
          "Sorter export must remain connected in the original supergraph");
    const auto projected = original_super.to_graph_solution(sorter);
    check(projected.service == contracted.service && projected.traversals == contracted.traversals &&
              projected.objective == contracted.objective,
          "Sorter export must preserve physical service, traversals and distance");
}

void check_cross_cluster_prohibitions() {
    Instance input = three_cluster_cycles();
    // No legal first entry into c3: its only access is 0 -> 5.
    input.illegal_turns = {{2, 0, 5}, {4, 0, 5}};
    const Graph original(input);
    const SuperGraph original_super(original, input.turns, input.illegal_turns);
    RCPPSolver solver(original_super, 1);
    solver.generate_MIP();
    const SolveResult result = solver.solve(0);
    check(!result.has_solution && result.status == IloAlgorithm::Infeasible,
          "Control graph must be infeasible: every first entry into c3 is forbidden");
    std::cout << "Control: original graph is infeasible\n";

    const NeutralExpandedGraph expanded(input);
    const SuperGraph expanded_super(
        expanded, expanded.get_turns(), expanded.get_illegal_turns(),
        expanded.get_illegal_ext_turns());

    const SuperGraph unrestricted_super(expanded, {}, {});
    BestClusterPathSolver unrestricted_solver(unrestricted_super, expanded.get_neutral_edges());
    unrestricted_solver.generate_MIP();
    const SolveResult unrestricted_result = unrestricted_solver.solve(0);
    check(unrestricted_result.has_solution &&
              unrestricted_result.status == IloAlgorithm::Optimal &&
              std::abs(unrestricted_result.get_obj_value() - 3) < 1e-9,
          "Direct crossings without prohibitions must preserve the three-crossing optimum");

    BestClusterPathSolver expanded_solver(expanded_super, expanded.get_neutral_edges());
    expanded_solver.generate_MIP();
    const SolveResult expanded_result = expanded_solver.solve(0);
    std::cout << "Prohibited turns: original=" << input.illegal_turns.size()
              << " expanded=" << expanded.get_illegal_turns().size()
              << " extended=" << expanded.get_illegal_ext_turns().size() << '\n';
    check(!expanded_result.has_solution && expanded_result.status == IloAlgorithm::Infeasible,
          "Expansion must preserve infeasibility caused by cross-cluster prohibitions");
}

// Two exact circuits cross the same pair of clusters at different boundary
// nodes. Neither provisional replacement has a direct SuperArc.
void check_unrepairable_crossings() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 11;
    input.deposit_nodes = {8};
    // The two cluster interiors loop around their own nodes. Pairing either
    // interior back to itself would join streets 1->3 / 0->1 and 4->0 / 3->4,
    // respectively: both forbidden U orientations. The original crossings
    // 1->3->4 and 4->0->1 are legal.
    input.arcs = {{0,1,0,1,0}, {1,2,0,1,0}, {2,6,0,1,0}, {6,1,0,1,0}, {1,3,0,1,0},
                  {3,4,0,1,0}, {4,5,0,1,0}, {5,7,0,1,0}, {7,4,0,1,0}, {4,0,0,1,0},
                  {8,9,0,1,0}, {9,10,0,1,0}, {10,8,0,1,0}};
    input.record_clusters = {1,1,1,1,1,2,2,2,2,2,1,1,1};
    input.neutral_nodes = {0,3};
    const NeutralExpandedGraph expanded(input);
    const SuperGraph super(expanded, expanded.get_turns(), expanded.get_illegal_turns(),
                           expanded.get_illegal_ext_turns());
    auto physical = [&](int id) -> const SuperArc& {
        for (const auto& arc : super.arcs()) if (arc.edge_id == id) return arc;
        throw std::runtime_error("Missing no-U fixture street");
    };
    auto connector = [&](int from, int to) -> const SuperArc& {
        for (const auto* arc : super.super_arcs_from(from)) if (arc->to == to) return *arc;
        throw std::runtime_error("Missing legal no-U fixture connector");
    };
    Solution solution;
    const auto& departure = connector(super.deposit(), physical(10).from);
    const auto& arrival = connector(physical(12).to, super.deposit());
    solution.deposit_traversals = {{departure.id,-1,departure.to,1,1},
                                  {arrival.id,arrival.from,-1,1,1}};
    for (int id = 0; id < 13; ++id) {
        const auto& arc = physical(id);
        solution.traversals.emplace_back(arc, 1);
        if (id == 12) continue;
        const auto& turn = connector(arc.to, physical(id == 9 ? 0 : id+1).from);
        solution.traversals.emplace_back(turn, 1);
        if (turn.is_neutral) ++solution.objective;
    }
    vector<int> neutral_ids;
    for (const auto& arc : super.arcs()) if (arc.is_neutral) neutral_ids.push_back(arc.id);
    expect_invalid([&] { SubtourBreaker(super, expanded, solution, neutral_ids); },
                   "reconectar la componente con cruces");
}

enum class RepairScenario { Feasible, Infeasible, DiscardFragments, WithoutService };

void check_crossing_repair(RepairScenario scenario = RepairScenario::Feasible) {
    const bool feasible = scenario != RepairScenario::Infeasible;
    const bool detach = scenario == RepairScenario::DiscardFragments;
    const bool service_free = scenario == RepairScenario::WithoutService;
    Instance input;
    input.vehicles = 1;
    input.nodes = 10;
    input.deposit_nodes = {0};
    input.arcs = {{0,1,1,1,1}, {1,2,1,1,1}, {2,3,1,1,1}, {3,0,1,1,1},
                  {4,5,0,1,0}, {8,6,0,1,0}, {6,7,0,1,0}, {9,4,0,1,0},
                  {2,4,0,1,0}, {6,0,0,1,0}, {0,6,0,1,0}, {4,2,0,1,0},
                  {5,8,0,1,0}, {7,9,0,1,0}};
    if (service_free) input.arcs[2].zone = input.arcs[3].zone = 0;
    input.record_clusters = {1,1,2,2,1,1,2,2,1,1,2,2,1,2};
    input.neutral_nodes = {0,2,4,6};
    // Removing the bridge leaves the provisional entry/exit pair infeasible.
    if (!feasible) input.illegal_turns = {{1,2,4}};
    const NeutralExpandedGraph expanded(input);
    const SuperGraph super(expanded, expanded.get_turns(), expanded.get_illegal_turns(),
                           expanded.get_illegal_ext_turns());
    auto physical = [&](int id) -> const SuperArc& {
        for (const auto& arc : super.arcs())
            if (arc.edge_id == id) return arc;
        throw std::runtime_error("Missing repair street");
    };
    auto connector = [&](int from, int to) -> const SuperArc& {
        for (const auto* arc : super.super_arcs_from(from))
            if (arc->to == to) return *arc;
        throw std::runtime_error("Missing repair connector");
    };
    vector<int> neutral_ids;
    for (const auto& arc : super.arcs())
        if (arc.is_neutral) neutral_ids.push_back(arc.id);
    Solution global;
    const auto& departure = connector(super.deposit(), physical(0).from);
    const auto& arrival = connector(physical(3).to, super.deposit());
    global.deposit_traversals = {{departure.id,-1,departure.to,1,1},
                                {arrival.id,arrival.from,-1,1,1}};
    for (const auto& route : {vector<int>{0,1,2,3}, vector<int>{4,12,5,6,13,7}})
        for (size_t i = 0; i < route.size(); ++i) {
            const auto& arc = physical(route[i]);
            (arc.requested ? global.service : global.traversals).emplace_back(arc, 1);
            if (route[i] == 3) continue;
            const int next = route[(i + 1) % route.size()];
            const auto& turn = connector(arc.to, physical(next).from);
            global.traversals.emplace_back(turn, 1);
            if (turn.is_neutral) ++global.objective;
        }
    check(global.objective == 3, "Expected one depot crossing and two disconnected crossings");
    // Order the full neutral vector to exercise both admissible operations:
    // reconnect to the depot circuit, or split and discard internal fragments.
    std::stable_partition(neutral_ids.begin(), neutral_ids.end(), [&](int id) {
        const auto& arc = *super.super_arc_with_id(id);
        return arc.from == physical(7).to || (detach && arc.from == physical(5).to);
    });
    const SubtourBreaker repair(super, expanded, global, neutral_ids);
    check(repair.was_repaired() && repair.get_solution().objective == 1 && repair.get_connections().size() == 2,
          "Exchange must remove two crossings and create two internal provisional links");
    // Verify balance independently, including the provisional support.
    map<int,long long> balance;
    for (const auto* values : {&repair.get_solution().service, &repair.get_solution().traversals,
                              &repair.get_solution().deposit_traversals})
        for (const auto& v : *values) { balance[v.from] += v.value; balance[v.to] -= v.value; }
    for (const auto& link : repair.get_connections()) {
        balance[link.from] += link.count;
        balance[link.to] -= link.count;
        check((link.count == 0) == detach, "Reclassification must discard crossing-free fragments: scenario=" + to_string(static_cast<int>(scenario)) + " count=" + to_string(link.count));
        for (const auto* arc : super.super_arcs_from(link.from))
            check(arc->to != link.to, "Fixture must require a provisional path, not a direct turn");
    }
    for (const auto& [node, value] : balance) check(value == 0, "Exchange changed node balance");
    ClusterBoundaryExtractor extractor(super, expanded, repair.get_solution(), repair.get_connections());
    const auto boundaries = extractor.get_boundaries();
    ClusterSubProblemBuilder builder(expanded, super, boundaries);
    auto problems = builder.get_subproblems();
    if (service_free)
        check(problems.at(1).boundary.visits.front().service_edge_ids.empty() &&
              problems.at(1).boundary.optimized_visit_count() == 1,
              "A provisional visit without services must still be optimized");
    ClusterSolutionMerger merger(super, expanded, problems, extractor.get_global_arc_by_edge());
    bool no_solution = false;
    for (auto& problem : problems) {
        ClusterRCPPSolver local(*problem.super_graph, problem.boundary, problem.mapping.local_to_global_node);
        local.generate_MIP();
        auto result = local.solve(0);
        if (!result.has_solution) { no_solution = true; break; }
        merger.add_solution(result.extract_solution());
    }
    if (!feasible) {
        check(no_solution, "A forbidden provisional reconstruction must remain infeasible");
        return;
    }
    check(!no_solution, "Local solvers must rebuild provisional connections");
    const auto merged = merger.build_solution();
    check(!SolutionEulerCircuit(super.nodes_amount(), merged).circuit().empty(),
          "Repaired merge must be connected and contain only real arcs");
    map<int,long long> expected_neutral, actual_neutral;
    for (const auto& value : repair.get_solution().traversals)
        if (super.super_arc_with_id(value.id)->is_neutral && value.value)
            expected_neutral[value.id] += value.value;
    for (const auto& value : merged.traversals)
        if (super.super_arc_with_id(value.id)->is_neutral && value.value)
            actual_neutral[value.id] += value.value;
    check(expected_neutral == actual_neutral, "Merge must preserve the repaired neutral vector");
    const auto projected = expanded.contract_solution(super.to_graph_solution(merged));
    const map<int,long long> services = service_free ? map<int,long long>{{0,1},{1,1}} :
        map<int,long long>{{0,1},{1,1},{2,1},{3,1}};
    check(projected.service == services,
          "Optional reconstruction must not create services");
    const Graph original(input);
    const SuperGraph original_super(original, input.turns, input.illegal_turns);
    const auto sorter = expanded.contract_solution(merged, super, original_super);
    check(!SolutionEulerCircuit(original_super.nodes_amount(), sorter).circuit().empty(),
          "Repaired physical solution must contract legally for the sorter");
}

void check_discarded_services() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 5;
    input.deposit_nodes = {0};
    input.arcs = {{0, 1, 1, 1, 1}, {1, 2, 1, 1, 1}, {2, 0, 1, 1, 1},
                  {0, 3, 1, 1, 0}, {3, 4, 1, 1, 0}, {4, 0, 1, 1, 0}};
    input.record_clusters = {1, 1, 1, 1, 1, 1};
    const NeutralExpandedGraph expanded(input);
    const SuperGraph super(expanded, {}, {});
    auto connector = [&](int from, int to) -> const SuperArc& {
        for (const auto* arc : super.super_arcs_from(from))
            if (arc->to == to) return *arc;
        throw std::runtime_error("Missing test connector");
    };
    Solution global;
    for (int first : {0, 3}) {
        for (int id = first; id < first + 3; ++id) {
            const auto& arc = *super.super_arc_with_id(id);
            global.service.emplace_back(arc, 1);
            const int next = id == first + 2 ? first : id + 1;
            if (first == 3 || id != 2)
                global.traversals.emplace_back(connector(arc.to,
                    super.super_arc_with_id(next)->from), first == 3 ? 3 : 1);
            if (first == 3) global.traversals.emplace_back(arc, 2);
        }
    }
    const auto& departure = connector(super.deposit(), super.super_arc_with_id(0)->from);
    const auto& arrival = connector(super.super_arc_with_id(2)->to, super.deposit());
    global.deposit_traversals = {{departure.id, -1, departure.to, 1, 1},
                                {arrival.id, arrival.from, -1, 1, 1}};
    // The strict extractor still rejects the original snapshot.
    expect_invalid([&] { SolutionEulerCircuit(super.nodes_amount(), global).circuit(); },
                   "desconectada");
    const SubtourBreaker repaired(super, expanded, global, {});
    check(!repaired.was_repaired() && repaired.get_connections().empty(),
          "Discard alone does not invalidate the crossing optimum");
    check(global.service.back().value == 1, "Repair must preserve the original snapshot");
    // A whole extra internal lap attached to the depot route must survive.
    Solution connected = repaired.get_solution();
    for (int id = 0; id < 3; ++id) {
        const auto& arc = *super.super_arc_with_id(id);
        connected.traversals.emplace_back(arc, 1);
        connected.traversals.emplace_back(connector(arc.to,
            super.super_arc_with_id((id + 1) % 3)->from), 1);
    }
    const SubtourBreaker unchanged(super, expanded, connected, {});
    check(!unchanged.was_repaired() && unchanged.get_connections().empty() &&
          SolutionEulerCircuit(super.nodes_amount(), unchanged.get_solution()).circuit().size() ==
          SolutionEulerCircuit(super.nodes_amount(), connected).circuit().size(),
          "Internal cycles in the depot component must remain unchanged");
    for (const auto& value : repaired.get_solution().service)
        check(value.value == (value.id < 3 ? 1 : 0), "Discard must remove disconnected X");
    for (const auto& value : repaired.get_solution().traversals)
        if (value.from >= 6 && value.to >= 6)
            check(value.value == 0, "Discard must remove turns and all multiplicities");
    ClusterBoundaryExtractor extractor(super, expanded, repaired.get_solution());
    auto boundaries = extractor.get_boundaries();
    ClusterSubProblemBuilder builder(expanded, super, boundaries);
    auto problems = builder.get_subproblems();
    check(builder.services_reassigned(), "Reassigning a service must invalidate optimality");
    auto passthrough = boundaries;
    passthrough.at(1).visits.front().service_edge_ids.clear();
    passthrough.at(1).visits.front().passthrough = true;
    ClusterSubProblemBuilder promoted(expanded, super, passthrough);
    const auto promoted_problems = promoted.get_subproblems();
    check(promoted.services_reassigned() &&
          promoted_problems[0].boundary.optimized_visit_count() == 1 &&
          promoted_problems[0].boundary.visits[0].service_edge_ids.size() == 6,
          "Services assigned to a former passthrough must trigger optimization");
    check(problems.size() == 1 && problems[0].boundary.visits[0].service_edge_ids.size() == 6,
          "Discarded zero-demand services must join the first cluster visit");
    ClusterSolutionMerger merger(super, expanded, problems, extractor.get_global_arc_by_edge());
    auto& problem = problems[0];
    ClusterRCPPSolver local(*problem.super_graph, problem.boundary, problem.mapping.local_to_global_node);
    local.generate_MIP();
    const auto result = local.solve(0);
    check(result.has_solution, "Discarded services must remain locally feasible");
    merger.add_solution(result.extract_solution());
    const auto merged = merger.build_solution();
    check(SolutionEulerCircuit(super.nodes_amount(), merged).circuit().size() > 0,
          "Reassigned services must produce a connected route");
    const auto physical = expanded.contract_solution(super.to_graph_solution(merged));
    check(physical.service == std::map<int, long long>{{0,1},{1,1},{2,1},{3,1},{4,1},{5,1}},
          "Every required record must be served once after discard");
    expect_invalid([&] {
        ClusterSubProblemBuilder missing(expanded, super, {});
        missing.get_subproblems();
    }, "cluster 1");
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--check-cross-cluster-turns") {
            check_cross_cluster_prohibitions();
            return 0;
        }
        check(argc == 1, "Usage: best_cluster_path_solver_test [--check-cross-cluster-turns]");
        check_discarded_services();
        check_unrepairable_crossings();
        check_crossing_repair();
        check_crossing_repair(RepairScenario::Infeasible);
        check_crossing_repair(RepairScenario::DiscardFragments);
        check_crossing_repair(RepairScenario::WithoutService);
        check_boundary_copy_pipeline();
        check_fixed_neutral_portals();
        check_three_cluster_pipeline(three_cluster_cycles());
        Instance branching = three_cluster_cycles();
        branching.arcs.push_back({1, 0, 0, 0.5, 0});
        branching.arcs.push_back({0, 2, 0, 0.5, 0});
        branching.record_clusters.insert(branching.record_clusters.end(), {1, 1});
        check_three_cluster_pipeline(branching, true);
        Instance restricted = three_cluster_cycles();
        restricted.illegal_turns = {{4, 0, 5}};
        check_three_cluster_pipeline(restricted);
        check_cross_cluster_prohibitions();
        constexpr int node_count = 10;
        Solution ordered_input;
        ordered_input.deposit_traversals = {
            {10, -1, 1, 1, 1}, {16, 7, -1, 1, 1},
        };
        ordered_input.service = {
            {11, 1, 2, 1, 1}, {14, 5, 6, 1, 1},
        };
        ordered_input.traversals = {
            {12, 2, 3, 1, 1}, {13, 3, 5, 1, 1},
            {15, 6, 7, 1, 1},
        };
        const std::vector<ClusterSolutionArc> ordered =
            SolutionEulerCircuit(node_count, ordered_input).circuit();
        check(ordered.size() == 7 && ordered.front().from == -1 &&
                  ordered.front().to == 1 && ordered.back().from == 7 &&
                  ordered.back().to == -1,
              "Hierholzer did not order X, Y and deposit connectors");
        for (size_t i = 1; i < ordered.size(); ++i)
            check(ordered[i - 1].to == ordered[i].from,
                  "Hierholzer returned a discontinuous circuit");

        Solution disconnected = ordered_input;
        disconnected.traversals.push_back({17, 8, 9, 1, 1});
        disconnected.traversals.push_back({18, 9, 8, 1, 1});
        expect_invalid([&] {
            SolutionEulerCircuit(node_count, disconnected).circuit();
        }, "desconectada");

        Solution disconnected_service = ordered_input;
        disconnected_service.service.push_back({17, 8, 9, 1, 1});
        disconnected_service.service.push_back({18, 9, 8, 1, 1});
        expect_invalid([&] {
            SolutionEulerCircuit(node_count, disconnected_service).circuit();
        }, "desconectada");

        expect_invalid([&] { read_instance(2); }, "exactamente un vehiculo");
        const NeutralExpandedGraph instance(read_instance());
        const SuperGraph super_graph(
            instance, instance.get_turns(), instance.get_illegal_turns());
        const std::set<int>& neutral_edges = instance.get_neutral_edges();
        InspectableBestClusterPathSolver solver(
            super_graph, neutral_edges);
        solver.generate_MIP();

        std::set<int> neutral_arc_ids;
        for (const SuperArc& arc : super_graph.arcs()) {
            if (neutral_edges.count(arc.edge_id) != 0)
                neutral_arc_ids.insert(arc.id);
        }
        check(neutral_arc_ids.size() == neutral_edges.size(),
              "Each neutral edge must produce exactly one SuperArc");
        for (int arc_id : neutral_arc_ids) {
            check(std::abs(solver.y_objective_coefficient(arc_id, 1) - 1.0) < 1e-12,
                  "Selected Y variable does not have unit objective cost");
        }
        for (int physical_arc_id = 0; physical_arc_id < 4; ++physical_arc_id) {
            check(std::abs(solver.y_objective_coefficient(physical_arc_id, 1)) < 1e-12,
                  "Physical-edge Y variable leaked into the transition objective");
            check(std::abs(solver.x_objective_coefficient(physical_arc_id, 1)) < 1e-12,
                  "X service variable leaked into the transition objective");
        }

        for (const auto& arc : super_graph.arcs()) {
            if (arc.edge_id == -2) continue;
            const double expected = neutral_edges.count(arc.edge_id) != 0 ? 1 : 0;
            check(std::abs(solver.y_objective_coefficient(arc.id, 1) - expected) < 1e-12,
                  "Objective includes a physical/turn arc or omits an in-out direction");
            if (arc.requested)
                check(solver.x_objective_coefficient(arc.id, 1) == 0, "Objective includes service");
        }

        const SolveResult result = solver.solve(0);
        check(result.has_solution, "Best-cluster-path model is not feasible");
        check(std::abs(result.get_obj_value() - 1.0) < 1e-9,
              "A three-node, two-cluster path must change cluster exactly once");
        long long transition_uses = 0;
        for (const ArcValue<long long>& traversal : result.extract_solution().traversals) {
            if (neutral_arc_ids.count(traversal.id) != 0)
                transition_uses += traversal.value;
        }
        check(transition_uses == 1,
              "The solution did not expose the selected cluster-change Y");

        // Los registros sinteticos (cluster 0) conectan subproblemas, pero no
        // deben asignarse como aristas internas de ninguno de ellos.
        const Solution global_solution = result.extract_solution();
        ClusterBoundaryExtractor boundary_extractor(
            super_graph, instance, global_solution);
        std::map<int, ClusterBoundary> boundaries =
            boundary_extractor.get_boundaries();
        ClusterSubProblemBuilder subproblem_builder(
            instance, super_graph, boundaries);
        std::vector<ClusterProblem> subproblems =
            subproblem_builder.get_subproblems();
        check(subproblems.size() == 2,
              "Synthetic cluster records changed the number of subproblems");
        for (const ClusterProblem& problem : subproblems) {
            check(problem.cluster > 0,
                  "A subproblem was created for synthetic cluster 0");
            check(problem.instance.edges.size() + problem.instance.arcs.size() == 1,
                  "A synthetic connector leaked into a cluster subproblem");
            check(problem.boundary.visits.size() == 1 &&
                      problem.instance.vehicles == 1,
                  "The global circuit was not represented as cluster visits");
            const InstanceEdge& local_record = problem.instance.edges.empty()
                ? problem.instance.arcs.front() : problem.instance.edges.front();
            check(local_record.zone == 1,
                  "The service was not assigned to its global-circuit visit");
        }

        ClusterSolutionMerger solution_merger(
            super_graph,
            instance,
            subproblems,
            boundary_extractor.get_global_arc_by_edge());
        for (ClusterProblem& problem : subproblems) {
            ClusterRCPPSolver subproblem_solver(
                *problem.super_graph, problem.boundary, problem.mapping.local_to_global_node);
            subproblem_solver.generate_MIP();
            const SolveResult subproblem_result = subproblem_solver.solve(0);
            check(subproblem_result.has_solution,
                  "A cluster subproblem is not feasible");
            solution_merger.add_solution(subproblem_result.extract_solution());
        }
        const Solution merged_solution = solution_merger.build_solution();
        check(std::abs(merged_solution.objective - 18.0) < 1e-9,
              "The merged cluster solution has the wrong distance");

        const GraphSolution contracted = instance.contract_solution(
            super_graph.to_graph_solution(merged_solution));
        check(std::abs(contracted.objective - 18.0) < 1e-9 && contracted.service.size() == 2,
              "Contraction did not preserve original distance and service");

        Instance local_instance;
        local_instance.vehicles = 2;
        local_instance.nodes = 4;
        local_instance.deposit_nodes = {0, 1, 2, 3};
        local_instance.arcs.push_back({0, 1, -1, 7, 3});
        local_instance.arcs.push_back({2, 3, -1, 11, 4});
        const Graph local_graph(local_instance);
        const SuperGraph local_super_graph(local_graph, {}, {});
        ClusterBoundary boundary;
        boundary.cluster = 1;
        boundary.visits = {
            {41,
             {ClusterPortalKind::GlobalDeposit, -1, 0, 1, true},
             {ClusterPortalKind::GlobalDeposit, -1, 1, 1, false}, {}, false, {}},
            {42,
             {ClusterPortalKind::NeutralInOut, -1, 2, 1, true},
             {ClusterPortalKind::NeutralInOut, -1, 3, 1, false}, {}, false, {}},
        };

        std::map<int, int> identity_mapping;
        for (int node = 0; node < local_super_graph.nodes_amount(); ++node)
            identity_mapping[node] = node;
        boundary.visits[1].entry.global_virtual_node = local_super_graph.super_arc_with_id(1)->from;
        boundary.visits[1].exit.global_virtual_node = local_super_graph.super_arc_with_id(1)->to;
        InspectableClusterRCPPSolver cluster_solver(local_super_graph, boundary, identity_mapping);
        cluster_solver.generate_MIP();
        const int departure_arc =
            local_super_graph.super_arcs_adj_depo_node().front()->id;
        check(cluster_solver.departure_bounds(departure_arc, 1).second == 1,
              "A cluster visit accepts more than one departure");
        const SolveResult cluster_result = cluster_solver.solve(0);
        check(cluster_result.has_solution,
              "Cluster solver rejected two paired visits");
        check(std::abs(cluster_result.get_obj_value() - 18.0) < 1e-9,
              "Cluster solver did not distribute service among visits");
        std::map<int, std::pair<int, int>> endpoints;
        for (const ArcValue<long long>& traversal :
             cluster_result.extract_solution().deposit_traversals) {
            if (traversal.value <= 0) continue;
            const SuperArc& arc =
                *local_super_graph.super_arc_with_id(traversal.id);
            if (traversal.from == -1)
                endpoints[traversal.vehicle].first =
                    local_super_graph.original_node(arc.to);
            if (traversal.to == -1)
                endpoints[traversal.vehicle].second =
                    local_super_graph.original_node(arc.from);
        }
        check(endpoints[1] == std::pair<int, int>(0, 1) &&
                  endpoints[2] == std::pair<int, int>(2, 3),
              "Cluster solver crossed the A1-A2 and A3-A4 visit pairs");
        return 0;
    } catch (const IloException& error) {
        std::cerr << "CPLEX test failure: " << error << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
    }
    return 1;
}
