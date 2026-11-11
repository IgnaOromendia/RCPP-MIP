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
              expanded.records().size() == 22 && expanded.requested_amount() == 5,
          "Two three-cluster neutral nodes must copy both shared physical records");

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

void check_six_node_boundary_expansion() {
    // External IDs and the before/after graph: fixtures/neutral_six_nodes.md.
    Instance input;
    input.vehicles = 1;
    input.nodes = 6;
    input.deposit_nodes = {0};
    input.edges = {{2, 0, 1, 1, 1}, {0, 1, 1, 2, 1}, {1, 3, 1, 3, 1},
                   {2, 4, 1, 4, 1}, {4, 5, 1, 5, 1}, {5, 3, 1, 6, 1},
                   {2, 3, 1, 7, 1}};
    input.record_clusters = {1, 1, 1, 2, 2, 2, 1};
    input.neutral_nodes = {2, 3};
    const Graph original(input);
    check(original.nodes_amount() == 6, "The fixture must start with six physical nodes");

    const ExpandedGraphView expanded(input);
    check(expanded.nodes_amount() == 8 && expanded.deposit() == 8,
          "Two shared neutrals must expand into four representations and eight physical nodes");
    check(expanded.neutral_boundary()[2] == std::set<int>{1, 2} &&
              expanded.neutral_boundary()[3] == std::set<int>{1, 2},
          "Both original neutrals must touch both clusters");
    const std::vector<EdgeKey> endpoints{
        {2, 0}, {0, 1}, {1, 3}, {6, 4}, {4, 5}, {5, 7}, {2, 3}};
    for (size_t id = 0; id < input.edges.size(); ++id) {
        const Edge& edge = expanded.records()[id];
        check(EdgeKey(edge.from, edge.to) == endpoints[id] &&
                  expanded.edge_cluster(id) == input.record_clusters[id] &&
                  edge.is_bidirectional && edge.zone == 1 && edge.demand == 1 &&
                  edge.cost == input.edges[id].cost,
              "Original streets must retain service and use their cluster's neutral representations");
    }

    std::set<int> neutral_representations;
    std::set<EdgeKey> crossings;
    int copy_id = -1;
    int physical_count = 0;
    for (const Edge& edge : expanded.records()) {
        if (expanded.is_neutral_edge(edge.id)) {
            crossings.emplace(edge.from, edge.to);
            neutral_representations.insert(edge.from);
            neutral_representations.insert(edge.to);
            check(expanded.edge_cluster(edge.id) == 0 && !edge.is_bidirectional &&
                      edge.zone == 0 && edge.demand == 0 && edge.cost == 28,
                  "Only directed synthetic transitions belong to cluster zero");
        } else if (expanded.edge_cluster(edge.id) > 0) {
            ++physical_count;
            if (edge.id < static_cast<int>(input.edges.size())) continue;
            check(copy_id == -1, "There must be exactly one physical boundary copy");
            copy_id = edge.id;
            check(edge.from == 6 && edge.to == 7 && expanded.edge_cluster(edge.id) == 2 &&
                      edge.is_bidirectional && edge.cost == 7 && edge.zone == 0 &&
                      edge.demand == 0 && !edge.requested && edge.requested_idx == -1,
                  "C2 must receive an optional undirected copy between its two neutrals");
        }
    }
    check(neutral_representations == std::set<int>{2, 3, 6, 7} &&
              crossings == std::set<EdgeKey>{{2, 6}, {6, 2}, {3, 7}, {7, 3}},
          "The four neutral representations must have exactly four directed cluster transitions");
    check(copy_id >= 0 && physical_count == 8 && expanded.requested_amount() == 7,
          "Expansion must add one physical street without duplicating any service");

    const SuperGraph super(expanded, expanded.get_turns(), expanded.get_illegal_turns(),
                           expanded.get_illegal_ext_turns());
    auto physical = [&](int record, int from, int to) -> const SuperArc& {
        for (const SuperArc& arc : super.arcs())
            if (arc.edge_id == record && super.original_node(arc.from) == from &&
                super.original_node(arc.to) == to) return arc;
        throw std::runtime_error("Missing physical orientation in the six-node graph");
    };
    const SuperArc& copy_forward = physical(copy_id, 6, 7);
    const SuperArc& copy_reverse = physical(copy_id, 7, 6);
    check(!copy_forward.is_neutral && !copy_reverse.is_neutral &&
              copy_forward.pair == copy_reverse.id && copy_reverse.pair == copy_forward.id,
          "Both copy orientations must be physical and paired");
    auto check_turn = [&](const SuperArc& from, const SuperArc& to, bool crossing) {
        for (const SuperArc& arc : super.arcs()) {
            if (arc.from != from.to || arc.to != to.from) continue;
            check(arc.is_neutral == crossing &&
                      (crossing ? expanded.is_neutral_edge(arc.edge_id) : arc.edge_id == -1),
                  "Only a transition between clusters must count as a crossing");
            return;
        }
        throw std::runtime_error("Missing legal turn in the six-node graph");
    };
    // C2 circuit, external IDs: 5 -> 6 -> 8 -> 7 -> 5.
    const std::vector<const SuperArc*> cycle{
        &physical(4, 4, 5), &physical(5, 5, 7), &copy_reverse, &physical(3, 6, 4)};
    for (size_t i = 0; i < cycle.size(); ++i)
        check_turn(*cycle[i], *cycle[(i + 1) % cycle.size()], false);
    // External IDs: 2 -> 4 (C1), then 8 -> 7 (C2) costs one crossing.
    check_turn(physical(2, 1, 3), copy_reverse, true);

    GraphSolution values;
    values.objective = 35;
    for (int id = 0; id < 7; ++id) values.service[id] = 1;
    values.traversals = {{6, 2}, {copy_id, 3}};
    for (int id : expanded.get_neutral_edges()) values.traversals[id] = 1;
    const GraphSolution contracted = expanded.contract_solution(values);
    check(contracted.service == values.service &&
              contracted.traversals == std::map<int, long long>{{6, 5}} &&
              contracted.objective == 35,
          "Contraction must keep seven services, sum original/copy traversals and omit crossings");
    std::cout << "PASS six-node boundary: neutral representations=4 physical nodes=8 copies=1\n";
}

void check_boundary_copies(int clusters) {
    Instance input;
    input.vehicles = 1;
    input.nodes = 6;
    // Parallel records must retain independent identities and directions.
    input.edges = {{0, 1, 1, 2.5, 3}, {0, 2, 0, 1, 0},
                   {1, 3, 0, 1, 0}, {0, 4, 0, 1, 0}};
    input.arcs = {{1, 0, 0, 0.75, 0}, {0, 1, 1, 4, 2}};
    input.record_clusters = {2, 1, 1, 3, 2, 2};
    if (clusters == 3) {
        input.edges.push_back({1, 5, 0, 1, 0});
        input.record_clusters.insert(input.record_clusters.begin() + 4, 3);
    }
    input.neutral_nodes = {0, 1};
    input.illegal_turns = {{2, 0, 1}};
    const ExpandedGraphView expanded(input);
    const int originals = input.edges.size() + input.arcs.size();
    std::map<int, int> copies;
    GraphSolution values;
    values.traversals[0] = 2;
    for (const Edge& edge : expanded.records()) {
        if (edge.id < originals || expanded.is_neutral_edge(edge.id)) continue;
        ++copies[expanded.edge_cluster(edge.id)];
        check(expanded.edge_cluster(edge.id) != 2 && edge.zone == 0 &&
                  !edge.requested && edge.requested_idx == -1 && edge.demand == 0,
              "A boundary copy must be optional in a different common cluster");
        const int original = edge.is_bidirectional ? 0 :
            (edge.cost == 0.75 ? originals - 2 : originals - 1);
        check(edge.cost == (original == 0 ? 2.5 : original == originals - 2 ? 0.75 : 4),
              "A boundary copy lost its original cost");
        const int cluster = expanded.edge_cluster(edge.id);
        const int from = expanded.records()[cluster == 1 ? 1 : 3].from;
        const int to = expanded.records()[cluster == 1 ? 2 : 4].from;
        check(edge.from == (edge.cost == 0.75 ? to : from) &&
                  edge.to == (edge.cost == 0.75 ? from : to),
              "A copy must preserve direction using its own cluster representations");
        values.traversals[edge.id] = 3;
    }
    check(copies == (clusters == 3 ? std::map<int, int>{{1, 3}, {3, 3}} :
                                                  std::map<int, int>{{1, 3}}),
          "Copy each original exactly once per common cluster, never to a missing endpoint");
    check(expanded.requested_amount() == 2,
          "Boundary copies must not duplicate required service");
    const auto contracted = expanded.contract_solution(values);
    const long long uses = 3 * (clusters - 1);
    check(contracted.traversals == std::map<int, long long>{
              {0, 2 + uses}, {originals - 2, uses}, {originals - 1, uses}},
          "Contraction must sum every copy into its own original record");

    const SuperGraph super(expanded, expanded.get_turns(), expanded.get_illegal_turns(),
                           expanded.get_illegal_ext_turns());
    for (const SuperArc& connector : super.arcs()) {
        if (connector.edge_id != -1 && !connector.is_neutral) continue;
        const SuperArc* incoming = nullptr;
        const SuperArc* outgoing = nullptr;
        for (const SuperArc* arc : super.super_arcs_to(connector.from))
            if (arc->edge_id >= 0 && !arc->is_neutral) incoming = arc;
        for (const SuperArc* arc : super.super_arcs_from(connector.to))
            if (arc->edge_id >= 0 && !arc->is_neutral) outgoing = arc;
        check(incoming && outgoing, "Turn must join two physical orientations");
        auto original_endpoints = [&](const SuperArc& arc) {
            Solution physical;
            physical.traversals.emplace_back(arc, 1);
            const auto values = expanded.contract_solution(super.to_graph_solution(physical));
            const int id = values.traversals.begin()->first;
            const InstanceEdge& record = id < static_cast<int>(input.edges.size())
                ? input.edges[id] : input.arcs[id - input.edges.size()];
            return arc.pair >= 0 && arc.id > arc.pair
                ? std::make_pair(record.to, record.from)
                : std::make_pair(record.from, record.to);
        };
        const auto [u, v] = original_endpoints(*incoming);
        const auto [w, k] = original_endpoints(*outgoing);
        check(v == w && u != k,
              "Copies must preserve continuity and forbid U-turns across representations");
        check(!(u == 2 && v == 0 && k == 1),
              "A prohibited turn became legal through a copy or crossing");
    }
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: neutral_expanded_graph_test <fixtures>");
        const std::filesystem::path fixtures = std::filesystem::absolute(argv[1]);
        check_six_node_boundary_expansion();
        check_boundary_copies(2);
        check_boundary_copies(3);
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
