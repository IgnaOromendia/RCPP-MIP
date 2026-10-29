#ifndef NEUTRAL_EXPANDED_GRAPH_H
#define NEUTRAL_EXPANDED_GRAPH_H
#include <graph/Graph.h>
#include <map>
#include <set>
#include <vector>

class NeutralExpandedGraph : public Graph {
public:
    explicit NeutralExpandedGraph(const Instance& instance);

    const map<int, double>& original_cluster_costs() const { return _cluster_cost; }
    const vector<set<int>>& neutral_boundary() const { return _neutral_boundary; }

private:
    vector<bool> _is_neutral;
    vector<int> _expanded_copy;
    map<int, double> _cluster_cost;
    vector<set<int>> _neutral_boundary;
    map<int, int> _cluster_copy;

    void cluster_cost_and_neutral_boundaries(const Instance& instance);
    void copy_neutral_nodes(const Instance& instance);
    void connect_copies(const Instance& instance);
    
    void add_edge(int from, int to, double cost, bool bidirectional);
};
#endif
