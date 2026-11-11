#include <graph/NeutralExpandedGraph.h>
#include <graph/SuperGraph.h>
#include <stdexcept>

NeutralExpandedGraph::NeutralExpandedGraph(const Instance& instance)
    : Graph(instance), _is_neutral(instance.nodes, false),
      _neutral_boundary(instance.nodes) {

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

    expand_neutral_nodes(instance);
    connect_expanded_nodes(instance);

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

Solution NeutralExpandedGraph::contract_solution(
        const Solution& expanded, const SuperGraph& expanded_super,
        const SuperGraph& original_super) const {
    // Physical orientations own their virtual endpoints. Copies collapse onto
    // the same original orientation; crossings become ordinary original turns.
    map<pair<int, bool>, const SuperArc*> original_orientations;
    map<EdgeKey, const SuperArc*> original_connections;
    for (const SuperArc& arc : original_super.arcs()) {
        original_connections.emplace(EdgeKey(arc.from, arc.to), &arc);
        if (arc.edge_id >= 0)
            original_orientations.emplace(
                make_pair(arc.edge_id, arc.pair >= 0 && arc.id > arc.pair), &arc);
    }
    map<int, int> nodes{{expanded_super.deposit(), original_super.deposit()}};
    for (const SuperArc& arc : expanded_super.arcs()) {
        if (arc.edge_id < 0) continue;
        const int original = _original_edge_by_expanded_edge.at(arc.edge_id);
        if (original < 0) continue;
        const SuperArc& target = *original_orientations.at(
            {original, arc.pair >= 0 && arc.id > arc.pair});
        nodes[arc.from] = target.from;
        nodes[arc.to] = target.to;
    }

    Solution contracted;
    contracted.objective = expanded.objective;
    auto project = [&](const auto& source, auto& destination) {
        using Value = decltype(source.front().value);
        map<pair<int, int>, Value> totals;
        for (const auto& value : source) {
            if (value.value == 0) continue;
            const SuperArc& arc = *expanded_super.super_arc_with_id(value.id);
            const SuperArc& target = *original_connections.at(
                {nodes.at(arc.from), nodes.at(arc.to)});
            totals[{target.id, value.vehicle}] += value.value;
        }
        for (const auto& [key, value] : totals) {
            const SuperArc& arc = *original_super.super_arc_with_id(key.first);
            destination.emplace_back(arc.id,
                arc.from == original_super.deposit() ? -1 : arc.from,
                arc.to == original_super.deposit() ? -1 : arc.to, key.second, value);
        }
    };
    project(expanded.service, contracted.service);
    project(expanded.traversals, contracted.traversals);
    project(expanded.deposit_traversals, contracted.deposit_traversals);
    project(expanded.flow, contracted.flow);
    project(expanded.deposit_flow, contracted.deposit_flow);
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
            if (_neutral_expansion_by_cluster.size() <= static_cast<size_t>(cluster))
                _neutral_expansion_by_cluster.resize(cluster + 1);
            _cluster_cost[cluster] += edge.cost;
            _neutral_boundary[edge.from].insert(cluster);
            _neutral_boundary[edge.to].insert(cluster);
        }
    };
    add_to_cluster(instance.edges);
    add_to_cluster(instance.arcs);
}

void NeutralExpandedGraph::expand_neutral_nodes(const Instance &instance) {
    for (int v : instance.neutral_nodes) {
        if (_is_neutral[v]) throw std::invalid_argument("Nodo neutro duplicado");
        _is_neutral[v] = true;

        vector<pair<int, int>> representations;
        bool first = true;
        for (int cluster : _neutral_boundary[v]) {
            const int expansion = first ? v : _n++;
            _neutral_expansion_by_cluster[cluster][v] = expansion;
            representations.emplace_back(cluster, expansion);

            if (!first) {
                _is_neutral.push_back(true);
                _is_adj_deposit.push_back(_is_adj_deposit[v]);
            }
            first = false;
        }

        for (const auto& [from_cluster, from_node] : representations) {
            for (const auto& [to_cluster, to_node] : representations) {
                if (from_cluster == to_cluster) continue;

                const double cost = _cluster_cost.at(from_cluster) + _cluster_cost.at(to_cluster);
                const int expanded_id = add_edge(0, from_node, to_node, cost, false);
                _neutral_edges_ids.insert(expanded_id);
            }
        }
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

void NeutralExpandedGraph::connect_expanded_nodes(const Instance &instance) {
    const size_t original_records = instance.edges.size() + instance.arcs.size();
    for (size_t id = 0; id < original_records; ++id) {
        // Appending copies may reallocate _all_edges: keep the original by value.
        const Edge edge = _all_edges[id];
        const int cluster = instance.record_clusters[id];
        _all_edges[id].from = representation(edge.from, cluster);
        _all_edges[id].to = representation(edge.to, cluster);

        if (!_is_neutral[edge.from] || !_is_neutral[edge.to]) continue;
        for (int other : _neutral_boundary[edge.from]) {
            if (other == cluster || !_neutral_boundary[edge.to].count(other)) continue;
            add_edge(other, representation(edge.from, other),
                     representation(edge.to, other), edge.cost,
                     edge.is_bidirectional, id);
        }
    }
}

int NeutralExpandedGraph::representation(int node, int cluster) const {
    if (!_is_neutral[node]) return node;
    return _neutral_expansion_by_cluster[cluster].at(node);
}

vector<int> NeutralExpandedGraph::representations_of(int node) const {
    if (!_is_neutral[node]) return {node};

    vector<int> representations;
    representations.reserve(_neutral_boundary[node].size());
    for (int cluster : _neutral_boundary[node])
        representations.push_back(representation(node, cluster));
    return representations;
}

void NeutralExpandedGraph::add_expanded_turns(const Instance& instance) {
    auto expand_turns = [&](const vector<Turn>& original, bool illegal = false) {
        set<Turn> expanded;

        for (const Turn& turn : original) {
            const vector<int> from_nodes = representations_of(turn.v);
            const vector<int> middle_nodes = representations_of(turn.w);
            const vector<int> to_nodes = representations_of(turn.u);

            for (int from : from_nodes)
                for (int wi : middle_nodes)
                    for (int wj : middle_nodes)
                        for (int to : to_nodes) {
                            if (!has_edge(from, wi) || !has_edge(wj, to))
                                continue;

                            if (wi == wj)
                                expanded.emplace(from, wi, to);
                            else if (illegal && has_edge(wi, wj))
                                _illegal_ext_turns.emplace(from, wi, wj, to);
                        }
        }

        return vector<Turn>(expanded.begin(), expanded.end());
    };

    _turns = expand_turns(instance.turns);
    _illegal_turns = expand_turns(instance.illegal_turns, true);
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
