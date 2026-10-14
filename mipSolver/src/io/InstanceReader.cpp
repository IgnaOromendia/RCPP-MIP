#include "../../lib/io/InstanceReader.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
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

const char* const CLUSTER_HEADER =
    "arista cluster origen destino vehiculo servicio recorridos pasadas";

std::string trim(const std::string& value) {
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

int parse_cluster_node(const std::string& token, const std::string& context) {
    std::size_t consumed = 0;
    long long value;
    try {
        value = std::stoll(token, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(context + ": nodo invalido");
    }
    if (consumed != token.size() || value <= 0 ||
        value > static_cast<long long>(std::numeric_limits<int>::max()))
        throw std::invalid_argument(context + ": nodo fuera de rango");
    return static_cast<int>(value - 1);
}

const InstanceEdge& original_edge(const Instance& instance, int edge_id) {
    const int undirected_count = static_cast<int>(instance.edges.size());
    if (edge_id < undirected_count) return instance.edges[edge_id];
    return instance.arcs[edge_id - undirected_count];
}

void read_clusters(Instance& instance, std::istream& clusters,
                   const std::string& cluster_name) {
    const std::size_t edge_count = instance.edges.size() + instance.arcs.size();
    constexpr int UNASSIGNED_CLUSTER = -1;
    std::vector<int> cluster_by_edge(edge_count, UNASSIGNED_CLUSTER);
    std::set<int> row_ids;
    bool header_found = false;
    std::string raw_line;
    int line_number = 0;
    while (std::getline(clusters, raw_line)) {
        ++line_number;
        const std::string line = trim(raw_line);
        if (line.empty() || line.front() == '#') continue;
        const std::string context =
            cluster_name + ": linea " + std::to_string(line_number);
        if (!header_found) {
            if (line != CLUSTER_HEADER)
                throw std::invalid_argument(context + ": cabecera invalida");
            header_found = true;
            continue;
        }

        int row_id, cluster, vehicle;
        std::string source_token, target_token, extra;
        long long service_count, deadhead_count, passages;
        std::istringstream row(line);
        if (!(row >> row_id >> cluster >> source_token >> target_token >> vehicle
                  >> service_count >> deadhead_count >> passages) || row >> extra)
            throw std::invalid_argument(context + ": fila invalida");
        if (row_id <= 0 || row_id > static_cast<int>(edge_count) ||
            !row_ids.insert(row_id).second)
            throw std::invalid_argument(
                context + ": identificador de arista invalido o duplicado");
        if (cluster <= 0)
            throw std::invalid_argument(context + ": cluster debe ser positivo");
        if (vehicle <= 0)
            throw std::invalid_argument(context + ": vehiculo debe ser positivo");
        if (service_count != 0 || deadhead_count != 0 || passages != 0)
            throw std::invalid_argument(
                context + ": RCPP requiere un archivo de clusters pre-solver");

        const int edge_id = row_id - 1;
        const InstanceEdge& edge = original_edge(instance, edge_id);
        if (parse_cluster_node(source_token, context) != edge.from ||
            parse_cluster_node(target_token, context) != edge.to)
            throw std::invalid_argument(
                context + ": extremos no coinciden con el grafo");

        cluster_by_edge[edge_id] = cluster - 1;
    }
    if (clusters.bad())
        throw std::runtime_error(cluster_name + ": error de lectura");
    if (!header_found)
        throw std::invalid_argument(cluster_name + ": falta la cabecera");
    if (std::find(cluster_by_edge.begin(), cluster_by_edge.end(), UNASSIGNED_CLUSTER) !=
        cluster_by_edge.end())
        throw std::invalid_argument(
            cluster_name + ": faltan asignaciones para aristas del grafo");

    std::vector<std::map<int, int>> cluster_votes(instance.nodes);
    for (std::size_t edge_id = 0; edge_id < edge_count; ++edge_id) {
        const InstanceEdge& edge = original_edge(instance, static_cast<int>(edge_id));
        ++cluster_votes[edge.from][cluster_by_edge[edge_id]];
        ++cluster_votes[edge.to][cluster_by_edge[edge_id]];
    }
    std::vector<int> primary_cluster(instance.nodes, UNASSIGNED_CLUSTER);
    for (int node = 0; node < instance.nodes; ++node) {
        int best_votes = -1;
        for (const auto& [cluster, votes] : cluster_votes[node]) {
            if (votes > best_votes) {
                primary_cluster[node] = cluster;
                best_votes = votes;
            }
        }
    }
    instance.frontier_edge.assign(edge_count, false);
    for (std::size_t edge_id = 0; edge_id < edge_count; ++edge_id) {
        const InstanceEdge& edge = original_edge(instance, static_cast<int>(edge_id));
        instance.frontier_edge[edge_id] =
            primary_cluster[edge.from] != primary_cluster[edge.to];
    }
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

Instance InstanceReader::read(std::istream& graph, std::istream& turns,
                              std::istream& clusters,
                              const std::string& graph_name,
                              const std::string& turns_name,
                              const std::string& cluster_name) {
    Instance result = read(graph, turns, graph_name, turns_name);
    read_clusters(result, clusters, cluster_name);
    result.validate();
    return result;
}

Instance InstanceReader::read_files(const std::string& graph_path,
                                    const std::string& turns_path,
                                    const std::string& cluster_path) {
    std::ifstream graph(graph_path), turns(turns_path), clusters(cluster_path);
    if (!graph) throw std::runtime_error("Error al abrir " + graph_path);
    if (!turns) throw std::runtime_error("Error al abrir " + turns_path);
    if (!clusters) throw std::runtime_error("Error al abrir " + cluster_path);
    return read(graph, turns, clusters, graph_path, turns_path, cluster_path);
}
