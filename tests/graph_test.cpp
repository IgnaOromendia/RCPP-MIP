#include "TestInstance.h"
#include "lib/graph/Graph.h"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check_graph(const std::filesystem::path& fixture, int undirected_count) {
    Graph graph(test_instance(fixture));
    const int zones[] = {0, 1, 0, -1};
    const int costs[] = {2, 3, 5, 7};
    const int demands[] = {0, 1, 0, 2};
    const int requested_indices[] = {-1, 0, -1, 1};
    const auto requested = graph.requested();
    check(graph.requested_amount() == 2 && requested.size() == 2, "Required count");
    check(requested[0] == graph.edge_with_id(1), "First required global ID");
    check(requested[1] == graph.edge_with_id(3), "Second required global ID");

    std::vector<std::vector<std::pair<int, int>>> expected_adjacency(5);
    for (int id = 0; id < 4; ++id) {
        const Edge& edge = *graph.edge_with_id(id);
        check(edge.id == id, "Global ID must equal position in all edges");
        check(edge.from == id && edge.to == (id + 1) % 4, "Original endpoints");
        check(edge.zone == zones[id] && edge.cost == costs[id] &&
              edge.demand == demands[id], "Original attributes");
        check(edge.requested == (zones[id] != 0) &&
              edge.requested_idx == requested_indices[id], "Required index");
        check(edge.is_bidirectional == (id < undirected_count), "Original direction");
        expected_adjacency[edge.from].emplace_back(edge.to, id);
        if (id < undirected_count) expected_adjacency[edge.to].emplace_back(edge.from, id);
    }
    check(graph.deposit() == 4, "Deposit node");
    for (int node = 0; node < 4; ++node) {
        const int id = 4 + node;
        const Edge& edge = *graph.edge_with_id(id);
        check(edge.id == id && edge.from == 4 && edge.to == node &&
              edge.is_bidirectional && !edge.requested && edge.requested_idx == -1,
              "Synthetic deposit edge must follow original global IDs");
        expected_adjacency[4].emplace_back(node, id);
        expected_adjacency[node].emplace_back(4, id);
    }
    for (int node = 0; node <= 4; ++node) {
        const auto& neighbours = graph.neighbours(node);
        const auto& expected = expected_adjacency[node];
        check(neighbours.size() == expected.size(), "Adjacency size");
        for (std::size_t i = 0; i < expected.size(); ++i) {
            // Graph stores the global edge ID in Node::cost.
            check(neighbours[i].id == expected[i].first && neighbours[i].cost == expected[i].second,
                  "Adjacency global ID");
        }
    }
}

void check_default_graph() {
    Graph graph;
    check(graph.nodes_amount() == 0 && graph.requested_amount() == 0, "Default graph counts");
    check(graph.deposit() == -1, "Default graph has no deposit");
    check(graph.requested().empty(), "Default graph has no required edges");
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: graph_test <fixture> <undirected_count>");
        check_default_graph();
        check_graph(argv[1], std::stoi(argv[2]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
