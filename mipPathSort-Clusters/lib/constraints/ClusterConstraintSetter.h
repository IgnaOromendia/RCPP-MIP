#ifndef CLUSTER_CONSTRAINTS_H
#define CLUSTER_CONSTRAINTS_H

#include <ConstraintSetter.hpp>
#include <model/ClusterPathSortInstance.h>

class ClusterConstraintSetter: public ConstraintSetter {
public:
    ClusterConstraintSetter(const VariableMatrix& O, const VariableMatrix& Q, const IloNumVarArray& X, const IloNumVarArray& L,
                            const map<int, vector<int>>& edges_by_cluster, const map<segment, int> &segment_map,
                            int cluster_count, int K, IloEnv& env, IloModel& model)
        : ConstraintSetter(env, model), _X(X), _L(L), _O(O), _Q(Q),
            _cluster_count(cluster_count), _K(K), _edges_by_cluster(edges_by_cluster), _segment_map(segment_map) {}


    void set_cluster_constraint();
    void set_cluster_max_position_constraint();

private:

    const map<int, vector<int>> & _edges_by_cluster;
    const map<segment, int>& _segment_map;
    const int _cluster_count, _K;
    const IloNumVarArray& _X, _L;
    const VariableMatrix& _O, _Q;
  
};

#endif
