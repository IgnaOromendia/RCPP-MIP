#include <io/ClusterAssignmentReader.h>
#include <algorithm>
#include <set>
#include <limits>
#include <sstream>
#include <stdexcept>

void ClusterAssignmentReader::read(Instance& instance, std::istream& input,
        std::istream* neutral, const std::string& name, const std::string& neutral_name) {
    std::string line;
    std::map<int, std::vector<int>> shared;
    std::set<int> positive_clusters;
    auto read_metadata = [&](const std::string& comment) {
        std::istringstream row(comment);
        std::string marker, kind;
        if (!(row >> marker >> kind) || marker != "#" || kind != "frontera") return;
        int id;
        if (!(row >> id) || id < 1 || !shared.emplace(id - 1, std::vector<int>{}).second)
            throw std::invalid_argument(name + ": registro de frontera invalido o duplicado");
        auto& members = shared.at(id - 1);
        std::string token;
        while (row >> token) {
            std::istringstream value(token);
            int cluster;
            char extra;
            if (!(value >> cluster) || (value >> extra))
                throw std::invalid_argument(name + ": barrio de frontera invalido");
            members.push_back(cluster);
        }
        std::sort(members.begin(), members.end());
        if (members.size() < 2 || members.front() <= 0 ||
            std::adjacent_find(members.begin(), members.end()) != members.end())
            throw std::invalid_argument(name + ": barrios de frontera invalidos");
    };
    while (std::getline(input, line) && (line.empty() || line[0] == '#'))
        read_metadata(line);
    if (line != "arista cluster origen destino vehiculo servicio recorridos pasadas")
        throw std::invalid_argument(name + ": cabecera invalida");
    std::vector<int> clusters(instance.edges.size() + instance.arcs.size(), 0);
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        if (line[0] == '#') {
            read_metadata(line);
            continue;
        }
        std::istringstream row(line);
        long long id, cluster, from, to, vehicle, service, traversals, passes;
        std::string extra;
        if (!(row >> id >> cluster >> from >> to >> vehicle >> service >> traversals >> passes) || (row >> extra))
            throw std::invalid_argument(name + ": fila invalida");
        if (id < 1 || id > static_cast<long long>(clusters.size()))
            throw std::invalid_argument(name + ": registro fuera de rango");
        if (clusters[id - 1] != 0) throw std::invalid_argument(name + ": registro duplicado");
        if ((cluster <= 0 && cluster != -1) || cluster > std::numeric_limits<int>::max())
            throw std::invalid_argument(name + ": cluster debe ser positivo o -1");
        const auto& edge = id <= static_cast<long long>(instance.edges.size())
            ? instance.edges[id - 1] : instance.arcs[id - 1 - instance.edges.size()];
        if (from != edge.from + 1 || to != edge.to + 1)
            throw std::invalid_argument(name + ": extremos no coinciden con el grafo");
        if (vehicle || service || traversals || passes)
            throw std::invalid_argument(name + ": columnas de solucion deben estar en cero");
        clusters[id - 1] = static_cast<int>(cluster);
        if (cluster > 0) positive_clusters.insert(static_cast<int>(cluster));
    }
    if (input.bad()) throw std::invalid_argument(name + ": error de lectura");
    if (std::find(clusters.begin(), clusters.end(), 0) != clusters.end())
        throw std::invalid_argument(name + ": faltan registros");
    for (size_t id = 0; id < clusters.size(); ++id)
        if ((clusters[id] == -1) != (shared.count(id) != 0))
            throw std::invalid_argument(name + ": falta o sobra metadata de frontera");
    for (const auto& [id, members] : shared) {
        if (static_cast<size_t>(id) >= clusters.size())
            throw std::invalid_argument(name + ": registro de frontera fuera de rango");
        for (int member : members)
            if (!positive_clusters.count(member))
                throw std::invalid_argument(name + ": barrio de frontera inexistente");
    }
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
    instance.shared_record_clusters = std::move(shared);
    instance.neutral_nodes = std::move(nodes);
}
