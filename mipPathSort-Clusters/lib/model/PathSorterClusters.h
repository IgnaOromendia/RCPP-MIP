#ifndef PATH_SORTER_CLUSTERS_H
#define PATH_SORTER_CLUSTERS_H

#include <model/PathSolver.h>
#include <model/ClusterPathSortInstance.h>

class PathSorterCluster : public PathSolver {
public:
    PathSorterCluster();
    explicit PathSorterCluster(ClusterPathSortInstance instance);
    ~PathSorterCluster() override;
    PathSorterCluster(const PathSorterCluster&) = delete;
    PathSorterCluster& operator=(const PathSorterCluster&) = delete;
    PathSorterCluster(PathSorterCluster&&) = delete;
    PathSorterCluster& operator=(PathSorterCluster&&) = delete;

    CPLEXSolveResult solve(double gapTolerance = 0);
    const std::vector<int>& edge_clusters() const noexcept;

private:
    void generate_variables() override;
    void generate_constraints() override;
    std::vector<int> _edge_clusters;
};

#endif
