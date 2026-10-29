#include <io/ClusterAssignmentReader.h>
#include <algorithm>
#include <set>
#include <limits>
#include <sstream>
#include <stdexcept>

void ClusterAssignmentReader::read(Instance& instance, std::istream& input,
        std::istream* neutral, const std::string& name, const std::string& neutral_name) {
    std::string line;
    while (std::getline(input, line) && (line.empty() || line[0] == '#')) {}
    if (line != "arista cluster origen destino vehiculo servicio recorridos pasadas")
        throw std::invalid_argument(name + ": cabecera invalida");
    std::vector<int> clusters(instance.edges.size() + instance.arcs.size(), 0);
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos || line[0] == '#') continue;
        std::istringstream row(line);
        long long id, cluster, from, to, vehicle, service, traversals, passes;
        std::string extra;
        if (!(row >> id >> cluster >> from >> to >> vehicle >> service >> traversals >> passes) || (row >> extra))
            throw std::invalid_argument(name + ": fila invalida");
        if (id < 1 || id > static_cast<long long>(clusters.size()))
            throw std::invalid_argument(name + ": registro fuera de rango");
        if (clusters[id - 1] != 0) throw std::invalid_argument(name + ": registro duplicado");
        if (cluster <= 0 || cluster > std::numeric_limits<int>::max())
            throw std::invalid_argument(name + ": cluster debe ser positivo");
        const auto& edge = id <= static_cast<long long>(instance.edges.size())
            ? instance.edges[id - 1] : instance.arcs[id - 1 - instance.edges.size()];
        if (from != edge.from + 1 || to != edge.to + 1)
            throw std::invalid_argument(name + ": extremos no coinciden con el grafo");
        if (vehicle || service || traversals || passes)
            throw std::invalid_argument(name + ": columnas de solucion deben estar en cero");
        clusters[id - 1] = static_cast<int>(cluster);
    }
    if (input.bad()) throw std::invalid_argument(name + ": error de lectura");
    if (std::find(clusters.begin(), clusters.end(), 0) != clusters.end())
        throw std::invalid_argument(name + ": faltan registros");
    std::vector<int> nodes;
    std::set<int> seen;
    if (neutral) {
        if (!std::getline(*neutral, line) || line != "node_id reasons")
            throw std::invalid_argument(neutral_name + ": cabecera invalida");
        while (std::getline(*neutral, line)) {
            if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
            std::istringstream row(line);
            long long node;
            std::string reasons, extra;
            if (!(row >> node >> reasons) || (row >> extra) || reasons.front() == ',' ||
                reasons.back() == ',' || reasons.find(",,") != std::string::npos)
                throw std::invalid_argument(neutral_name + ": nodo invalido");
            if (node < 1 || node > instance.nodes)
                throw std::invalid_argument(neutral_name + ": nodo fuera de rango");
            if (!seen.insert(static_cast<int>(node - 1)).second)
                throw std::invalid_argument(neutral_name + ": nodo duplicado");
            nodes.push_back(static_cast<int>(node - 1));
        }
        if (neutral->bad()) throw std::invalid_argument(neutral_name + ": error de lectura");
    }
    instance.record_clusters = std::move(clusters);
    instance.neutral_nodes = std::move(nodes);
}
