#ifndef GRAPH_H
#define GRAPH_H

#include "Edges.h"
#include "../model/Instance.h"
#include <vector>

using namespace std;

class Graph {
public:

    // Default construction leaves an unbuilt graph (deposit == -1).
    Graph() = default;
    explicit Graph(const Instance& instance);
    virtual ~Graph();

    // Nodes
    int deposit() const;
    virtual bool allows_deposit_access(int record_id) const;
    int nodes_amount() const;
    const vector<Node>& neighbours(int v) const;
    
    // Edges
    int requested_amount() const { return this->_requested_idx_to_edge.size(); };
    const vector<const Edge*> requested() const;
    const Edge* edge_with_id(int id) const { return &this->_all_edges[id]; }
    virtual bool is_neutral_edge(int e) const { return false; }
    
protected:
    friend class SuperGraph;

    int _n = 0, _adj_deposit_amount = 0, _deposit = -1;
    // Edge::id indexes _all_edges: undirected edges, directed arcs, then deposit edges.
    // _all_edges owns the records; _arcs and _edges store global IDs into it.
    vector<Edge> _all_edges;
    vector<int> _arcs, _edges;
    vector<int> _requested_idx_to_edge; // Required index -> global Edge::id.
    vector<vector<Node> > _adj;

    vector<bool> _is_adj_deposit;
    vector<int> _adj_depoist_nodes;

};


#endif
