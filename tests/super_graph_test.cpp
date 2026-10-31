#include "TestInstance.h"
#include "lib/graph/SuperGraph.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<SuperGraph>);
static_assert(!std::is_copy_assignable_v<SuperGraph>);
static_assert(!std::is_assignable_v<SuperGraph&, SuperGraph&>);
static_assert(std::is_nothrow_move_constructible_v<SuperGraph>);
static_assert(std::is_nothrow_move_assignable_v<SuperGraph>);
static_assert(std::is_nothrow_move_constructible_v<HashMap>);
static_assert(std::is_nothrow_move_assignable_v<HashMap>);
static_assert(!std::is_nothrow_constructible_v<SuperGraph, const Graph&,
              std::vector<Turn>&, std::vector<Turn>&>);

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check_default_graph() {
    SuperGraph graph;
    check(graph.nodes_amount() == 0 && graph.arcs_amount() == 0, "Default counts");
    check(graph.deposit() == -1, "Default graph has no deposit");
    edgeKeySet neighborhood;
    graph.random_edge_neighborhood(neighborhood, 0);
    check(neighborhood.empty(), "Empty graph has no subset");
    check(graph.arcs().empty(), "Default containers");
    check(graph.super_arcs_adj_depo_node().empty() && graph.super_arcs_adj_node_depo().empty(),
          "Default deposit adjacency");
}

auto arc_values(const SuperArc& arc) {
    return std::tie(arc.id, arc.edge_id, arc.requested_idx, arc.from, arc.to,
                    arc.zone, arc.cost, arc.demand, arc.pair, arc.requested,
                    arc.is_neutral);
}

void check_super_arc_neutral_default() {
    const SuperArc ordinary(0, 0, -1, 0, 1, 0, 1, 0, -1);
    const SuperArc neutral(1, 1, -1, 1, 2, 0, 1, 0, -1, true);
    check(!ordinary.is_neutral && neutral.is_neutral,
          "SuperArc neutrality constructor default/override");
}

Instance directed_instance(int nodes, std::vector<InstanceEdge> arcs,
                           std::vector<int> deposit_nodes = {}) {
    Instance instance;
    instance.vehicles = 1;
    instance.nodes = nodes;
    instance.deposit_nodes = std::move(deposit_nodes);
    instance.arcs = std::move(arcs);
    return instance;
}

const SuperArc& physical_arc(const SuperGraph& graph, int edge_id) {
    const auto found = std::find_if(graph.arcs().begin(), graph.arcs().end(),
        [edge_id](const SuperArc& arc) { return arc.edge_id == edge_id; });
    check(found != graph.arcs().end(), "Missing physical super-arc");
    return *found;
}

int connector_count(const SuperGraph& graph, int edge_id, int from, int to) {
    return std::count_if(graph.arcs().begin(), graph.arcs().end(),
        [=](const SuperArc& arc) {
            return arc.edge_id == edge_id && arc.from == from && arc.to == to;
        });
}

class NeutralGraphView : public Graph {
public:
    using Graph::Graph;
    bool is_neutral_edge(int edge_id) const override { return edge_id == 1; }
};

void check_neutral_propagation(const std::string& fixture) {
    const NeutralGraphView graph(test_instance(fixture));
    const SuperGraph super_graph(graph, {}, {});
    for (const SuperArc& arc : super_graph.arcs()) {
        const bool expected = arc.edge_id == 1;
        check(arc.is_neutral == expected,
              "SuperArc must preserve neutral-edge classification");
    }
}

std::vector<int> arc_ids(const SuperGraph& graph, const std::vector<const SuperArc*>& arcs) {
    std::vector<int> ids;
    for (const SuperArc* arc : arcs) {
        check(arc == graph.super_arc_with_id(arc->id), "Adjacency must point into its graph");
        ids.push_back(arc->id);
    }
    return ids;
}

void check_arc_adjacency(const SuperGraph& graph) {
    for (const SuperArc& arc : graph.arcs()) {
        const std::vector<int> outgoing = arc_ids(graph, graph.super_arcs_from(arc.from));
        const std::vector<int> incoming = arc_ids(graph, graph.super_arcs_to(arc.to));
        check(std::find(outgoing.begin(), outgoing.end(), arc.id) != outgoing.end(),
              "Generated arc missing from outgoing adjacency");
        check(std::find(incoming.begin(), incoming.end(), arc.id) != incoming.end(),
              "Generated arc missing from incoming adjacency");
    }
    for (int node = 0; node <= graph.deposit(); ++node) {
        for (const SuperArc* arc : graph.super_arcs_from(node))
            check(arc->from == node, "Outgoing adjacency contains an arc from another node");
        for (const SuperArc* arc : graph.super_arcs_to(node))
            check(arc->to == node, "Incoming adjacency contains an arc to another node");
    }
}

void check_turn_generation() {
    const Instance instance = directed_instance(4, {
        {0, 1, 0, 2, 0},
        {1, 2, 1, 3, 4},
        {2, 3, 0, 5, 0},
    });
    const Graph original(instance);

    // The first turn list is descriptive, not an exhaustive allowlist.
    const SuperGraph all_turns(original, {Turn(3, 2, 1)}, {});
    const SuperArc& first = physical_arc(all_turns, 0);
    const SuperArc& second = physical_arc(all_turns, 1);
    const SuperArc& third = physical_arc(all_turns, 2);
    check(connector_count(all_turns, -1, first.to, second.from) == 1,
          "Connected orientations must generate a turn connector");
    check(connector_count(all_turns, -1, second.to, third.from) == 1,
          "Turn generation must not be restricted to the listed turns");
    check(all_turns.arcs_amount() == 5, "Directed chain must generate exactly two turns");
    for (const SuperArc& arc : all_turns.arcs()) {
        if (arc.edge_id == -1)
            check(arc.zone == 0 && arc.cost == 0 && arc.demand == 0 &&
                  !arc.requested && arc.requested_idx == -1 && arc.pair == -1,
                  "Turn connectors must carry only synthetic attributes");
    }
    check_arc_adjacency(all_turns);

    // Deliberately unsorted: SuperGraph must normalize in-memory prohibitions.
    const std::vector<Turn> illegal = {Turn(1, 2, 3), Turn(0, 1, 2)};
    const SuperGraph prohibited(original, {}, illegal);
    check(prohibited.arcs_amount() == 3,
          "Every prohibited turn must be omitted regardless of input order");
}

void check_non_turn_cases() {
    const Graph original(directed_instance(4, {
        {0, 1, 0, 1, 0},
        {1, 0, 0, 1, 0},
        {2, 3, 0, 1, 0},
    }));
    const SuperGraph graph(original, {}, {});
    check(graph.arcs_amount() == 3,
          "U-turns and orientations at different intersections must not connect");

    Instance undirected;
    undirected.vehicles = 1;
    undirected.nodes = 3;
    undirected.edges = {{0, 1, 0, 2, 0}, {1, 2, 0, 3, 0}};
    const Graph undirected_original(undirected);
    const SuperGraph undirected_graph(undirected_original, {}, {});
    const SuperArc& zero_to_one = *undirected_graph.super_arc_with_id(0);
    const SuperArc& one_to_zero = *undirected_graph.super_arc_with_id(1);
    const SuperArc& one_to_two = *undirected_graph.super_arc_with_id(2);
    const SuperArc& two_to_one = *undirected_graph.super_arc_with_id(3);
    check(undirected_graph.arcs_amount() == 6,
          "Two incident undirected edges must generate two directional turns");
    check(connector_count(undirected_graph, -1, zero_to_one.to, one_to_two.from) == 1 &&
          connector_count(undirected_graph, -1, two_to_one.to, one_to_zero.from) == 1,
          "Undirected orientations must connect in both travel directions");
    check(connector_count(undirected_graph, -1, zero_to_one.to, one_to_zero.from) == 0,
          "Paired reverse orientations must not generate a U-turn");
}

class NoDepositAccessGraph : public Graph {
public:
    using Graph::Graph;
    bool allows_deposit_access(int) const override { return false; }
};

void check_deposit_generation() {
    const Instance instance = directed_instance(2, {{0, 1, 0, 7, 0}}, {0, 1});
    const Graph original(instance);
    const SuperGraph graph(original, {}, {});
    const SuperArc& physical = physical_arc(graph, 0);
    const int deposit = graph.deposit();

    check(graph.super_arcs_adj_depo_node().size() == 1 &&
          connector_count(graph, -2, deposit, physical.from) == 1,
          "Deposit-adjacent origin must generate one departure connector");
    check(graph.super_arcs_adj_node_depo().size() == 1 &&
          connector_count(graph, -2, physical.to, deposit) == 1,
          "Deposit-adjacent destination must generate one arrival connector");
    check(connector_count(graph, -2, deposit, physical.to) == 0 &&
          connector_count(graph, -2, physical.from, deposit) == 0,
          "Deposit connectors must respect the physical orientation");
    check(graph.is_adj_depo_node(physical.from) && graph.is_adj_node_depo(physical.to),
          "Deposit adjacency flags must match generated connectors");
    for (const SuperArc& arc : graph.arcs()) {
        if (arc.edge_id == -2)
            check(arc.zone == 0 && arc.cost == 0 && arc.demand == 0 &&
                  !arc.requested && arc.requested_idx == -1 && arc.pair == -1,
                  "Deposit connectors must carry only synthetic attributes");
    }
    check_arc_adjacency(graph);

    const NoDepositAccessGraph restricted(instance);
    const SuperGraph without_access(restricted, {}, {});
    check(without_access.arcs_amount() == 1 &&
          without_access.super_arcs_adj_depo_node().empty() &&
          without_access.super_arcs_adj_node_depo().empty(),
          "Graph policy must be able to suppress every deposit connector");
}

void check_loops_and_empty_graph() {
    Instance loops;
    loops.vehicles = 1;
    loops.nodes = 1;
    loops.deposit_nodes = {0};
    loops.edges = {{0, 0, 0, 2, 0}};
    loops.arcs = {{0, 0, 0, 3, 0}};
    const Graph loop_original(loops);
    const SuperGraph loop_graph(loop_original, {}, {});
    check(loop_graph.arcs_amount() == 9,
          "Three loop orientations must generate six deposit connectors and no turns");
    check(loop_graph.super_arcs_adj_depo_node().size() == 3 &&
          loop_graph.super_arcs_adj_node_depo().size() == 3,
          "Each loop orientation must remain reachable from and to the deposit");
    check_arc_adjacency(loop_graph);

    Instance empty;
    empty.vehicles = 1;
    empty.nodes = 1;
    const Graph empty_original(empty);
    const SuperGraph empty_graph(empty_original, {}, {});
    check(empty_graph.nodes_amount() == 0 && empty_graph.arcs_amount() == 0 &&
          empty_graph.deposit() == 0,
          "A valid graph without records must generate an empty expanded graph");

    bool rejected = false;
    try {
        const Graph unbuilt;
        const SuperGraph invalid(unbuilt, {}, {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "An unbuilt original graph must be rejected");
}

struct Snapshot {
    int nodes, count, deposit;
    std::vector<SuperArc> arcs;
    std::vector<edgeKeySet> neighborhoods;
    std::vector<std::vector<int>> incoming, outgoing;
    std::vector<int> from_deposit, to_deposit;
    std::vector<bool> adjacent_from_deposit, adjacent_to_deposit;

    explicit Snapshot(const SuperGraph& graph)
        : nodes(graph.nodes_amount()), count(graph.arcs_amount()), deposit(graph.deposit()),
          arcs(graph.arcs()),
          from_deposit(arc_ids(graph, graph.super_arcs_adj_depo_node())),
          to_deposit(arc_ids(graph, graph.super_arcs_adj_node_depo())) {
        for (int node = 0; node < nodes; ++node) {
            edgeKeySet neighborhood;
            graph.add_edge_neighborhood(neighborhood, node, 3, graph.arcs().size());
            neighborhoods.push_back(std::move(neighborhood));
        }
        // Include the synthetic deposit, whose index follows the virtual nodes.
        for (int node = 0; node <= nodes; ++node) {
            incoming.push_back(arc_ids(graph, graph.super_arcs_from(node)));
            outgoing.push_back(arc_ids(graph, graph.super_arcs_to(node)));
            adjacent_from_deposit.push_back(graph.is_adj_depo_node(node));
            adjacent_to_deposit.push_back(graph.is_adj_node_depo(node));
        }
    }
};

void check_snapshot(const SuperGraph& graph, const Snapshot& expected) {
    const Snapshot actual(graph);
    check(actual.nodes == expected.nodes && actual.count == expected.count &&
          actual.deposit == expected.deposit, "Move must preserve counts and deposit");
    check(actual.arcs.size() == expected.arcs.size(), "Move must preserve all arcs");
    for (std::size_t i = 0; i < actual.arcs.size(); ++i)
        check(arc_values(actual.arcs[i]) == arc_values(expected.arcs[i]),
              "Move must preserve arc attributes and pairs");
    check(actual.incoming == expected.incoming && actual.outgoing == expected.outgoing,
          "Move must preserve node adjacency maps");
    check(actual.from_deposit == expected.from_deposit && actual.to_deposit == expected.to_deposit &&
          actual.adjacent_from_deposit == expected.adjacent_from_deposit &&
          actual.adjacent_to_deposit == expected.adjacent_to_deposit,
          "Move must preserve deposit adjacency");
    check(actual.neighborhoods == expected.neighborhoods, "Move must preserve BFS adjacency");
}

void check_moves(const std::string& fixture) {
    Graph graph(test_instance(fixture));
    Graph small_graph(test_instance(std::filesystem::path(fixture).parent_path() / "feasible.dat"));
    std::vector<Turn> turns, illegal_turns;
    SuperGraph reference(graph, turns, illegal_turns);
    const Snapshot expected(reference);

    SuperGraph destination(small_graph, turns, illegal_turns);
    check(destination.deposit() != expected.deposit, "Exercise replacement of different graph data");
    {
        SuperGraph source(graph, turns, illegal_turns);
        SuperGraph moved(std::move(source));
        check_snapshot(moved, expected);

        source = SuperGraph(graph, turns, illegal_turns);
        check_snapshot(source, expected);
        check_snapshot(moved, expected);

        destination = std::move(moved);
        check_snapshot(destination, expected);
        moved = SuperGraph(graph, turns, illegal_turns);
        check_snapshot(moved, expected);
    }
    // Both moved-from objects have been reassigned and destroyed.
    check_snapshot(destination, expected);

    SuperGraph initially_unbuilt;
    initially_unbuilt = std::move(destination);
    check_snapshot(initially_unbuilt, expected);
}

void check_super_graph(const std::string& fixture, int undirected_count) {
    Graph graph(test_instance(fixture));
    std::vector<Turn> turns, illegal_turns;
    SuperGraph super_graph(graph, turns, illegal_turns);
    int super_id = 0;
    for (int edge_id = 0; edge_id < 4; ++edge_id) {
        const Edge& edge = *graph.edge_with_id(edge_id);
        const int orientations = edge_id < undirected_count ? 2 : 1;
        for (int orientation = 0; orientation < orientations; ++orientation) {
            const SuperArc& arc = *super_graph.super_arc_with_id(super_id);
            check(arc.id == super_id, "Super-arc ID");
            check(arc.edge_id == edge_id, "Super-arc must preserve original global ID");
            check(arc.from == 2 * super_id && arc.to == 2 * super_id + 1, "Virtual endpoints");
            const int expected_from = orientation == 0 ? edge.from : edge.to;
            const int expected_to = orientation == 0 ? edge.to : edge.from;
            check(super_graph.original_node(arc.from) == expected_from &&
                  super_graph.original_node(arc.to) == expected_to,
                  "Virtual endpoints must preserve each original orientation");
            check(arc.zone == edge.zone && arc.cost == edge.cost && arc.demand == edge.demand,
                  "Original attributes must survive transformation");
            check(arc.requested == edge.requested && arc.requested_idx == edge.requested_idx,
                  "Required index must survive transformation");
            const int pair = orientations == 1 ? -1 : super_id + (orientation == 0 ? 1 : -1);
            check(arc.pair == pair, "Orientation pair");
            if (pair >= 0) {
                const SuperArc& reverse = *super_graph.super_arc_with_id(pair);
                check(reverse.pair == super_id && reverse.edge_id == edge_id,
                      "Reciprocal pair must share original global ID");
            }
            ++super_id;
        }
    }
    int turns_count = 0, deposit_count = 0;
    for (const SuperArc& arc : super_graph.arcs()) {
        if (arc.id < super_id) continue;
        const bool deposit_arc = arc.from == super_graph.deposit() || arc.to == super_graph.deposit();
        check(arc.edge_id == (deposit_arc ? -2 : -1), "Connector sentinel");
        check(!arc.requested && !arc.is_neutral && arc.requested_idx == -1 && arc.pair == -1,
              "Connectors must not refer to required edges or orientation pairs");
        if (deposit_arc) ++deposit_count;
        else ++turns_count;
    }
    const auto from_deposit = super_graph.super_arcs_adj_depo_node();
    const auto to_deposit = super_graph.super_arcs_adj_node_depo();
    for (const SuperArc* arc : from_deposit)
        check(arc->edge_id == -2 && arc->from == super_graph.deposit() &&
              arc->to != super_graph.deposit(), "Deposit departure connector");
    for (const SuperArc* arc : to_deposit)
        check(arc->edge_id == -2 && arc->from != super_graph.deposit() &&
              arc->to == super_graph.deposit(), "Deposit arrival connector");
    check(static_cast<int>(from_deposit.size() + to_deposit.size()) == deposit_count,
          "Deposit adjacency must expose every deposit connector");
    check(turns_count > 0 && deposit_count == 2 * super_id, "Exercise both connector types");
}

void check_edge_subset(const std::string& fixture) {
    const SuperGraph graph = test_super_graph(test_instance(fixture));
    const int n = graph.nodes_amount();
    const std::size_t unlimited = graph.arcs().size();
    // Independent shortest-path oracle: unit costs, no paths through the deposit.
    std::vector<std::vector<int>> distance(n, std::vector<int>(n, n + 1));
    for (int u = 0; u < n; ++u) distance[u][u] = 0;
    for (const SuperArc& arc : graph.arcs())
        if (arc.from != graph.deposit() && arc.to != graph.deposit())
            distance[arc.from][arc.to] = 1;
    for (int k = 0; k < n; ++k)
        for (int u = 0; u < n; ++u)
            for (int v = 0; v < n; ++v)
                distance[u][v] = std::min(distance[u][v], distance[u][k] + distance[k][v]);

    for (int start = 0; start < n; ++start) {
        for (int d : {0, 1, 2, n}) {
            std::set<EdgeKey> expected;
            for (const SuperArc& arc : graph.arcs()) {
                if (arc.edge_id == -2) {
                    if (arc.from == graph.deposit() && distance[start][arc.to] < d)
                        expected.emplace(-1, arc.to);
                    else if (arc.to == graph.deposit() && distance[start][arc.from] < d)
                        expected.emplace(arc.from, -1);
                } else if (distance[start][arc.from] <= d && distance[start][arc.to] <= d) {
                    expected.emplace(arc.from, arc.to);
                }
            }

            edgeKeySet subset;
            graph.add_edge_neighborhood(subset, start, d, unlimited);
            check(std::set<EdgeKey>(subset.begin(), subset.end()) == expected,
                  "Neighborhood must contain every arc induced by the BFS radius");
        }
    }

    edgeKeySet random_subset;
    graph.random_edge_neighborhood(random_subset, 0);
    check(random_subset.empty(), "Radius zero has no arcs");
}

void check_cycle_and_deposit_neighborhood() {
    Instance instance;
    instance.vehicles = 1;
    instance.nodes = 3;
    instance.deposit_nodes = {0};
    instance.arcs = {{0, 1, 0, 1, 0}, {1, 2, 0, 1, 0}, {2, 0, 0, 1, 0}};
    const SuperGraph graph = test_super_graph(instance);

    const int start = graph.super_arc_with_id(0)->from;
    edgeKeySet subset;
    graph.add_edge_neighborhood(subset, start, graph.nodes_amount(), graph.arcs().size());
    const std::set<EdgeKey> neighborhood(subset.begin(), subset.end());

    for (const SuperArc& arc : graph.arcs()) {
        EdgeKey key{arc.from, arc.to};
        if (arc.from == graph.deposit()) key.first = -1;
        if (arc.to == graph.deposit()) key.second = -1;
        check(neighborhood.count(key) == 1,
              "Full-radius neighborhood must preserve cycles and deposit connectors");
    }

    for (const std::size_t limit : {std::size_t{0}, std::size_t{1},
                                    graph.arcs().size() / 2, graph.arcs().size()}) {
        edgeKeySet limited;
        graph.add_edge_neighborhood(limited, start, graph.nodes_amount(), limit);
        check(limited.size() <= limit, "BFS neighborhood exceeded its strict arc limit");
        for (const EdgeKey& edge : limited)
            check(neighborhood.count(edge) == 1,
                  "Limited BFS neighborhood contains an arc outside the full neighborhood");
    }
}

void check_graph_solution_projection() {
    Instance instance;
    instance.vehicles = 1;
    instance.nodes = 3;
    instance.deposit_nodes = {0};
    instance.edges = {{0, 1, 1, 2, 1}};
    instance.arcs = {{1, 2, 0, 3, 0}};

    const NeutralGraphView graph(instance);
    const SuperGraph super_graph(graph, {}, {});
    Solution solution;
    solution.objective = 13;

    const SuperArc& forward = *super_graph.super_arc_with_id(0);
    const SuperArc& reverse = *super_graph.super_arc_with_id(1);
    const SuperArc& directed = *super_graph.super_arc_with_id(2);
    check(directed.is_neutral,
          "Graph projection test must exercise a neutral physical edge");
    solution.service.emplace_back(forward, 1);
    solution.service.emplace_back(reverse, 2);
    solution.traversals.emplace_back(forward, 3);
    solution.traversals.emplace_back(directed, 4);

    for (const SuperArc& arc : super_graph.arcs()) {
        if (arc.edge_id == -1) solution.traversals.emplace_back(arc, 5);
        if (arc.edge_id == -2)
            solution.deposit_traversals.emplace_back(arc, 6);
    }

    const GraphSolution projected = super_graph.to_graph_solution(solution);
    check(projected.objective == 13, "Graph projection must preserve the objective");
    check(projected.service == std::map<int, long long>({{0, 3}}),
          "Graph projection must combine both orientations of one edge");
    check(projected.traversals == std::map<int, long long>({{0, 3}, {1, 4}}),
          "Graph projection must retain only physical traversal multiplicities");
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: super_graph_test <fixture> <undirected_count>");
        check_super_arc_neutral_default();
        check_turn_generation();
        check_non_turn_cases();
        check_deposit_generation();
        check_loops_and_empty_graph();
        check_neutral_propagation(argv[1]);
        check_default_graph();
        check_graph_solution_projection();
        check_cycle_and_deposit_neighborhood();
        check_edge_subset(argv[1]);
        check_super_graph(argv[1], std::stoi(argv[2]));
        check_moves(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
