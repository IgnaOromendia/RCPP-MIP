#ifndef PATH_SORTER_CLUSTERS_H
#define PATH_SORTER_CLUSTERS_H

#include <CPLEXSolver.h>
#include <model/PathSortInstance.h>
#include <vector>

class PathSorterCluster : public CPLEXSolver {
public:
    PathSorterCluster();
    explicit PathSorterCluster(PathSortInstance instance);
    ~PathSorterCluster() override;
    PathSorterCluster(const PathSorterCluster&) = delete;
    PathSorterCluster& operator=(const PathSorterCluster&) = delete;
    PathSorterCluster(PathSorterCluster&&) = delete;
    PathSorterCluster& operator=(PathSorterCluster&&) = delete;

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
