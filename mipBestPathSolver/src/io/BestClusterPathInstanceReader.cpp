#include <io/BestClusterPathInstanceReader.h>
#include <io/ClusterAssignmentReader.h>
#include <io/InstanceReader.h>
#include <fstream>
#include <stdexcept>

Instance BestClusterPathInstanceReader::read(std::istream& graph, std::istream& turns,
        std::istream& clusters, std::istream* neutral_nodes) {
    Instance result = InstanceReader::read(graph, turns);
    if (result.vehicles != 1)
        throw std::invalid_argument("bestClusterPath requiere exactamente un vehiculo");
    ClusterAssignmentReader::read(result, clusters, neutral_nodes);
    return result;
}
Instance BestClusterPathInstanceReader::read_files(const std::string& graph_path,
        const std::string& turns_path, const std::string& clusters_path,
        const std::string& neutral_path) {
    std::ifstream graph(graph_path), turns(turns_path), clusters(clusters_path), neutral;
    if (!graph) throw std::runtime_error("Error al abrir " + graph_path);
    if (!turns) throw std::runtime_error("Error al abrir " + turns_path);
    if (!clusters) throw std::runtime_error("Error al abrir " + clusters_path);
    if (!neutral_path.empty()) {
        neutral.open(neutral_path);
        if (!neutral) throw std::runtime_error("Error al abrir " + neutral_path);
    }
    return read(graph, turns, clusters, neutral_path.empty() ? nullptr : &neutral);
}
