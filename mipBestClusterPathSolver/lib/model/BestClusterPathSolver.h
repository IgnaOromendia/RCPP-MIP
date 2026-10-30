#ifndef BEST_CLUSTER_PATH_SOLVER_H
#define BEST_CLUSTER_PATH_SOLVER_H

#include <model/RCPPSolver.h>
#include <set>
#include <vector>

class BestClusterPathSolver : public RCPPSolver {
public:
    BestClusterPathSolver(
        const SuperGraph& super_graph,
        const set<int>& neutral_edges,
        ModelOptions options = {});

    const vector<int>& get_neutral_arc_ids() const { return _neutral_arc_ids; };

private:
    friend struct BestClusterPathSolverTestAccess;

    vector<int> _neutral_arc_ids;

    void set_objective() override;

};

#endif
