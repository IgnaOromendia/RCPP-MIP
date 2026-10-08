#include <model/PathSorterRegion.h>
#include <stdexcept>
#include <utility>

namespace {
const char* const NOT_IMPLEMENTED =
    "El modelo PathSorterRegion todavia no esta implementado.";
}

PathSorterRegion::PathSorterRegion(): PathSorterRegion(PathSortInstance{}) {}

PathSorterRegion::PathSorterRegion(PathSortInstance instance):
    _instance(std::move(instance)) {}

PathSorterRegion::~PathSorterRegion() = default;

const PathSortInstance& PathSorterRegion::instance() const noexcept {
    return _instance;
}

const std::vector<PathEdge>& PathSorterRegion::edges() const noexcept {
    return _instance.edges;
}

CPLEXSolveResult PathSorterRegion::solve(double) {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterRegion::generate_variables() {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterRegion::generate_constraints() {
    throw std::logic_error(NOT_IMPLEMENTED);
}

void PathSorterRegion::invalidate_result() {}
