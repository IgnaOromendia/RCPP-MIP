#ifndef PATH_SORTER_REGION_H
#define PATH_SORTER_REGION_H

#include <CPLEXSolver.h>
#include <model/PathSortInstance.h>
#include <vector>

class PathSorterRegion : public CPLEXSolver {
public:
    PathSorterRegion();
    explicit PathSorterRegion(PathSortInstance instance);
    ~PathSorterRegion() override;
    PathSorterRegion(const PathSorterRegion&) = delete;
    PathSorterRegion& operator=(const PathSorterRegion&) = delete;
    PathSorterRegion(PathSorterRegion&&) = delete;
    PathSorterRegion& operator=(PathSorterRegion&&) = delete;

    const PathSortInstance& instance() const noexcept;
    const std::vector<PathEdge>& edges() const noexcept;
    CPLEXSolveResult solve(double gapTolerance = 0);

private:
    void generate_variables() override;
    void generate_constraints() override;
    void invalidate_result() override;

    PathSortInstance _instance;
};

#endif
