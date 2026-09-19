#ifndef GRAPH_H
#define GRAPH_H

#include "SuperGraph.h"

using namespace std;

class Graph {
    public:

    Graph();
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

    int _n, _adj_deposit_amount, _deposit;
    vector<Edge> _all_edges, _arcs, _edges;
    vector<int> _requested_idx_to_edge;
    vector<vector<Node> > _adj;

    vector<bool> _is_adj_deposit;
    vector<int> _adj_depoist_nodes;

};


#endif