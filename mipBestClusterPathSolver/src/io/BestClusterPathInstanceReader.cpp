#include <io/BestClusterPathInstanceReader.h>
#include <io/InstanceReader.h>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
constexpr const char* IN_OUT_HEADER = "cluster in_node out_node";

void read_in_out_edges(
    std::istream& input,
    const std::string& name,
    int virtual_node_count,
    std::map<int, std::vector<EdgeKey>>& edges_by_cluster) {
    std::string line;
    if (!std::getline(input, line) || line != IN_OUT_HEADER)
        throw std::invalid_argument(name + ": cabecera invalida");

    std::set<EdgeKey> seen;
    int line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;

        std::istringstream row(line);
        int cluster = 0;
        long long from = 0;
        long long to = 0;
        std::string extra;
        if (!(row >> cluster >> from >> to) || (row >> extra))
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) + " invalida");
        if (cluster <= 0)
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": cluster debe ser positivo");
        if (from < 1 || from > virtual_node_count ||
            to < 1 || to > virtual_node_count) {
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": nodo fuera de rango");
        }

        const EdgeKey edge(static_cast<int>(from - 1), static_cast<int>(to - 1));
        if (!seen.insert(edge).second)
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": arista duplicada");
        edges_by_cluster[cluster].push_back(edge);
    }
    if (input.bad()) throw std::invalid_argument(name + ": error de lectura");
    if (seen.empty()) throw std::invalid_argument(name + ": no contiene aristas");
}
}

BestClusterPathInstance BestClusterPathInstanceReader::read(
    std::istream& graph,
    std::istream& turns,
    std::istream& in_out,
    const std::string& graph_name,
    const std::string& turns_name,
    const std::string& in_out_name) {
    BestClusterPathInstance result;
    result.rcpp = InstanceReader::read(graph, turns, graph_name, turns_name);

    const long long virtual_nodes =
        4LL * result.rcpp.edges.size() + 2LL * result.rcpp.arcs.size();
    if (virtual_nodes <= 0 || virtual_nodes > std::numeric_limits<int>::max())
        throw std::invalid_argument(in_out_name + ": cantidad de nodos fuera de rango");
    read_in_out_edges(
        in_out, in_out_name, static_cast<int>(virtual_nodes),
        result.in_out_edges_by_cluster);
    return result;
}

BestClusterPathInstance BestClusterPathInstanceReader::read_files(
    const std::string& graph_path,
    const std::string& turns_path,
    const std::string& in_out_path) {
    std::ifstream graph(graph_path), turns(turns_path), in_out(in_out_path);
    if (!graph) throw std::runtime_error("Error al abrir " + graph_path);
    if (!turns) throw std::runtime_error("Error al abrir " + turns_path);
    if (!in_out) throw std::runtime_error("Error al abrir " + in_out_path);
    return read(
        graph, turns, in_out, graph_path, turns_path, in_out_path);
}
