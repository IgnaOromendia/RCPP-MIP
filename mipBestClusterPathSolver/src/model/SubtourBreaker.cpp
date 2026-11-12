#include <model/SubtourBreaker.h>
#include <algorithm>
#include <stdexcept>

// Positive support only: multiplicities are not expanded during classification.
vector<int> SubtourBreaker::components(const SuperGraph& graph) const {
    const int deposit = graph.deposit();
    vector<vector<int>> adjacency(deposit + 1);
    auto add = [&](int from, int to, long long count) {
        if (count <= 0) return;
        if (from == -1) from = deposit;
        if (to == -1) to = deposit;
        adjacency[from].push_back(to);
        adjacency[to].push_back(from);
    };
    for (const auto* values : {&_solution.service, &_solution.traversals,
                              &_solution.deposit_traversals})
        for (const auto& value : *values) add(value.from, value.to, value.value);
    for (const auto& link : _connections) add(link.from, link.to, link.count);

    vector<int> component(deposit + 1, -1);
    // Classify the depot first, including an empty depot component.
    vector<int> seeds{deposit};
    for (int node = 0; node < deposit; ++node)
        if (!adjacency[node].empty()) seeds.push_back(node);
    for (int seed : seeds) {
        if (component[seed] != -1) continue;
        component[seed] = seed;
        vector<int> pending{seed};
        for (size_t i = 0; i < pending.size(); ++i)
            for (int next : adjacency[pending[i]])
                if (component[next] == -1) {
                    component[next] = seed;
                    pending.push_back(next);
                }
    }
    return component;
}

SubtourBreaker::SubtourBreaker(const SuperGraph& graph, const NeutralExpandedGraph& expanded,
        const Solution& solution, const vector<int>& neutral_arc_ids)
        : _solution(solution) {
    vector<bool> neutral(graph.arcs_amount(), false);
    for (int id : neutral_arc_ids) neutral[id] = true;
    // Index Y once; X is never changed by an exchange.
    vector<int> traversal_index(graph.arcs_amount(), -1);
    for (size_t i = 0; i < _solution.traversals.size(); ++i)
        traversal_index[_solution.traversals[i].id] = i;

    vector<int> cluster(graph.nodes_amount(), 0);
    vector<int> street_other_end(graph.nodes_amount(), -1);
    map<EdgeKey, int> direct;
    for (const auto& arc : graph.arcs()) {
        if (arc.edge_id == -1) direct.emplace(EdgeKey(arc.from, arc.to), arc.id);
        if (arc.edge_id < 0 || arc.is_neutral) continue;
        cluster[arc.from] = cluster[arc.to] = expanded.edge_cluster(arc.edge_id);
        street_other_end[arc.from] = graph.original_node(arc.to);
        street_other_end[arc.to] = graph.original_node(arc.from);
    }
    // Same no-U rule as SuperGraph, including representations of one neutral.
    set<EdgeKey> aliases;
    for (int id : expanded.get_neutral_edges()) {
        const auto& edge = *expanded.edge_with_id(id);
        aliases.emplace(edge.from, edge.to);
    }
    auto compatible = [&](int from, int to) {
        const int before = street_other_end[from], after = street_other_end[to];
        return cluster[from] == cluster[to] && before != after &&
            !aliases.count({before, after}) && !aliases.count({after, before});
    };
    auto add_link = [&](int from, int to, long long count) {
        const auto found = direct.find({from, to});
        if (found == direct.end()) {
            _connections.push_back({from, to, count});
            return;
        }
        int& index = traversal_index[found->second];
        if (index == -1) {
            index = _solution.traversals.size();
            _solution.traversals.emplace_back(*graph.super_arc_with_id(found->second), 0);
        }
        _solution.traversals[index].value += count;
    };

    while (true) {
        const auto component = components(graph);
        set<int> with_crossings;
        for (const auto& value : _solution.traversals)
            if (value.value > 0 && neutral[value.id])
                with_crossings.insert(component[value.from]);
        // Remove whole balanced components, including services and turn arcs.
        for (auto* values : {&_solution.service, &_solution.traversals})
            for (auto& value : *values)
                if (value.value > 0 && component[value.from] != graph.deposit() &&
                    !with_crossings.count(component[value.from])) value.value = 0;
        for (auto& link : _connections)
            if (link.count > 0 && component[link.from] != graph.deposit() &&
                !with_crossings.count(component[link.from])) link.count = 0;

        with_crossings.erase(graph.deposit());
        if (with_crossings.empty()) break;
        const int target = *with_crossings.begin();
        bool exchanged = false;
        // Opposite cluster orientations let the two new links stay internal.
        // A pair may split the current component; classification then discards
        // the resulting crossing-free fragments or processes surviving ones.
        for (int first : neutral_arc_ids) {
            int ai = traversal_index[first];
            if (ai == -1 || _solution.traversals[ai].value == 0) continue;
            const auto a = _solution.traversals[ai];
            if (component[a.from] != target) continue;
            for (int second : neutral_arc_ids) {
                int bi = traversal_index[second];
                if (bi == -1 || _solution.traversals[bi].value == 0) continue;
                const auto b = _solution.traversals[bi];
                if (!compatible(a.from, b.to) || !compatible(b.from, a.to)) continue;
                const auto count = min(a.value, b.value);
                _solution.traversals[ai].value -= count;
                _solution.traversals[bi].value -= count;
                add_link(a.from, b.to, count);
                add_link(b.from, a.to, count);
                _repaired = exchanged = true;
                break;
            }
            if (exchanged) break;
        }
        if (!exchanged)
            throw invalid_argument("No se puede reconectar la componente con cruces " + to_string(target));
    }
    _solution.objective = 0;
    for (const auto& value : _solution.traversals)
        if (neutral[value.id]) _solution.objective += value.value;
}
