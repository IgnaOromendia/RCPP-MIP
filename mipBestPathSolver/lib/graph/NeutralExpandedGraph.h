#ifndef NEUTRAL_EXPANDED_GRAPH_H
#define NEUTRAL_EXPANDED_GRAPH_H
#include <graph/Graph.h>
#include <model/Solution.h>
#include <map>
#include <set>

class SuperGraph;

class NeutralExpandedGraph : public Graph {
public:
    explicit NeutralExpandedGraph(const Instance& instance);

    const map<int, double>& original_cluster_costs() const { return _cluster_cost; }
    const vector<set<int>>& neutral_boundary() const { return _neutral_boundary; }
    const vector<Turn>& get_turns() const {return _turns; };
    const vector<Turn>& get_illegal_turns() const {return _illegal_turns; };
    const set<ExtendedTurn>& get_illegal_ext_turns() const { return _illegal_ext_turns; };
    const set<int>& get_neutral_edges() const { return _neutral_edges_ids; };
    const vector<int>& expanded_edge_ids() const noexcept { return _edges; }
    const vector<int>& expanded_arc_ids() const noexcept { return _arcs; }
    bool edge_has_cluster(int e, int c) const { return edge_cluster(e) == c; };
    int edge_cluster(int e) const { return _cluster_by_edge_id.at(e); };
    int edges_amount() const { return _cluster_by_edge_id.size(); };
    bool is_neutral(int e) const { return _neutral_edges_ids.count(e) != 0; };
    bool is_neutral_edge(int e) const override { return is_neutral(e); };
    GraphSolution contract_solution(const GraphSolution& expanded) const;
    Solution contract_solution(const Solution& expanded, const SuperGraph& expanded_super,
                               const SuperGraph& original_super) const;

private:
    vector<bool> _is_neutral;
    vector<int> _cluster_by_edge_id;
    vector<int> _original_edge_by_expanded_edge;

    vector<map<int, int>> _neutral_expansion_by_cluster;

    set<int> _neutral_edges_ids;
    vector<set<int>> _neutral_boundary;

    map<int, double> _cluster_cost;

    vector<Turn> _turns, _illegal_turns;
    set<ExtendedTurn> _illegal_ext_turns;

    edgeKeySet _edge_set;
    

    void cluster_cost_and_neutral_boundaries(const Instance& instance);
    void expand_neutral_nodes(const Instance& instance);
    void connect_expanded_nodes(const Instance& instance);
    void add_expanded_turns(const Instance& instance);
    int representation(int node, int cluster) const;
    vector<int> representations_of(int node) const;
    
    int add_edge(int cluster, int from, int to, double cost, bool bidirectional,
                 int original_edge_id = -1);
    bool has_edge(int from, int to) const { return _edge_set.count({from, to}) != 0; }
    void contract_values(const map<int, long long>& expanded,
                         map<int, long long>& contracted) const;
};
#endif
