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

    int cluster_count() const noexcept;
    const std::vector<int>& edge_clusters() const noexcept;

private:
    void generate_variables() override;
    void generate_constraints() override;
    void set_objective() override;

    void set_cluster_order_variable(int c, int d);
    void set_cluster_segment_variable(int c, int e);
    
    int _cluster_count;
    map<int, vector<int>> _edges_by_cluster;
    vector<int> _edge_clusters;
    VariableMatrix _O, _Q;

};

#endif
