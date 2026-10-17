#ifndef BEST_CLUSTER_PATH_SOLVER_H
#define BEST_CLUSTER_PATH_SOLVER_H

#include <model/RCPPSolver.h>
#include <map>
#include <vector>

class BestClusterPathSolver : public RCPPSolver {
public:
    BestClusterPathSolver(
        const SuperGraph& super_graph,
        int vehicles,
        const std::map<int, std::vector<EdgeKey>>& in_out_edges_by_cluster,
        ModelOptions options = {});

    const std::map<int, std::vector<int>>& in_out_arc_ids_by_cluster() const noexcept;

private:
    friend struct BestClusterPathSolverTestAccess;

    void set_objective() override;

    std::map<int, std::vector<int>> _in_out_arc_ids_by_cluster;
};

#endif
