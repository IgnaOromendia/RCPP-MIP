#ifndef PATH_SORTER_CLUSTERS_H
#define PATH_SORTER_CLUSTERS_H

#include <model/PathSolver.h>
#include <model/ClusterPathSortInstance.h>

class WarmStart;

class PathSorterCluster : public PathSolver {
public:
    PathSorterCluster();
    explicit PathSorterCluster(
        ClusterPathSortInstance instance,
        int hierholzer_lookahead = 1,
        int hierholzer_branch_width = 8);
    ~PathSorterCluster() override;
    PathSorterCluster(const PathSorterCluster&) = delete;
    PathSorterCluster& operator=(const PathSorterCluster&) = delete;
    PathSorterCluster(PathSorterCluster&&) = delete;
    PathSorterCluster& operator=(PathSorterCluster&&) = delete;

    int cluster_count() const noexcept;
    const std::vector<int>& edge_clusters() const noexcept;
    std::vector<OrderedPass> hierholzer_order() const;
    bool complete_hierholzer_warm_start();

private:
    friend struct PathSorterClusterTestAccess;
    friend class WarmStart;

    using WarmStartBounds = std::vector<VariableBounds>;

    void generate_variables() override;
    void generate_constraints() override;
    void set_objective() override;
    void generate_warm_start() override;

    int _cluster_count;
    vector<int> _edge_clusters;
    std::vector<int> _warm_start_order;
    int _hierholzer_lookahead = 1;

};

#endif
