#ifndef NEUTRAL_EXPANDED_GRAPH_H
#define NEUTRAL_EXPANDED_GRAPH_H
#include <graph/Graph.h>
#include <map>
#include <set>
#include <vector>

struct InOutEdgeClusters {
    int id, source_cluster, target_cluster;
};

struct NeutralExpansion {
    int nodel;
    int copy_node;
    int in_out_id;
    int out_in_id;
    int left_cluster;
    int right_cluster;
};

class NeutralExpandedGraph : public Graph {
public:
    explicit NeutralExpandedGraph(const Instance& instance);

    const map<int, double>& original_cluster_costs() const { return _cluster_cost; }
    const vector<set<int>>& neutral_boundary() const { return _neutral_boundary; }
    const vector<Turn>& get_turns() const {return _turns; };
    const vector<Turn>& get_illegal_turns() const {return _illegal_turns; };
    const set<int>& get_neutral_edges() const { return _neutral_edges_ids; };

private:
    vector<bool> _is_neutral;
    vector<int> _expanded_copy;
    map<int, double> _cluster_cost;
    vector<set<int>> _neutral_boundary;
    map<int, int> _cluster_copy;
    vector<Turn> _turns, _illegal_turns;
    edgeKeySet _edge_set;
    set<int> _neutral_edges_ids;
    vector<NeutralExpansion> _expansions;

    void cluster_cost_and_neutral_boundaries(const Instance& instance);
    void copy_neutral_nodes(const Instance& instance);
    void connect_copies(const Instance& instance);
    void add_expanded_turns(const Instance& instance);
    
    int add_edge(int from, int to, double cost, bool bidirectional);
    bool has_edge(int from, int to) { return _edge_set.count({from, to}) != 0; }
};
#endif
