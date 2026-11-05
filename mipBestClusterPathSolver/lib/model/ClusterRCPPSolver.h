#ifndef CLUSTER_RCPP_SOLVER_H
#define CLUSTER_RCPP_SOLVER_H

#include <model/ClusterBoundary.h>
#include <model/RCPPSolver.h>

class ClusterRCPPSolver : public RCPPSolver {
public:
    ClusterRCPPSolver(
        const SuperGraph& super_graph,
        const ClusterBoundary& boundary,
        const map<int, int>& local_to_global_node,
        ModelOptions options = {});

private:
    void generate_constraints() override;
    void set_boundary_constraints();
    bool matches_portal(int local_virtual_node, const ClusterPortal& portal) const;

    ClusterBoundary _boundary;
    map<int, int> _local_to_global_node;
};

#endif
