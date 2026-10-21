#include <io/BestClusterPathInstanceReader.h>
#include <io/InstanceReader.h>
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
constexpr const char* IN_OUT_HEADER =
    "source_cluster target_cluster in_node out_node";
constexpr const char* ARC_CLUSTERS_HEADER = "arc cluster";

void read_in_out_edges(
    std::istream& input,
    const std::string& name,
    int virtual_node_count,
    std::map<int, std::vector<EdgeKey>>& edges_by_cluster,
    std::vector<ClusterTransition>& transitions) {
    std::string line;
    if (!std::getline(input, line) || line != IN_OUT_HEADER)
        throw std::invalid_argument(name + ": cabecera invalida");

    std::set<EdgeKey> seen;
    int line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;

        std::istringstream row(line);
        int source_cluster = 0;
        int target_cluster = 0;
        long long from = 0;
        long long to = 0;
        std::string extra;
        if (!(row >> source_cluster >> target_cluster >> from >> to) ||
            (row >> extra))
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) + " invalida");
        if (source_cluster <= 0 || target_cluster <= 0 ||
            source_cluster == target_cluster)
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": clusters de origen y destino invalidos");
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
        edges_by_cluster[source_cluster].push_back(edge);
        transitions.push_back({source_cluster, target_cluster, edge});
    }
    if (input.bad()) throw std::invalid_argument(name + ": error de lectura");
    if (seen.empty()) throw std::invalid_argument(name + ": no contiene aristas");
}

void read_arc_clusters(
    std::istream& input,
    const std::string& name,
    std::size_t record_count,
    std::vector<int>& clusters) {
    std::string line;
    if (!std::getline(input, line) || line != ARC_CLUSTERS_HEADER)
        throw std::invalid_argument(name + ": cabecera invalida");

    clusters.assign(record_count, -1);
    int line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) continue;
        std::istringstream row(line);
        long long arc = 0;
        int cluster = -1;
        std::string extra;
        if (!(row >> arc >> cluster) || (row >> extra))
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) + " invalida");
        if (arc < 1 || arc > static_cast<long long>(record_count))
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": arco fuera de rango");
        if (cluster < 0)
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": cluster fuera de rango");
        int& destination = clusters[static_cast<std::size_t>(arc - 1)];
        if (destination != -1)
            throw std::invalid_argument(
                name + ": fila " + std::to_string(line_number) +
                ": arco duplicado");
        destination = cluster;
    }
    if (input.bad()) throw std::invalid_argument(name + ": error de lectura");
    if (std::find(clusters.begin(), clusters.end(), -1) != clusters.end())
        throw std::invalid_argument(name + ": faltan arcos");
}
}

BestClusterPathInstance BestClusterPathInstanceReader::read(
    std::istream& graph,
    std::istream& turns,
    std::istream& in_out,
    std::istream& arc_clusters,
    const std::string& graph_name,
    const std::string& turns_name,
    const std::string& in_out_name,
    const std::string& arc_clusters_name) {
    BestClusterPathInstance result;
    result.rcpp = InstanceReader::read(graph, turns, graph_name, turns_name);
    if (result.rcpp.vehicles != 1)
        throw std::invalid_argument(
            graph_name + ": bestClusterPath requiere exactamente un vehiculo");

    const long long virtual_nodes =
        4LL * result.rcpp.edges.size() + 2LL * result.rcpp.arcs.size();
    if (virtual_nodes <= 0 || virtual_nodes > std::numeric_limits<int>::max())
        throw std::invalid_argument(in_out_name + ": cantidad de nodos fuera de rango");
    read_in_out_edges(
        in_out, in_out_name, static_cast<int>(virtual_nodes),
        result.in_out_edges_by_cluster, result.transitions);
    read_arc_clusters(
        arc_clusters, arc_clusters_name,
        result.rcpp.edges.size() + result.rcpp.arcs.size(),
        result.arc_clusters);
    return result;
}

BestClusterPathInstance BestClusterPathInstanceReader::read_files(
    const std::string& graph_path,
    const std::string& turns_path,
    const std::string& in_out_path,
    const std::string& arc_clusters_path) {
    std::ifstream graph(graph_path), turns(turns_path), in_out(in_out_path),
        arc_clusters(arc_clusters_path);
    if (!graph) throw std::runtime_error("Error al abrir " + graph_path);
    if (!turns) throw std::runtime_error("Error al abrir " + turns_path);
    if (!in_out) throw std::runtime_error("Error al abrir " + in_out_path);
    if (!arc_clusters)
        throw std::runtime_error("Error al abrir " + arc_clusters_path);
    return read(
        graph, turns, in_out, arc_clusters, graph_path, turns_path,
        in_out_path, arc_clusters_path);
}
