#include "lib/SuperGraph.h"
#include <stdexcept>
#include <string>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void check_super_graph(const std::string& fixture, int undirected_count) {
    Graph graph(fixture);
    std::vector<Turn> turns, illegal_turns;
    SuperGraph super_graph(&graph, turns, illegal_turns);
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
    check(turns_count > 0 && deposit_count == 2 * super_id, "Exercise both connector types");
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "Usage: super_graph_test <fixture> <undirected_count>");
        check_super_graph(argv[1], std::stoi(argv[2]));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
