#include <model/PathSorterClusters.h>
#include <stdexcept>
#include <utility>

namespace {
const char* const NOT_IMPLEMENTED =
    "El modelo PathSorterCluster todavia no esta implementado.";
}

PathSorterCluster::PathSorterCluster(): PathSolver() {}

PathSorterCluster::PathSorterCluster(ClusterPathSortInstance instance):
    PathSolver(std::move(instance.path)),
    _edge_clusters(std::move(instance.edge_clusters)) {
    if (_edge_clusters.size() != _instance.edges.size())
        throw std::invalid_argument(
            "La cantidad de clusters no coincide con la cantidad de aristas");
    for (int cluster : _edge_clusters)
        if (cluster <= 0)
            throw std::invalid_argument("Cada arista debe tener un cluster positivo");
}

PathSorterCluster::~PathSorterCluster() = default;

const std::vector<int>& PathSorterCluster::edge_clusters() const noexcept {
    return _edge_clusters;
}

CPLEXSolveResult PathSorterCluster::solve(double) {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterCluster::generate_variables() {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterCluster::generate_constraints() {
    throw std::logic_error(NOT_IMPLEMENTED);
}
