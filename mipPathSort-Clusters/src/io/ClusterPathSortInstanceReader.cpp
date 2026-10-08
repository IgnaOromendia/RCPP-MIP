#include <io/ClusterPathSortInstanceReader.h>
#include <io/PathSortInstanceReader.h>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace {
const char* const HEADER =
    "arista cluster origen destino vehiculo servicio recorridos pasadas";

std::string trim(const std::string& value) {
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

int parse_node(const std::string& token, int deposit, const std::string& context) {
    if (token == "D") return deposit;
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
}

ClusterPathSortInstance ClusterPathSortInstanceReader::read(
    PathSortInstance instance,
    std::istream& clusters,
    const std::string& cluster_name) {
    using EdgeKey = std::tuple<int, int, int>;
    std::map<EdgeKey, std::size_t> edge_by_endpoints;
    for (std::size_t edge_index = 0; edge_index < instance.edges.size(); ++edge_index) {
        const PathEdge& edge = instance.edges[edge_index];
        if (!edge_by_endpoints.emplace(
                EdgeKey{edge.from, edge.to, edge.vehicle}, edge_index).second)
            throw std::logic_error(
                "La instancia de orden contiene aristas indistinguibles para clusters");
    }

    std::vector<int> edge_clusters(instance.edges.size(), 0);
    int cluster_count = 0;
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
            if (line != HEADER)
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
        if (row_id <= 0 || row_id > static_cast<int>(instance.edges.size()) ||
            !row_ids.insert(row_id).second)
            throw std::invalid_argument(context + ": identificador de arista invalido o duplicado");
        if (cluster <= 0)
            throw std::invalid_argument(context + ": cluster debe ser positivo");
        if (cluster > cluster_count) cluster_count = cluster;
        if (vehicle <= 0)
            throw std::invalid_argument(context + ": vehiculo debe ser positivo");
        if (service_count < 0 || deadhead_count < 0 || passages < 0 ||
            service_count > std::numeric_limits<long long>::max() - deadhead_count ||
            service_count + deadhead_count != passages)
            throw std::invalid_argument(context + ": cantidades de pasadas invalidas");

        const int source = parse_node(source_token, instance.deposit, context);
        const int target = parse_node(target_token, instance.deposit, context);
        const auto found = edge_by_endpoints.find({source, target, vehicle});
        if (found == edge_by_endpoints.end())
            throw std::invalid_argument(context + ": arista inexistente en PathSortInstance");
        const std::size_t edge_index = found->second;
        if (edge_clusters[edge_index] != 0)
            throw std::invalid_argument(context + ": asignacion de cluster duplicada");
        const PathEdge& edge = instance.edges[edge_index];
        if (service_count != edge.service_count ||
            deadhead_count != edge.deadhead_count || passages != edge.times())
            throw std::invalid_argument(context + ": pasadas no coinciden con PathSortInstance");
        edge_clusters[edge_index] = cluster;
    }
    if (clusters.bad())
        throw std::runtime_error(cluster_name + ": error de lectura");
    if (!header_found)
        throw std::invalid_argument(cluster_name + ": falta la cabecera");
    for (int cluster : edge_clusters)
        if (cluster == 0)
            throw std::invalid_argument(cluster_name + ": faltan asignaciones de clusters");

    return {std::move(instance), std::move(edge_clusters), cluster_count};
}

ClusterPathSortInstance ClusterPathSortInstanceReader::read_file(
    PathSortInstance instance,
    const std::string& cluster_path) {
    std::ifstream clusters(cluster_path);
    if (!clusters) throw std::runtime_error("Error al abrir " + cluster_path);
    return read(std::move(instance), clusters, cluster_path);
}

ClusterPathSortInstance ClusterPathSortInstanceReader::read_files(
    const std::string& graph_path,
    const std::string& turns_path,
    const std::string& solution_path,
    const std::string& cluster_path) {
    return read_file(
        PathSortInstanceReader::read_files(graph_path, turns_path, solution_path),
        cluster_path);
}
