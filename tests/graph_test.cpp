#include "TestInstance.h"
#include "lib/graph/Graph.h"
#include <graph/NeutralExpandedGraph.h>
#include <graph/SuperGraph.h>
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
            check(neighbours[i].id == expected[i].first && neighbours[i].edge_id == expected[i].second,
                  "Adjacency global ID");
        }
    }
}

class MutableGraphView : public Graph {
public:
    using Graph::Graph;

    void grow_and_update_records() {
        _all_edges.reserve(_all_edges.capacity() + 1);
        const int id = _all_edges.size();
        _all_edges.emplace_back(id, 2, 0, 0, 4, 0, -1, false);
        _arcs.push_back(id);
        _all_edges[_edges.front()].to = 2;
        _all_edges[_edges.front()].cost = 7;
        _all_edges[_arcs.front()].from = 0;
        _all_edges[_arcs.front()].cost = 9;
    }
};

void check_record_storage() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 3;
    input.deposit_nodes = {0};
    input.edges = {{0,1,1,1,1}};
    input.arcs = {{1,2,0,2,0}};
    MutableGraphView graph(input);
    graph.grow_and_update_records();
    const SuperGraph super(graph, {}, {});
    int orientations = 0;
    for (const SuperArc& arc : super.arcs()) {
        if (arc.edge_id < 0) continue;
        ++orientations;
        const Edge& record = *graph.edge_with_id(arc.edge_id);
        const bool reversed = record.is_bidirectional && arc.id > arc.pair;
        check(super.original_node(arc.from) == (reversed ? record.to : record.from) &&
              super.original_node(arc.to) == (reversed ? record.from : record.to),
              "SuperGraph must read updated endpoints after record reallocation");
        check(arc.cost == record.cost && arc.requested == record.requested &&
              arc.requested_idx == record.requested_idx,
              "SuperGraph must read updated attributes from the owned record");
    }
    check(orientations == 4, "Original and appended orientation counts");
}

// Exercise the inherited Graph contract as well as mixed expansion topology.
class ExpandedGraphView : public NeutralExpandedGraph {
public:
    using NeutralExpandedGraph::NeutralExpandedGraph;
    const std::vector<Edge>& records() const { return _all_edges; }
    const std::vector<int>& edges() const { return _edges; }
    const std::vector<int>& arcs() const { return _arcs; }
};

void check_expansion() {
    Instance input;
    input.vehicles = 1;
    input.nodes = 5;
    input.deposit_nodes = {1, 4};
    input.edges = {{0,1,1,1.25,1}, {1,2,1,2.5,2}};
    input.arcs = {{2,3,0,3.75,0}, {3,4,1,4.5,3}};
    input.record_clusters = {1,2,2,2};
    input.neutral_nodes = {1,2,3};
    const ExpandedGraphView graph(input);
    check(graph.nodes_amount() == 8 && graph.deposit() == 8, "Copies must precede deposit");
    check(graph.edges().size() == 3 && graph.arcs().size() == 9, "Mixed copy counts");
    check(graph.original_cluster_costs().at(1) == 1.25 &&
          graph.original_cluster_costs().at(2) == 10.75, "Original decimal cluster costs");
    check(graph.requested_amount() == 3, "Expansion must not duplicate service");
    const auto& records = graph.records();
    check(records.size() == 14, "Physical, deposit, crossing and copy records");
    for (int id = 0; id < 4; ++id)
        check(records[id].from == id && records[id].to == id + 1,
              "Moving deposit must preserve original physical endpoints");
    for (std::size_t id = 0; id < records.size(); ++id) {
        const Edge& edge = records[id];
        check(edge.id == id, "Expanded global ID");
        if (id >= 6) check(!edge.requested && edge.requested_idx == -1 && edge.demand == 0,
                          "Added records permit traversal only");
    }
    check(records[4].from == 8 && records[4].to == 1 &&
          records[5].from == 8 && records[5].to == 4, "Preserve original deposit access nodes");
    check(graph.neighbours(graph.deposit()).size() == input.deposit_nodes.size(),
          "Expansion must not add direct deposit access");
    for (const Node& access : graph.neighbours(graph.deposit()))
        check(access.id == 1 || access.id == 4, "Only original adjacent nodes access deposit");
    for (int id = 6; id < 12; ++id)
        check(!records[id].is_bidirectional && records[id].cost == (id < 8 ? 12 : 10.75), "Directed decimal crossings");
    check(records[12].from == 5 && records[12].to == 6 &&
          records[12].is_bidirectional && records[12].cost == 2.5, "Undirected physical copy");
    check(records[13].from == 6 && records[13].to == 7 &&
          !records[13].is_bidirectional && records[13].cost == 3.75, "Directed physical copy");
    std::vector<int> incidence(records.size(), 0);
    for (int v = 0; v <= graph.deposit(); ++v) {
        for (const Node& adjacent : graph.neighbours(v)) {
            check(adjacent.edge_id >= 0 && adjacent.edge_id < records.size(), "Adjacency record ID range");
            const Edge& edge = records[adjacent.edge_id];
            check((edge.from == v && edge.to == adjacent.id) ||
                  (edge.is_bidirectional && edge.to == v && edge.from == adjacent.id),
                  "Adjacency must match record direction");
            ++incidence[adjacent.edge_id];
        }
    }
    for (const Edge& edge : records)
        check(incidence[edge.id] == (edge.is_bidirectional ? 2 : 1), "Record adjacency multiplicity");

    const SuperGraph super(graph, {}, {});
    int copy_orientations = 0;
    for (const SuperArc& arc : super.arcs()) {
        if (arc.edge_id != 12 && arc.edge_id != 13) continue;
        ++copy_orientations;
        check((arc.edge_id == 12) == (arc.pair != -1), "Copied orientation pairing");
        if (arc.edge_id == 13)
            check(super.original_node(arc.from) == 6 && super.original_node(arc.to) == 7,
                  "Directed copy orientation in SuperGraph");
        bool connected = false;
        for (const SuperArc& turn : super.arcs())
            if (turn.edge_id == -1 && turn.from == arc.to) connected = true;
        check(connected, "Physical copy must connect to turns");
    }
    check(copy_orientations == 3, "Two undirected orientations and one directed orientation");

    auto invalid = [&](const Instance& instance) {
        try { const NeutralExpandedGraph rejected(instance); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("Expected invalid expansion input to be rejected");
    };
    Instance bad = input; bad.neutral_nodes.push_back(1); invalid(bad);
    bad = input; bad.neutral_nodes.clear();
    const ExpandedGraphView ordinary(bad);
    check(ordinary.records().size() == 6 && ordinary.deposit() == 5 &&
          ordinary.edges().size() == 2 && ordinary.arcs().size() == 2, "No-neutral expansion");
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
        check_record_storage();
        check_expansion();
        check_graph(argv[1], std::stoi(argv[2]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
