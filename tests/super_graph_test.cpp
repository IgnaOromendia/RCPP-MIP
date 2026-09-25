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
    check(graph.random_edge_neighborhood(0).empty(), "Empty graph has no subset");
    check(graph.arcs().empty() && graph.deposit_dist().empty(), "Default containers");
    check(graph.super_arcs_adj_depo_node().empty() && graph.super_arcs_adj_node_depo().empty(),
          "Default deposit adjacency");
}

auto arc_values(const SuperArc& arc) {
    return std::tie(arc.id, arc.edge_id, arc.requested_idx, arc.from, arc.to,
                    arc.zone, arc.cost, arc.demand, arc.pair, arc.requested);
}

std::vector<int> arc_ids(const SuperGraph& graph, const std::vector<const SuperArc*>& arcs) {
    std::vector<int> ids;
    for (const SuperArc* arc : arcs) {
        check(arc == graph.super_arc_with_id(arc->id), "Adjacency must point into its graph");
        ids.push_back(arc->id);
    }
    return ids;
}

struct Snapshot {
    int nodes, count, deposit;
    std::vector<SuperArc> arcs;
    std::vector<double> distances;
    std::vector<std::vector<int>> incoming, outgoing;
    std::vector<int> from_deposit, to_deposit;
    std::vector<bool> adjacent_from_deposit, adjacent_to_deposit;

    explicit Snapshot(const SuperGraph& graph)
        : nodes(graph.nodes_amount()), count(graph.arcs_amount()), deposit(graph.deposit()),
          arcs(graph.arcs()), distances(graph.deposit_dist()),
          from_deposit(arc_ids(graph, graph.super_arcs_adj_depo_node())),
          to_deposit(arc_ids(graph, graph.super_arcs_adj_node_depo())) {
        // Include the synthetic deposit, whose index follows the virtual nodes.
        for (int node = 0; node <= nodes; ++node) {
            incoming.push_back(arc_ids(graph, graph.super_arcs_from(node)));
            outgoing.push_back(arc_ids(graph, graph.super_arcs_to(node)));
            adjacent_from_deposit.push_back(graph.is_adj_depo_node(node));
            adjacent_to_deposit.push_back(graph.is_adj_node_depo(node));
        }
    }
};

void check_snapshot(SuperGraph& graph, const Snapshot& expected) {
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
    check(actual.distances == expected.distances, "Move must transfer cached distances");
    graph.calculate_depo_dists();
    check(graph.deposit_dist() == expected.distances, "Move must preserve adjacency for Dijkstra");
}

void check_moves(const std::string& fixture) {
    Graph graph(test_instance(fixture));
    Graph small_graph(test_instance(std::filesystem::path(fixture).parent_path() / "feasible.dat"));
    std::vector<Turn> turns, illegal_turns;
    SuperGraph reference(graph, turns, illegal_turns);
    reference.calculate_depo_dists();
    const Snapshot expected(reference);
    check(!expected.distances.empty(), "Exercise a populated distance cache");

    SuperGraph destination(small_graph, turns, illegal_turns);
    destination.calculate_depo_dists();
    check(destination.deposit() != expected.deposit, "Exercise replacement of different graph data");
    {
        SuperGraph source(graph, turns, illegal_turns);
        source.calculate_depo_dists();
        SuperGraph moved(std::move(source));
        check_snapshot(moved, expected);

        source = SuperGraph(graph, turns, illegal_turns);
        source.calculate_depo_dists();
        check_snapshot(source, expected);
        check_snapshot(moved, expected);

        destination = std::move(moved);
        check_snapshot(destination, expected);
        moved = SuperGraph(graph, turns, illegal_turns);
        moved.calculate_depo_dists();
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
        check(!arc.requested && arc.requested_idx == -1 && arc.pair == -1,
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

            const auto subset = graph.bfs_tree(start, d, unlimited);
            check(std::set<EdgeKey>(subset.begin(), subset.end()) == expected,
                  "Neighborhood must contain every arc induced by the BFS radius");
        }
    }

    check(graph.random_edge_neighborhood(0).empty(), "Radius zero has no arcs");
}

void check_cycle_and_deposit_neighborhood() {
    Instance instance;
    instance.vehicles = 1;
    instance.nodes = 3;
    instance.deposit_nodes = {0};
    instance.arcs = {{0, 1, 0, 1, 0}, {1, 2, 0, 1, 0}, {2, 0, 0, 1, 0}};
    const SuperGraph graph = test_super_graph(instance);

    const int start = graph.super_arc_with_id(0)->from;
    const auto subset = graph.bfs_tree(start, graph.nodes_amount(), graph.arcs().size());
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
        const auto limited = graph.bfs_tree(start, graph.nodes_amount(), limit);
        check(limited.size() <= limit, "BFS neighborhood exceeded its strict arc limit");
        for (const EdgeKey& edge : limited)
            check(neighborhood.count(edge) == 1,
                  "Limited BFS neighborhood contains an arc outside the full neighborhood");
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: super_graph_test <fixture> <undirected_count>");
        check_default_graph();
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
