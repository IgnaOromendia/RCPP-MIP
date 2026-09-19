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

int read_count(std::istream& stream, const std::string& context, int minimum = 0) {
    const int value = read_value<int>(stream, context);
    if (value < minimum || value > std::numeric_limits<int>::max() - 2)
        throw std::invalid_argument(context + ": cantidad fuera de rango");
    return value;
}

int read_node(std::istream& stream, int nodes, const std::string& context) {
    const int value = read_value<int>(stream, context);
    if (value < 1 || value > nodes) throw std::invalid_argument(context + ": nodo fuera de rango");
    return value - 1; // El archivo numera desde 1; el grafo, desde 0.
}

// Tanto las aristas como los arcos tienen: origen destino zona costo demanda.
InstanceEdge read_edge(std::istream& stream, int nodes, const std::string& context) {
    InstanceEdge edge;
    edge.from = read_node(stream, nodes, context + " origen");
    edge.to = read_node(stream, nodes, context + " destino");
    edge.zone = read_value<int>(stream, context + " zona");
    edge.cost = read_value<double>(stream, context + " costo");
    edge.demand = read_value<double>(stream, context + " demanda");
    return edge;
}

Turn read_turn(std::istream& stream, int nodes, const std::string& context) {
    const int from = read_node(stream, nodes, context);
    const int intersection = read_node(stream, nodes, context);
    const int to = read_node(stream, nodes, context);
    return Turn(from, intersection, to);
}

void check_end_of_input(std::istream& stream, const std::string& context) {
    stream >> std::ws;
    if (!stream.eof() || stream.bad())
        throw std::invalid_argument(context + ": contenido adicional o error de lectura");
}
}

Instance InstanceReader::read(std::istream& graph, std::istream& turns, const std::string& graph_name, const std::string& turns_name) {
    Instance result;

    // Cabecera del grafo: vehiculos, nodos, adyacentes al deposito, aristas y arcos.
    result.vehicles = read_count(graph, graph_name + ": vehiculos", 1);
    result.nodes = read_count(graph, graph_name + ": nodos", 1);
    const int deposit_node_count = read_count(graph, graph_name + ": adyacentes al deposito");
    const int edge_count = read_count(graph, graph_name + ": aristas");
    const int arc_count = read_count(graph, graph_name + ": arcos");

    // Los IDs son int. Cada arista genera 4 nodos virtuales y cada arco, 2.
    const long long virtual_node_count = 4LL * edge_count + 2LL * arc_count;
    const long long total_edge_count = 1LL * deposit_node_count + edge_count + arc_count;
    const int max_index = std::numeric_limits<int>::max();
    if (deposit_node_count > result.nodes || virtual_node_count >= max_index ||
        total_edge_count >= max_index)
        throw std::invalid_argument(graph_name + ": cantidades fuera de rango");

    // Cuerpo del grafo, en el mismo orden que el archivo.
    for (int i = 0; i < deposit_node_count; ++i) {
        const std::string context = graph_name + ": adyacente al deposito " + std::to_string(i + 1);
        result.deposit_nodes.push_back(read_node(graph, result.nodes, context));
    }
    for (int i = 0; i < edge_count; ++i) {
        const std::string context = graph_name + ": arista " + std::to_string(i + 1);
        result.edges.push_back(read_edge(graph, result.nodes, context));
    }
    for (int i = 0; i < arc_count; ++i) {
        const std::string context = graph_name + ": arco " + std::to_string(i + 1);
        result.arcs.push_back(read_edge(graph, result.nodes, context));
    }
    check_end_of_input(graph, graph_name);
    try {
        result.validate();
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(graph_name + ": " + error.what());
    }

    // Archivo de giros: primero los listados, luego los prohibidos.
    const int turn_count = read_count(turns, turns_name + ": cantidad de giros");
    const int illegal_turn_count = read_count(turns, turns_name + ": cantidad de giros prohibidos");
    for (int i = 0; i < turn_count; ++i) {
        const std::string context = turns_name + ": giro " + std::to_string(i + 1);
        result.turns.push_back(read_turn(turns, result.nodes, context));
    }
    for (int i = 0; i < illegal_turn_count; ++i) {
        const std::string context = turns_name + ": giro prohibido " + std::to_string(i + 1);
        result.illegal_turns.push_back(read_turn(turns, result.nodes, context));
    }
    std::sort(result.turns.begin(), result.turns.end());
    std::sort(result.illegal_turns.begin(), result.illegal_turns.end());
    check_end_of_input(turns, turns_name);
    return result;
}

Instance InstanceReader::read_files(const std::string& graph_path, const std::string& turns_path) {
    std::ifstream graph(graph_path), turns(turns_path);
    if (!graph) throw std::runtime_error("Error al abrir " + graph_path);
    if (!turns) throw std::runtime_error("Error al abrir " + turns_path);
    return read(graph, turns, graph_path, turns_path);
}
