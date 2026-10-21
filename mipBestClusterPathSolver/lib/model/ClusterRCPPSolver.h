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
    IloNum deposit_traversal_upper_bound() const override;

    ClusterBoundary _boundary;
    long long _max_boundary_multiplicity = 1;
};

#endif
