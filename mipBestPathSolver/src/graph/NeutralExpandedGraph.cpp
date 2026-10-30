#include <graph/NeutralExpandedGraph.h>
#include <stdexcept>

NeutralExpandedGraph::NeutralExpandedGraph(const Instance& instance)
    : Graph(instance), _is_neutral(instance.nodes, false),
      _expanded_copy(instance.nodes, -1), _neutral_boundary(instance.nodes) {

    cluster_cost_and_neutral_boundaries(instance);
    copy_neutral_nodes(instance);
    connect_copies(instance);  

    // Recalculamos adj
    _adj.assign(_n + 2, {});
    for (const Edge& edge : _all_edges) {
        _adj[edge.from].emplace_back(edge.to, edge.id);
        if (edge.is_bidirectional) _adj[edge.to].emplace_back(edge.from, edge.id);
    }

    for (const Edge& edge: _all_edges) {
        _edge_set.emplace(edge.from, edge.to);
        if (edge.is_bidirectional) _edge_set.emplace(edge.to, edge.from);
    }

    add_expanded_turns(instance);
}

void NeutralExpandedGraph::cluster_cost_and_neutral_boundaries(const Instance& instance) {
    size_t record = 0;
    auto add_to_cluster = [&](const vector<InstanceEdge>& records) {
        for (const InstanceEdge& edge : records) {
            const int cluster = instance.record_clusters[record++];
            _cluster_cost[cluster] += edge.cost;
            _neutral_boundary[edge.from].insert(cluster);
            _neutral_boundary[edge.to].insert(cluster);
        }
    };
    add_to_cluster(instance.edges);
    add_to_cluster(instance.arcs);
}

void NeutralExpandedGraph::copy_neutral_nodes(const Instance &instance) {
    for (int v : instance.neutral_nodes) {
        if (_is_neutral[v]) throw std::invalid_argument("Nodo neutro duplicado");
        _is_neutral[v] = true;
        _expanded_copy[v] = _n++;

        int left_cluster, right_cluster;
        
        double cost = 0;
        for (int cluster : _neutral_boundary[v]) {
            if (_cluster_copy.count(_expanded_copy[v]) == 0) {
                _cluster_copy[_expanded_copy[v]] = cluster;
                right_cluster = cluster;
            }
            left_cluster = cluster;
            cost += _cluster_cost.at(cluster);
        }
        
        const int in_out_id = add_edge(v, _expanded_copy[v], cost, false);
        const int out_in_id = add_edge(_expanded_copy[v], v, cost, false);

        _neutral_edges_ids.insert(in_out_id);
        _neutral_edges_ids.insert(out_in_id);
        // _expansions.emplace_back(v, _expanded_copy[v], in_out_id, out_in_id, left_cluster, right_cluster);
    }

    // Actualizamos el deposito
    const int original_deposit = _deposit;
    _deposit = _n;
    for (Edge& edge : _all_edges) {
        if (edge.from == original_deposit) edge.from = _deposit;
        if (edge.to == original_deposit) edge.to = _deposit;
    }
    _is_adj_deposit.resize(_n, false);
}

void NeutralExpandedGraph::connect_copies(const Instance &instance) {
    const size_t original_records = instance.edges.size() + instance.arcs.size();
    for (size_t id = 0; id < original_records; ++id) {
        Edge& edge = _all_edges[id];

        const bool from_neutral = _is_neutral[edge.from];
        const bool to_neutral = _is_neutral[edge.to];

        // Between copies
        if (from_neutral and to_neutral)
            add_edge(_expanded_copy[edge.from], _expanded_copy[edge.to], edge.cost, edge.is_bidirectional);

        // Copy with cluster
        const int cluster = instance.record_clusters[id];

        if (from_neutral and cluster == _cluster_copy[edge.from])
            edge.from = _expanded_copy[edge.from];
        else if (to_neutral and cluster == _cluster_copy[edge.to])
            edge.to = _expanded_copy[edge.to];
    }
}

void NeutralExpandedGraph::add_expanded_turns(const Instance& instance) {
    set<Turn> turns, illegal;

    for(const Turn& t: instance.turns) {
        vector<int> v_nodes{t.v}, w_nodes{t.w}, u_nodes{t.u};
        
        if (_is_neutral[t.v]) v_nodes.push_back(_expanded_copy[t.v]);
        if (_is_neutral[t.w]) w_nodes.push_back(_expanded_copy[t.w]);
        if (_is_neutral[t.u]) u_nodes.push_back(_expanded_copy[t.u]);

        for(int v: v_nodes)
            for(int w: w_nodes)
                for(int u: u_nodes)
                    turns.emplace(v,w,u);
    }

    _turns.assign(turns.begin(), turns.end());

    for (const Turn& t : instance.illegal_turns) {
        vector<int> v_nodes{t.v}, w_nodes{t.w}, u_nodes{t.u};
        
        if (_is_neutral[t.v]) v_nodes.push_back(_expanded_copy[t.v]);
        if (_is_neutral[t.w]) w_nodes.push_back(_expanded_copy[t.w]);
        if (_is_neutral[t.u]) u_nodes.push_back(_expanded_copy[t.u]);

        for(int v: v_nodes)
            for(int w: w_nodes)
                for(int u: u_nodes)
                    if (has_edge(v, w) and has_edge(w, u))
                        illegal.emplace(v,w,u);
    }

    _illegal_turns.assign(illegal.begin(), illegal.end());

}

int NeutralExpandedGraph::add_edge(int from, int to, double cost, bool bidirectional) {
    const int id = _all_edges.size();
    _all_edges.emplace_back(id, from, to, 0, cost, 0, -1, bidirectional);
    (bidirectional ? _edges : _arcs).push_back(id);
    return id;
}
