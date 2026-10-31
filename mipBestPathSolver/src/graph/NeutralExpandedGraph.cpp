#include <graph/NeutralExpandedGraph.h>
#include <stdexcept>

NeutralExpandedGraph::NeutralExpandedGraph(const Instance& instance)
    : Graph(instance), _is_neutral(instance.nodes, false),
      _expanded_copy(instance.nodes, -1), _neutral_boundary(instance.nodes) {

    cluster_cost_and_neutral_boundaries(instance);

    // deposit edge -> cluster -1
    // neutral edge -> cluster 0
    // graph edge -> cluster i
    _cluster_by_edge_id.assign(_all_edges.size(), -1);
    std::copy(
        instance.record_clusters.begin(),
        instance.record_clusters.end(),
        _cluster_by_edge_id.begin());

    _original_edge_by_expanded_edge.assign(_all_edges.size(), -1);
    const size_t original_records = instance.edges.size() + instance.arcs.size();
    for (size_t id = 0; id < original_records; ++id)
        _original_edge_by_expanded_edge[id] = id;

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

GraphSolution NeutralExpandedGraph::contract_solution(const GraphSolution& expanded) const {
    GraphSolution contracted;
    contracted.objective = expanded.objective;
    contract_values(expanded.service, contracted.service);
    contract_values(expanded.traversals, contracted.traversals);
    return contracted;
}

void NeutralExpandedGraph::contract_values(
        const map<int, long long>& expanded,
        map<int, long long>& contracted) const {
    for (const auto& [expanded_edge_id, multiplicity] : expanded) {
        const int original_edge_id = _original_edge_by_expanded_edge.at(expanded_edge_id);
        if (original_edge_id >= 0 && multiplicity != 0)
            contracted[original_edge_id] += multiplicity;
    }
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
        
        const int in_out_id = add_edge(0, v, _expanded_copy[v], cost, false);
        const int out_in_id = add_edge(0, _expanded_copy[v], v, cost, false);

        _neutral_edges_ids.insert(in_out_id);
        _neutral_edges_ids.insert(out_in_id);
        // _expansions.emplace_back(v, _expanded_copy[v], in_out_id, out_in_id, left_cluster, right_cluster);
    }

    // Actualizamos el deposito
    const int original_deposit = _deposit;
    _deposit = _n;
    const size_t first_deposit_edge = instance.edges.size() + instance.arcs.size();
    const size_t after_deposit_edges = first_deposit_edge + instance.deposit_nodes.size();
    for (size_t id = first_deposit_edge; id < after_deposit_edges; ++id) {
        Edge& edge = _all_edges[id];
        if (edge.from == original_deposit) edge.from = _deposit;
        if (edge.to == original_deposit) edge.to = _deposit;
    }
    _is_adj_deposit.resize(_n, false);
}

void NeutralExpandedGraph::connect_copies(const Instance &instance) {
    const size_t original_records = instance.edges.size() + instance.arcs.size();
    for (size_t id = 0; id < original_records; ++id) {
        const Edge original = _all_edges[id];

        const bool from_neutral = _is_neutral[original.from];
        const bool to_neutral = _is_neutral[original.to];
        const int cluster = instance.record_clusters[id];

        // Keep the original record in P and add its complete copy to P'.
        if (from_neutral and to_neutral) {
            add_edge(cluster, _expanded_copy[original.from], _expanded_copy[original.to],
                     original.cost, original.is_bidirectional, original.id);
            continue;
        }

        // Copy with cluster
        Edge& edge = _all_edges[id];
        if (from_neutral and cluster == _cluster_copy.at(_expanded_copy[original.from]))
            edge.from = _expanded_copy[original.from];
        else if (to_neutral and cluster == _cluster_copy.at(_expanded_copy[original.to]))
            edge.to = _expanded_copy[original.to];
    }
}

void NeutralExpandedGraph::add_expanded_turns(const Instance& instance) {
    auto expand_turns = [&](const vector<Turn>& original) {
        set<Turn> expanded;

        for (const Turn& turn : original) {
            vector<int> from_nodes{turn.v};
            vector<int> middle_nodes{turn.w};
            vector<int> to_nodes{turn.u};

            if (_is_neutral[turn.v]) from_nodes.push_back(_expanded_copy[turn.v]);
            if (_is_neutral[turn.w]) middle_nodes.push_back(_expanded_copy[turn.w]);
            if (_is_neutral[turn.u]) to_nodes.push_back(_expanded_copy[turn.u]);

            for (int from : from_nodes)
                for (int middle : middle_nodes)
                    for (int to : to_nodes)
                        if (has_edge(from, middle) and has_edge(middle, to))
                            expanded.emplace(from, middle, to);
        }

        return vector<Turn>(expanded.begin(), expanded.end());
    };

    _turns = expand_turns(instance.turns);
    _illegal_turns = expand_turns(instance.illegal_turns);
}

int NeutralExpandedGraph::add_edge(
        int cluster, int from, int to, double cost, bool bidirectional,
        int original_edge_id) {
    const int id = _all_edges.size();
    _all_edges.emplace_back(id, from, to, 0, cost, 0, -1, bidirectional);
    _cluster_by_edge_id.push_back(cluster);
    _original_edge_by_expanded_edge.push_back(original_edge_id);
    (bidirectional ? _edges : _arcs).push_back(id);
    return id;
}
