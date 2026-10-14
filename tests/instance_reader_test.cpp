#include "lib/io/InstanceReader.h"
#include "lib/graph/Graph.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Instance read(const std::string& graph, const std::string& turns = "0 0") {
    std::istringstream graph_stream(graph), turns_stream(turns);
    return InstanceReader::read(graph_stream, turns_stream, "graph.dat", "turns.dat");
}

Instance read_clustered(const std::string& graph, const std::string& clusters,
                        const std::string& turns = "0 0") {
    std::istringstream graph_stream(graph), turns_stream(turns),
        cluster_stream(clusters);
    return InstanceReader::read(
        graph_stream, turns_stream, cluster_stream,
        "graph.dat", "turns.dat", "clusters.dat");
}

void reject(const std::string& graph, const std::string& turns, const std::string& diagnostic) {
    try {
        read(graph, turns);
    } catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(diagnostic) != std::string::npos,
              "Missing context: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Accepted malformed input: " + graph + " / " + turns);
}

void reject_clusters(const std::string& graph, const std::string& clusters,
                     const std::string& diagnostic) {
    try {
        read_clustered(graph, clusters);
    } catch (const std::invalid_argument& error) {
        check(std::string(error.what()).find(diagnostic) != std::string::npos,
              "Missing cluster context: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("Accepted malformed cluster input: " + clusters);
}

int main(int argc, char** argv) {
    try {
        if (argc == 3) {
            const auto generated = InstanceReader::read_files(argv[1], argv[2]);
            const Graph graph(generated);
            std::cout << graph.nodes_amount() << ' ' << generated.edges.size() << ' '
                      << graph.requested_amount() << ' ' << generated.turns.size() << ' '
                      << generated.illegal_turns.size() << '\n';
            return 0;
        }
        check(argc == 1, "Usage: instance_reader_test [graph.dat turns.dat]");
        const std::string minimal = "1 2 2 0 1\n1 2\n1 2 1 7 3";
        const auto instance = read("2 3 1 1 1\n1\n1 2 -1 1.123456789123 0.125\n2 3 2 4 2",
                                   "2 2\n3 2 1\n1 2 3\n3 2 1\n1 2 3");
        check(instance.vehicles == 2 && instance.nodes == 3 && instance.deposit_nodes[0] == 0,
              "Header and node normalization");
        check(instance.edges.size() == 1 && instance.arcs.size() == 1 && instance.arcs[0].zone == 2,
              "Mixed records and zones");
        check(instance.turns[0].v == 0 && instance.illegal_turns[0].v == 0 &&
              instance.illegal_turns[1].u == 0, "Normalize and sort both turn lists");
        const Graph graph(instance);
        check(std::abs(graph.edge_with_id(0)->cost - 1.123456789123) < 1e-12,
              "Reader and Graph must preserve double precision");
        check(graph.edge_with_id(0)->demand == 0.125 && graph.edge_with_id(1)->zone == 2,
              "Preserve demand and assigned vehicle");
        for (const auto& invalid : std::vector<std::string>{
                 "", "-1 2 0 0 0", "1 0 0 0 0", "1 2 -1 0 0", "1 2 0 -1 0",
                 "1 2 0 0 -1", "1 2 3 0 0", "1 2 0 2147483647 0",
                 "1 2 1 0 0\n0", "1 2 1 0 0\n3", "1 2 1 0 0\n",
                 "1 2 0 0 1\n0 2 1 7 3", "1 2 0 0 1\n1 3 1 7 3",
                 "1 2 0 0 1\n1 2 2 7 3", "1 2 0 0 1\n1 2 -2 7 3",
                 "1 2 0 0 1\n1 2 1 7", "1 2 0 0 1\n1 2 1 nope 3",
                 "1 2 0 0 1\n1 2 1 0 3", "1 2 0 0 1\n1 2 1 7 -3",
                 "1 2 0 0 1\n1 2 1 inf 3", minimal + " extra"})
            reject(invalid, "0 0", "graph.dat");
        for (const auto& invalid : {"", "-1 0", "0 -1", "1 0", "0 1 1 2",
                                    "0 1 1 2 3", "1 0 0 1 2", "0 0 extra"})
            reject(minimal, invalid, "turns.dat");
        reject("1 2 0 0 1\n1 2 2 7 3", "0 0", "arco 1");
        reject(minimal, "0 1 1 2 3", "giro prohibido 1");
        auto changed = instance;
        changed.arcs[0].to = 3;
        bool caught = false;
        try { Graph invalid(changed); }
        catch (const std::invalid_argument&) { caught = true; }
        check(caught, "In-memory instances must be validated too");
        // Zero demand remains accepted; connectivity for that domain is a separate issue.
        check(read("1 2 0 0 1\n1 2 1 7 0").arcs[0].demand == 0, "Zero demand contract");

        const std::string clustered_graph =
            "2 4 1 2 1\n1\n"
            "1 2 -1 3 1\n"
            "2 3 0 4 0\n"
            "3 4 2 5 2\n";
        const std::string cluster_header =
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n";
        const Instance clustered = read_clustered(
            clustered_graph,
            "# clusters del grafo original\n" + cluster_header +
            "3 7 3 4 1 0 0 0\n"
            "1 7 1 2 1 0 0 0\n"
            "2 1 2 3 1 0 0 0\n");
        check(clustered.frontier_edge ==
                  std::vector<bool>({true, false, true}),
              "Edges connecting different primary node clusters were not marked");

        reject_clusters(clustered_graph,
                        cluster_header + "1 1 1 2 1 0 0 0\n",
                        "faltan asignaciones");
        reject_clusters(
            clustered_graph,
            cluster_header +
                "1 1 4 2 1 0 0 0\n"
                "2 1 2 3 1 0 0 0\n"
                "3 1 3 4 1 0 0 0\n",
            "extremos no coinciden");
        reject_clusters(
            clustered_graph,
            cluster_header +
                "1 1 1 2 1 1 0 1\n"
                "2 1 2 3 1 0 0 0\n"
                "3 1 3 4 1 0 0 0\n",
            "pre-solver");
        reject_clusters(
            clustered_graph,
            cluster_header +
                "1 1 1 2 1 0 0 0\n"
                "1 2 1 2 1 0 0 0\n"
                "3 1 3 4 1 0 0 0\n",
            "duplicado");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
