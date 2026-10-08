#include <model/PathSorterClusters.h>
#include <stdexcept>
#include <utility>

namespace {
const char* const NOT_IMPLEMENTED =
    "El modelo PathSorterCluster todavia no esta implementado.";
}

PathSorterCluster::PathSorterCluster(): PathSolver() {}

PathSorterCluster::PathSorterCluster(PathSortInstance instance):
    PathSolver(std::move(instance)) {}

PathSorterCluster::~PathSorterCluster() = default;

CPLEXSolveResult PathSorterCluster::solve(double) {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterCluster::generate_variables() {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterCluster::generate_constraints() {
    throw std::logic_error(NOT_IMPLEMENTED);
}
