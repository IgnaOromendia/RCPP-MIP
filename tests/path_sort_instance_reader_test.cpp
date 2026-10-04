#include "lib/io/SolutionWriter.h"
#include "lib/graph/Graph.h"
#include "lib/graph/SuperGraph.h"
#include "lib/model/RCPPSolver.h"
#include <io/PathSortInstanceReader.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const SuperArc& find_arc(const SuperGraph& graph, int edge_id) {
    for (const SuperArc& arc : graph.arcs())
        if (arc.edge_id == edge_id) return arc;
    throw std::runtime_error("Missing test arc");
}

const SuperArc& find_connector(const SuperGraph& graph, int from, int to) {
    for (const SuperArc& arc : graph.arcs())
        if (arc.from == from && arc.to == to) return arc;
    throw std::runtime_error("Missing test connector");
}

const PathEdge& find_path_edge(const PathSortInstance& instance, int super_arc_id) {
    for (const PathEdge& edge : instance.edges)
        if (edge.super_arc_id == super_arc_id) return edge;
    throw std::runtime_error("Missing parsed path edge");
}

int main() {
    try {
        Instance graph_instance;
        graph_instance.vehicles = 1;
        graph_instance.nodes = 3;
        graph_instance.deposit_nodes = {0, 2};
        graph_instance.arcs = {{0, 1, 1, 2, 1}, {1, 2, 1, 3, 1}};

        Graph graph(graph_instance);
        SuperGraph super_graph(graph, graph_instance.turns, graph_instance.illegal_turns);
        const SuperArc& first = find_arc(super_graph, 0);
        const SuperArc& second = find_arc(super_graph, 1);
        const SuperArc& turn = find_connector(super_graph, first.to, second.from);
        const SuperArc& departure = find_connector(super_graph, super_graph.deposit(), first.from);
        const SuperArc& arrival = find_connector(super_graph, second.to, super_graph.deposit());

        Solution solution;
        solution.objective = 7;
        solution.service = {{first.id, first.from, first.to, 1, 1},
                            {second.id, second.from, second.to, 1, 1}};
        solution.traversals = {{first.id, first.from, first.to, 1, 2},
                               {turn.id, turn.from, turn.to, 1, 1}};
        solution.deposit_traversals = {
            {departure.id, -1, departure.to, 1, 1},
            {arrival.id, arrival.from, -1, 1, 1}
        };
        std::ostringstream output;
        SolutionWriter::write(output, solution);
        std::istringstream input(output.str());

        const PathSortInstance parsed =
            PathSortInstanceReader::read(graph_instance, input, "test solution");
        check(parsed.vehicles == 1 && parsed.edges.size() == 5,
              "Reader must retain vehicles and positive X + Y multiplicities");
        const PathEdge& parsed_first = find_path_edge(parsed, first.id);
        check(parsed_first.original_edge_id == 0 && parsed_first.vehicle == 1,
              "Original edge and vehicle mapping");
        check(parsed_first.service_count == 1 && parsed_first.deadhead_count == 2 &&
              parsed_first.times() == 3, "X and Y must be added to obtain total passages");
        check(find_path_edge(parsed, turn.id).original_edge_id == -1,
              "Turn connector mapping");
        check(find_path_edge(parsed, departure.id).from == -1 &&
              find_path_edge(parsed, arrival.id).to == -1,
              "Deposit connector mapping");

        RCPPSolver solver(super_graph, graph_instance.vehicles);
        solver.generate_MIP();
        solver.set_time_objective();
        const SolveResult result = solver.solve();
        check(result.has_solution, "The integration instance must be feasible");
        SolutionWriter::write_file("actual-solver-out.dat", result.extract_solution());
        const PathSortInstance actual =
            PathSortInstanceReader::read_file(graph_instance, "actual-solver-out.dat");
        check(find_path_edge(actual, first.id).times() == 1 &&
              find_path_edge(actual, second.id).times() == 1,
              "Reader must consume actual RCPPSolver output");

        std::ofstream graph_file("path-graph.dat");
        graph_file << "1 3 2 0 2\n1 3\n1 2 1 2 1\n2 3 1 3 1\n";
        graph_file.close();
        std::ofstream turns_file("path-turns.dat");
        turns_file << "0 0\n";
        turns_file.close();
        const PathSortInstance from_files = PathSortInstanceReader::read_files(
            "path-graph.dat", "path-turns.dat", "actual-solver-out.dat");
        check(from_files.edges.size() == actual.edges.size() &&
              find_path_edge(from_files, first.id).times() == 1,
              "Reader must load graph, turns and solution files together");

        std::istringstream invalid("OBJ: 0\n\n---- X ----\nX_99_100_1 = 1\n");
        bool caught = false;
        try {
            PathSortInstanceReader::read(graph_instance, invalid, "invalid solution");
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        check(caught, "Unknown arcs must be rejected");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
