#include "../lib/InstanceReader.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {
template<class T>
T read_value(std::istream& stream, const std::string& context) {
    T value;
    if (!(stream >> value)) throw std::invalid_argument(context + ": dato ausente o invalido");
    return value;
}

int count(std::istream& stream, const std::string& context, int minimum = 0) {
    const int value = read_value<int>(stream, context);
    if (value < minimum || value > std::numeric_limits<int>::max() - 2)
        throw std::invalid_argument(context + ": cantidad fuera de rango");
    return value;
}

int node(std::istream& stream, int nodes, const std::string& context) {
    const int value = read_value<int>(stream, context);
    if (value < 1 || value > nodes) throw std::invalid_argument(context + ": nodo fuera de rango");
    return value - 1;
}

void end_of_input(std::istream& stream, const std::string& context) {
    stream >> std::ws;
    if (!stream.eof() || stream.bad())
        throw std::invalid_argument(context + ": contenido adicional o error de lectura");
}
}

Instance InstanceReader::read(std::istream& graph, std::istream& turns,
                              const std::string& graph_name, const std::string& turns_name) {
    Instance result;
    result.vehicles = count(graph, graph_name + ": vehiculos", 1);
    result.nodes = count(graph, graph_name + ": nodos", 1);
    const int adjacent = count(graph, graph_name + ": adyacentes al deposito");
    const int edges = count(graph, graph_name + ": aristas");
    const int arcs = count(graph, graph_name + ": arcos");
    if (adjacent > result.nodes || 4LL * edges + 2LL * arcs >= std::numeric_limits<int>::max() ||
        1LL * adjacent + edges + arcs >= std::numeric_limits<int>::max())
        throw std::invalid_argument(graph_name + ": cantidades fuera de rango");

    for (int i = 0; i < adjacent; ++i)
        result.deposit_nodes.push_back(node(graph, result.nodes,
            graph_name + ": adyacente al deposito " + std::to_string(i + 1)));
    auto read_edges = [&](int size, auto& records, const std::string& kind) {
        for (int i = 0; i < size; ++i) {
            const auto context = graph_name + ": " + kind + " " + std::to_string(i + 1);
            const int from = node(graph, result.nodes, context + " origen");
            const int to = node(graph, result.nodes, context + " destino");
            const int zone = read_value<int>(graph, context + " zona");
            const double cost = read_value<double>(graph, context + " costo");
            const double demand = read_value<double>(graph, context + " demanda");
            records.push_back({from, to, zone, cost, demand});
        }
    };
    read_edges(edges, result.edges, "arista");
    read_edges(arcs, result.arcs, "arco");
    end_of_input(graph, graph_name);
    try {
        result.validate();
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(graph_name + ": " + error.what());
    }

    const int listed = count(turns, turns_name + ": cantidad de giros");
    const int illegal = count(turns, turns_name + ": cantidad de giros prohibidos");
    auto read_turns = [&](int size, auto& records, const std::string& kind) {
        for (int i = 0; i < size; ++i) {
            const auto context = turns_name + ": " + kind + " " + std::to_string(i + 1);
            const int v = node(turns, result.nodes, context);
            const int w = node(turns, result.nodes, context);
            const int u = node(turns, result.nodes, context);
            records.emplace_back(v, w, u);
        }
        std::sort(records.begin(), records.end());
    };
    read_turns(listed, result.turns, "giro");
    read_turns(illegal, result.illegal_turns, "giro prohibido");
    end_of_input(turns, turns_name);
    return result;
}

Instance InstanceReader::read_files(const std::string& graph_path, const std::string& turns_path) {
    std::ifstream graph(graph_path), turns(turns_path);
    if (!graph) throw std::runtime_error("Error al abrir " + graph_path);
    if (!turns) throw std::runtime_error("Error al abrir " + turns_path);
    return read(graph, turns, graph_path, turns_path);
}
