#include <model/PathSorterClusters.h>
#include <stdexcept>
#include <utility>

namespace {
const char* const NOT_IMPLEMENTED =
    "El modelo PathSorterCluster todavia no esta implementado.";
}

PathSorterCluster::PathSorterCluster(): PathSorterCluster(PathSortInstance{}) {}

PathSorterCluster::PathSorterCluster(PathSortInstance instance):
    _instance(std::move(instance)) {}

PathSorterCluster::~PathSorterCluster() = default;

const PathSortInstance& PathSorterCluster::instance() const noexcept {
    return _instance;
}

const std::vector<PathEdge>& PathSorterCluster::edges() const noexcept {
    return _instance.edges;
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

void PathSorterCluster::invalidate_result() {}
