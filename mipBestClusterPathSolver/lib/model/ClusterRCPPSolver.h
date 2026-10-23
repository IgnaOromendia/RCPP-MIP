#ifndef CLUSTER_RCPP_SOLVER_H
#define CLUSTER_RCPP_SOLVER_H

#include <model/ClusterBoundary.h>
#include <model/RCPPSolver.h>

class ClusterRCPPSolver : public RCPPSolver {
public:
    ClusterRCPPSolver(
        const SuperGraph& super_graph,
        const ClusterBoundary& boundary,
        ModelOptions options = {});

private:
    void generate_constraints() override;
    void set_boundary_constraints();

    ClusterBoundary _boundary;
};

#endif
