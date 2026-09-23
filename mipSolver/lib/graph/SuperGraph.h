#ifndef SUPERGRAPH_H
#define SUPERGRAPH_H
 
#include <fstream>
#include <iostream>
#include "../util/HashMap.h"
#include "Edges.h"
#include "Turn.h"

using namespace std;

typedef pair<int, int> EdgeKey;

struct Edge;
struct Turn;
class Graph;

class SuperGraph {
public:

    // Default construction leaves an unbuilt graph (deposit == -1).
    SuperGraph() noexcept = default;
    SuperGraph(const Graph& graph, const vector<Turn>& turns, const vector<Turn>& illegal_turns);
    SuperGraph(const SuperGraph&) = delete;
    SuperGraph& operator=(const SuperGraph&) = delete;

    // After moving, the source may only be destroyed or assigned a new graph.
    SuperGraph(SuperGraph&& other) noexcept = default;
    SuperGraph& operator=(SuperGraph&& other) noexcept = default;
    ~SuperGraph() = default;

    // Deposit
    int deposit() const;

    // Amounts
    int nodes_amount() const;
    int arcs_amount() const;

    // Arcs
    const vector<SuperArc>& arcs() const;
    const SuperArc* super_arc_with_id(int arc_id) const;
    
    const vector<const SuperArc*> super_arcs_from(int v) const;
    const vector<const SuperArc*> super_arcs_to(int v) const;

    const vector<const SuperArc*> super_arcs_adj_depo_node() const;
    const vector<const SuperArc*> super_arcs_adj_node_depo() const;

    // BFS radius d >= 0 over outgoing arcs, excluding the deposit. Root comes first.
    vector<EdgeKey> random_edge_neighborhood(int d) const;
    vector<EdgeKey> bfs_tree(int start, int d) const;

    // Deposit adjacents
    bool is_adj_depo_node(int v) const;
    bool is_adj_node_depo(int v) const;

    // Distances
    void calculate_depo_dists();
    const vector<double>& deposit_dist() const;
    
    // Testing
    void write();

private:

    // Generation
    void add_super_arc_from_edge(const Graph* graph, const Edge& edge);
    void add_super_arc_from_arc(const Graph* graph, const Edge& edge);
    void add_super_arcs_between_nodes(const vector<Turn> &turns, const vector<Turn> &illegal_turns);
    void add_super_arcs_with_deposit();

    void add_new_arc(const Graph* graph, const Edge& edge, int pair_id = -1, bool invert_direction = false);
    void add_adj_depo(const Graph* graph, int from, int to);
    void add_node_map(int from, int to);
    void add_to_adj_list(int from, int to, double cost);

    const vector<const SuperArc*> super_arcs_for_ids(const vector<int>& node_ids) const;

    int _m = 0, _n = 0, _deposit = -1;
    vector<SuperArc> _arcs;
    vector<vector<int> > _original_to_virtual;
    vector<vector<Node> > _adj;
    vector<int> _virtual_to_original, _adj_deposit_node, _adj_node_deposit, _is_adj_deposit_node, _is_adj_node_deposit;
    vector<double> _depo_dist;
    HashMap _node_to_super_in, _node_to_super_out;
};

#endif
