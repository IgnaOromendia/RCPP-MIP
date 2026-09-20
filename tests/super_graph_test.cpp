#include "TestInstance.h"
#include "lib/graph/SuperGraph.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
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
    check(graph.edge_subset(0).empty(), "Empty graph has no subset");
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

    for (int d : {0, 1, 2, n}) {
        const auto subset = graph.edge_subset(d);
        if (d == 0) {
            check(subset.empty(), "Radius zero has no discovery edges");
            continue;
        }
        if (subset.empty()) {
            // The random root is not exposed when it has no outgoing edges.
            bool has_sink = false;
            for (int u = 0; u < n; ++u) {
                bool reaches_other = false;
                for (int v = 0; v < n; ++v)
                    if (v != u && distance[u][v] <= d) reaches_other = true;
                if (!reaches_other) has_sink = true;
            }
            check(has_sink, "Empty subset requires a possible sink root");
            continue;
        }
        const int start = subset.front().first;
        check(start >= 0 && start < n, "Root is a virtual node");
        std::vector<bool> included(n, false);
        included[start] = true;
        int previous_distance = 0;
        for (const auto& [u, v] : subset) {
            check(u >= 0 && u < n && v >= 0 && v < n, "Endpoints exclude deposit");
            check(std::any_of(graph.arcs().begin(), graph.arcs().end(),
                             [from = u, to = v](const SuperArc& arc) {
                                 return arc.from == from && arc.to == to;
                             }), "Subset contains existing directed arcs");
            check(included[u], "Discovery edge starts at an already reached node");
            check(!included[v], "Each edge discovers a new node");
            check(distance[start][v] == distance[start][u] + 1,
                  "Discovery edge follows a shortest path");
            included[v] = true;
            check(distance[start][v] >= previous_distance, "Subset is in BFS order");
            previous_distance = distance[start][v];
        }
        for (int v = 0; v < n; ++v)
            check(included[v] == (distance[start][v] <= d), "Subset matches BFS radius");
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: super_graph_test <fixture> <undirected_count>");
        check_default_graph();
        check_edge_subset(argv[1]);
        check_super_graph(argv[1], std::stoi(argv[2]));
        check_moves(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
