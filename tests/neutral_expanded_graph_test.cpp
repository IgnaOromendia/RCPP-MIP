#include <graph/NeutralExpandedGraph.h>
#include <graph/SuperGraph.h>
#include <io/ClusterAssignmentReader.h>
#include <io/InstanceReader.h>
#include "ThreeClusterInstance.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>

static_assert(std::has_virtual_destructor_v<Graph>);

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Action>
void expect_invalid(Action action, const std::string& fragment) {
    try {
        action();
    } catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos,
              "Unexpected validation diagnostic: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Expected invalid_argument");
}

class ExpandedGraphView : public NeutralExpandedGraph {
public:
    using NeutralExpandedGraph::NeutralExpandedGraph;

    const std::vector<Edge>& records() const { return _all_edges; }
};

void check_reader_validation() {
    auto read_bad = [](const std::string& rows,
                       const std::string& nodes = "node_id reasons\n") {
        std::istringstream assignments(
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n" + rows);
        std::istringstream neutrals(nodes);
        Instance instance;
        instance.nodes = 2;
        instance.edges = {{0, 1, 1, 1, 1}};
        ClusterAssignmentReader::read(instance, assignments, &neutrals);
    };

    expect_invalid([&] { read_bad(""); }, "faltan");
    expect_invalid([&] {
        read_bad("1 1 1 2 0 0 0 0\n1 1 1 2 0 0 0 0\n");
    }, "duplicado");
    expect_invalid([&] { read_bad("1 1 2 1 0 0 0 0\n"); }, "extremos");
    expect_invalid([&] { read_bad("1 0 1 2 0 0 0 0\n"); }, "positivo");
    expect_invalid([&] { read_bad("1 1 1 2 0 1 0 0\n"); }, "cero");
    expect_invalid([&] {
        read_bad("1 1 1 2 0 0 0 0\n", "node_id reasons\n2 street\n2 street\n");
    }, "duplicado");
    expect_invalid([&] {
        read_bad("1 1 1 2 0 0 0 0\n", "node_id reasons\n3 street\n");
    }, "rango");
}

Instance read_fixture(const std::filesystem::path& fixtures) {
    std::ifstream graph_file(fixtures / "neutral_binary.dat");
    std::istringstream no_turns("0 0\n");
    Instance input = InstanceReader::read(graph_file, no_turns);
    std::ifstream clusters(fixtures / "neutral_binary.clusters.dat");
    std::ifstream neutral(fixtures / "neutral_binary.neutral.dat");
    ClusterAssignmentReader::read(input, clusters, &neutral);
    return input;
}

void check_fixture_reader(const std::filesystem::path& fixtures,
                          const Instance& input) {
    check(input.record_clusters == std::vector<int>({1, 2, 2, 2}) &&
              input.neutral_nodes == std::vector<int>({1, 2, 3}),
          "Reader indexing");

    const Instance from_files(
        (fixtures / "neutral_binary.dat").string(),
        (fixtures / "turns.dat").string(),
        (fixtures / "neutral_binary.clusters.dat").string(),
        (fixtures / "neutral_binary.neutral.dat").string());
    check(from_files.nodes == input.nodes &&
              from_files.edges.size() == input.edges.size() &&
              from_files.arcs.size() == input.arcs.size() &&
              from_files.record_clusters == input.record_clusters &&
              from_files.neutral_nodes == input.neutral_nodes,
          "Instance constructor did not read graph, clusters and neutral nodes");

    const Instance without_neutrals(
        (fixtures / "neutral_binary.dat").string(),
        (fixtures / "turns.dat").string(),
        (fixtures / "neutral_binary.clusters.dat").string());
    check(without_neutrals.neutral_nodes.empty(),
          "Default neutral path must declare no neutral nodes");
}

void check_expansion(const Instance& input) {
    const ExpandedGraphView expanded(input);
    const auto& records = expanded.records();

    check(expanded.nodes_amount() == 6 && expanded.deposit() == 6,
          "Only nodes shared by multiple clusters must be expanded");
    check(expanded.original_cluster_costs().at(1) == 1.25 &&
              expanded.original_cluster_costs().at(2) == 10.75,
          "Original cluster costs must preserve decimals");
    check(expanded.get_neutral_edges().size() == 2,
          "Two clusters must produce two directed crossing records");
    check(records.size() == 8 && expanded.edges_amount() == 8,
          "Unexpected expanded record count");
    check(expanded.requested_amount() == 3,
          "Expansion must not duplicate required service");

    for (int id : expanded.get_neutral_edges()) {
        const Edge& crossing = records.at(id);
        check(expanded.is_neutral_edge(id) && expanded.edge_cluster(id) == 0,
              "Crossing record must be neutral and belong to synthetic cluster zero");
        check(!crossing.is_bidirectional && !crossing.requested &&
                  crossing.demand == 0,
              "Crossing record must be a traversal-only directed arc");
    }

    check(records[0].from == 0 && records[0].to == 1 &&
              records[1].from == 5 && records[1].to == 2 &&
              records[2].from == 2 && records[2].to == 3 &&
              records[3].from == 3 && records[3].to == 4,
          "Physical records must use their cluster representations exactly once");

    const SuperGraph super_graph(
        expanded, expanded.get_turns(), expanded.get_illegal_turns());
    for (int record_id : expanded.get_neutral_edges()) {
        const int orientations = std::count_if(
            super_graph.arcs().begin(), super_graph.arcs().end(),
            [record_id](const SuperArc& arc) { return arc.edge_id == record_id; });
        check(orientations == 1,
              "Each directed neutral record must produce exactly one SuperArc");
    }

    GraphSolution projected;
    projected.objective = 42;
    projected.service = {{0, 1}, {6, 9}};
    projected.traversals = {{1, 3}, {2, 5}, {4, 9}, {7, 9}};
    const GraphSolution contracted = expanded.contract_solution(projected);
    check(contracted.objective == 42,
          "Contraction must preserve the objective");
    check(contracted.service == std::map<int, long long>({{0, 1}}),
          "Contraction must omit synthetic service");
    check(contracted.traversals ==
              std::map<int, long long>({{1, 3}, {2, 5}}),
          "Contraction must omit synthetic traversals");

    Instance duplicate = input;
    duplicate.neutral_nodes.push_back(1);
    expect_invalid([&] { NeutralExpandedGraph rejected(duplicate); }, "duplicado");
}

void check_multiple_cluster_expansion() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 5;
    input.edges = {
        {0, 1, 1, 1, 1},
        {0, 2, 1, 2, 1},
        {0, 3, 1, 3, 1},
        {0, 4, 1, 4, 1},
    };
    input.record_clusters = {1, 2, 3, 1};
    input.neutral_nodes = {0};
    input.turns = {{1, 0, 4}, {1, 0, 2}};
    input.illegal_turns = input.turns;

    const ExpandedGraphView expanded(input);
    const auto& records = expanded.records();

    check(expanded.nodes_amount() == 7 && expanded.deposit() == 7,
          "Three clusters must create two additional representations");
    check(expanded.get_neutral_edges().size() == 6,
          "Three clusters must create six directed crossings");
    check(records.size() == input.edges.size() + 6,
          "Expansion must not copy physical records");
    check(records[0].from == 0 && records[1].from == 5 &&
              records[2].from == 6 && records[3].from == 0,
          "Each physical record must use the representation of its cluster");

    const std::set<std::tuple<int, int, int>> expected{{1, 0, 4}};
    auto triples = [](const vector<Turn>& turns) {
        std::set<std::tuple<int, int, int>> result;
        for (const Turn& turn : turns)
            result.emplace(turn.v, turn.w, turn.u);
        return result;
    };
    check(triples(expanded.get_turns()) == expected &&
              triples(expanded.get_illegal_turns()) == expected,
          "Turns must only use representations with both physical connections");
    check(expanded.get_illegal_ext_turns().size() == 1 &&
              expanded.get_illegal_ext_turns().count(ExtendedTurn(1, 0, 5, 2)) == 1,
          "Cross-cluster prohibition must preserve both middle representations");
}

void check_three_cluster_deposit_access() {
    Instance input = three_cluster_cycles();
    input.deposit_nodes = {0};
    const ExpandedGraphView expanded(input);
    const SuperGraph super_graph(
        expanded, expanded.get_turns(), expanded.get_illegal_turns());
    std::set<int> departures, arrivals;
    for (const SuperArc* connector : super_graph.super_arcs_adj_depo_node())
        departures.insert(connector->to);
    for (const SuperArc* connector : super_graph.super_arcs_adj_node_depo())
        arrivals.insert(connector->from);

    for (const SuperArc& arc : super_graph.arcs()) {
        if (arc.edge_id < 0 || arc.edge_id >= 9) continue;
        const InstanceEdge& original = input.arcs[arc.edge_id];
        if (original.from == 0)
            check(departures.count(arc.from) == 1,
                  "A neutral representation cannot depart from the deposit");
        if (original.to == 0)
            check(arrivals.count(arc.to) == 1,
                  "A neutral representation cannot return to the deposit");
    }
}

void check_three_cluster_physical_records() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 6;
    input.edges = {{0, 1, 1, 2.5, 3}, {0, 2, 1, 1, 1},
                   {0, 3, 1, 2, 1}, {1, 4, 1, 3, 1}, {1, 5, 1, 4, 1}};
    input.arcs = {{1, 0, 0, 0.75, 0}};
    input.record_clusters = {2, 1, 3, 1, 3, 2};
    input.neutral_nodes = {0, 1};
    const ExpandedGraphView expanded(input);
    check(expanded.get_neutral_edges().size() == 12 &&
              expanded.records().size() == 18 && expanded.requested_amount() == 5,
          "Two three-cluster neutral nodes must not duplicate physical records");

    GraphSolution projected;
    for (int id = 0; id < 6; ++id) {
        const InstanceEdge& original = id < 5 ? input.edges[id] : input.arcs[0];
        const Edge& record = expanded.records()[id];
        check(record.id == id && record.cost == original.cost &&
                  record.demand == original.demand && record.zone == original.zone &&
                  record.requested == (original.zone != 0) &&
                  record.is_bidirectional == (id < 5) &&
                  expanded.edge_cluster(id) == input.record_clusters[id],
              "Physical record metadata changed during three-cluster expansion");
        projected.traversals[id] = id + 2;
        if (record.requested) projected.service[id] = 1;
    }
    const Edge& shared = expanded.records()[0];
    const Edge& reverse = expanded.records()[5];
    check(shared.from == reverse.to && shared.to == reverse.from &&
              shared.from != 0 && shared.to != 1,
          "Both neutral endpoints must use their cluster-2 representation");

    const SuperGraph super_graph(
        expanded, expanded.get_turns(), expanded.get_illegal_turns());
    std::map<int, int> orientations;
    for (const SuperArc& arc : super_graph.arcs()) {
        if (arc.edge_id < 0 || arc.edge_id >= 6) continue;
        ++orientations[arc.edge_id];
        if (arc.edge_id == 5) {
            check(arc.pair == -1, "Directed physical arc acquired a reverse pair");
        } else {
            const SuperArc& pair = *super_graph.super_arc_with_id(arc.pair);
            check(pair.pair == arc.id && pair.edge_id == arc.edge_id &&
                      super_graph.original_node(arc.from) == super_graph.original_node(pair.to) &&
                      super_graph.original_node(arc.to) == super_graph.original_node(pair.from),
                  "Undirected orientations lost their physical ID or inverse pair");
        }
    }
    check(orientations == std::map<int, int>({{0, 2}, {1, 2}, {2, 2},
                                            {3, 2}, {4, 2}, {5, 1}}),
          "Mixed expansion changed physical orientation counts");

    const auto physical_service = projected.service;
    const auto physical_traversals = projected.traversals;
    for (int id : expanded.get_neutral_edges()) {
        projected.service[id] = 7;
        projected.traversals[id] = 11;
    }
    const GraphSolution contracted = expanded.contract_solution(projected);
    check(contracted.service == physical_service &&
              contracted.traversals == physical_traversals,
          "Three-cluster contraction lost or duplicated physical multiplicities");
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: neutral_expanded_graph_test <fixtures>");
        const std::filesystem::path fixtures = std::filesystem::absolute(argv[1]);
        check_reader_validation();
        const Instance input = read_fixture(fixtures);
        check_fixture_reader(fixtures, input);
        check_expansion(input);
        check_multiple_cluster_expansion();
        check_three_cluster_deposit_access();
        check_three_cluster_physical_records();
        std::cout << "PASS neutral expansion, reader and contraction\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
