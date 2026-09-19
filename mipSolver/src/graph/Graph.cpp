#include "../../lib/graph/Graph.h"

Graph::Graph(const Instance& instance) {
    instance.validate();
    _n = instance.nodes;
    _deposit = _n;
    _adj_deposit_amount = static_cast<int>(instance.deposit_nodes.size());
    _adj_depoist_nodes = instance.deposit_nodes;
    _is_adj_deposit.assign(_n, false);
    _adj.resize(_n + 2);
    _all_edges.reserve(instance.edges.size() + instance.arcs.size() + instance.deposit_nodes.size());
    _edges.reserve(instance.edges.size());
    _arcs.reserve(instance.arcs.size());
    for (int v : _adj_depoist_nodes) _is_adj_deposit[v] = true;

    auto add_edges = [&](const auto& records, auto& destination, bool bidirectional) {
        for (const auto& record : records) {
            const int id = static_cast<int>(_all_edges.size());
            const int requested = record.zone != 0 ? static_cast<int>(_requested_idx_to_edge.size()) : -1;
            if (record.zone != 0) _requested_idx_to_edge.push_back(id);
            _all_edges.emplace_back(id, record.from, record.to, record.zone,
                                    record.cost, record.demand, requested, bidirectional);
            destination.push_back(_all_edges.back());
            _adj[record.from].emplace_back(record.to, id);
            if (bidirectional) _adj[record.to].emplace_back(record.from, id);
        }
    };
    add_edges(instance.edges, _edges, true);
    add_edges(instance.arcs, _arcs, false);
    for (int v : _adj_depoist_nodes) {
        const int id = static_cast<int>(_all_edges.size());
        _adj[_deposit].emplace_back(v, id);
        _adj[v].emplace_back(_deposit, id);
        _all_edges.emplace_back(id, _deposit, v, 0, 0, 0, -1, true);
    }
}

Graph::~Graph() {}

int Graph::nodes_amount() const {
    return this->_n;
}

int Graph::deposit() const {
    return this->_deposit;
}

const vector<Node>& Graph::neighbours(int v) const {
    return this->_adj[v];
}

int Graph::requested_amount() const {
    return this->_requested_idx_to_edge.size();
}

const vector<const Edge*> Graph::requested() const {
    vector<const Edge*> result;
    result.reserve(this->_requested_idx_to_edge.size()); 

    for(int id: this->_requested_idx_to_edge)
        result.push_back(&this->_all_edges[id]);
    
        
    return result;
}

const Edge* Graph::edge_with_id(int id) const {
    return &this->_all_edges[id];
}
