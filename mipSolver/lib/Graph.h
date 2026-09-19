#ifndef GRAPH_H
#define GRAPH_H

#include "SuperGraph.h"

using namespace std;

class Graph {
    public:

    // Default construction leaves an unbuilt graph (deposit == -1).
    Graph() = default;
    Graph(string file_name);
    ~Graph();

    // Nodes
    int deposit() const;
    int nodes_amount() const;
    const vector<Node>& neighbours(int v) const;
    
    // Edges
    int requested_amount() const;
    const vector<const Edge*> requested() const;
    const Edge* edge_with_id(int id) const;
    
    private:
    friend class SuperGraph;

    int _n = 0, _adj_deposit_amount = 0, _deposit = -1;
    // Edge::id indexes _all_edges: undirected edges, directed arcs, then deposit edges.
    // Positions in _arcs and _edges are local; their Edge::id remains global.
    vector<Edge> _all_edges, _arcs, _edges;
    vector<int> _requested_idx_to_edge; // Required index -> global Edge::id.
    vector<vector<Node> > _adj;

    vector<bool> _is_adj_deposit;
    vector<int> _adj_depoist_nodes;

};


#endif
