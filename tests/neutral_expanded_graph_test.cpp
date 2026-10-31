#include <graph/NeutralExpandedGraph.h>
#include <graph/SuperGraph.h>
#include <io/ClusterAssignmentReader.h>
#include <io/InstanceReader.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
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

    check(expanded.nodes_amount() == 8 && expanded.deposit() == 8,
          "Neutral copies must precede the synthetic deposit");
    check(expanded.original_cluster_costs().at(1) == 1.25 &&
              expanded.original_cluster_costs().at(2) == 10.75,
          "Original cluster costs must preserve decimals");
    check(expanded.get_neutral_edges().size() == 2 * input.neutral_nodes.size(),
          "Each neutral node must have two directed crossing records");
    check(records.size() == 14 && expanded.edges_amount() == 14,
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

    check(records[12].from == 5 && records[12].to == 6 &&
              records[12].is_bidirectional && records[12].cost == 2.5,
          "Undirected physical copy mismatch");
    check(records[13].from == 6 && records[13].to == 7 &&
              !records[13].is_bidirectional && records[13].cost == 3.75,
          "Directed physical copy mismatch");

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
    projected.service = {{0, 1}, {6, 9}, {12, 2}};
    projected.traversals = {{1, 3}, {4, 9}, {7, 9}, {12, 4}, {13, 5}};
    const GraphSolution contracted = expanded.contract_solution(projected);
    check(contracted.objective == 42,
          "Contraction must preserve the objective");
    check(contracted.service == std::map<int, long long>({{0, 1}, {1, 2}}),
          "Contraction must omit synthetic service and map copied records");
    check(contracted.traversals ==
              std::map<int, long long>({{1, 7}, {2, 5}}),
          "Contraction must omit synthetic traversals and combine multiplicities");

    Instance duplicate = input;
    duplicate.neutral_nodes.push_back(1);
    expect_invalid([&] { NeutralExpandedGraph rejected(duplicate); }, "duplicado");
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Usage: neutral_expanded_graph_test <fixtures>");
        const std::filesystem::path fixtures = std::filesystem::absolute(argv[1]);
        check_reader_validation();
        const Instance input = read_fixture(fixtures);
        check_fixture_reader(fixtures, input);
        check_expansion(input);
        std::cout << "PASS neutral expansion, reader and contraction\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
