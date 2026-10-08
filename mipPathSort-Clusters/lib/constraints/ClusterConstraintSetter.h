#ifndef CLUSTER_CONSTRAINTS_H
#define CLUSTER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/ClusterPathSortInstance.h>

class ClusterConstraintSetter: public ConstraintSetter {
public:
    ClusterConstraintSetter(const VariableMatrix& O, const VariableMatrix& Q, const IloNumVarArray& X,
                            int cluster_count, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _X(X), _O(O), _Q(Q), _cluster_count(cluster_count), _K(K) {}


    void set_cluster_constraint(const map<segment, int>& segment_map, const map<int, vector<int>>& edges_by_cluster);

private:

    const int _cluster_count, _K;
    const IloNumVarArray& _X;
    const VariableMatrix& _O, _Q;
  
};

#endif