#include <heuristic/ClusterGuidedHierholzer.h>
#include <stdexcept>

ClusterGuidedHierholzer::ClusterGuidedHierholzer(
    const PathSortInstance& instance,
    const std::vector<int>& edge_clusters,
    const std::map<segment, int>& segment_map,
    const std::vector<segment>& segments):
    _instance(instance), _edge_clusters(edge_clusters),
    _segment_map(segment_map), _segments(segments) {}

std::vector<int> ClusterGuidedHierholzer::build() const {
    std::vector<int> order = map_edges_to_segments(
        build_reversed_edge_order(build_outgoing()));
    validate_order(order);
    return order;
}

ClusterGuidedHierholzer::OutgoingByNode
ClusterGuidedHierholzer::build_outgoing() const {
    OutgoingByNode outgoing;
    int edge_index = 0;
    for (const PathEdge& edge : _instance.edges) {
        for (int pass = 0; pass < edge.times(); ++pass) {
            const segment r = {edge_index, pass};
            outgoing[edge.from].all.insert(r);
            outgoing[edge.from].by_cluster[_edge_clusters[edge_index]].insert(r);
        }
        ++edge_index;
    }
    return outgoing;
}

std::vector<int> ClusterGuidedHierholzer::build_reversed_edge_order(
    OutgoingByNode outgoing) const {
    std::vector<int> node_stack{_instance.deposit};
    std::vector<int> edge_stack;
    std::vector<int> reversed_edges;
    reversed_edges.reserve(_segments.size());

    while (!node_stack.empty()) {
        const int node = node_stack.back();
        auto outgoing_it = outgoing.find(node);

        if (outgoing_it == outgoing.end() || outgoing_it->second.all.empty()) {
            node_stack.pop_back();
            if (!edge_stack.empty()) {
                reversed_edges.push_back(edge_stack.back());
                edge_stack.pop_back();
            }
            continue;
        }

        const int previous_edge = edge_stack.empty() ? -1 : edge_stack.back();
        const segment passage =
            take_next_passage(outgoing_it->second, previous_edge);

        edge_stack.push_back(passage.first);
        node_stack.push_back(_instance.edges[passage.first].to);
    }

    if (reversed_edges.size() != _segments.size())
        throw std::invalid_argument(
            "La solucion RCPP no forma un circuito euleriano completo");
    return reversed_edges;
}

segment ClusterGuidedHierholzer::take_next_passage(
    Outgoing& choices, int previous_edge) const {
    auto selected = choices.all.begin();
    if (previous_edge != -1) {
        const int previous_cluster = _edge_clusters[previous_edge];
        auto preferred = choices.by_cluster.find(previous_cluster);
        if (preferred != choices.by_cluster.end() &&
            !preferred->second.empty())
            selected = choices.all.find(*preferred->second.begin());
    }

    const segment passage = *selected;
    const int cluster = _edge_clusters[passage.first];
    choices.all.erase(selected);
    auto cluster_choices = choices.by_cluster.find(cluster);
    cluster_choices->second.erase(passage);
    if (cluster_choices->second.empty())
        choices.by_cluster.erase(cluster_choices);
    return passage;
}

std::vector<int> ClusterGuidedHierholzer::map_edges_to_segments(
    const std::vector<int>& reversed_edges) const {
    std::vector<int> next_occurrence(_instance.edges.size(), 0);
    std::vector<int> order;
    order.reserve(_segments.size());
    for (int i = reversed_edges.size() - 1; i >= 0; --i) {
        const int e = reversed_edges[i];
        const int pass = next_occurrence[e]++;
        const auto segment_it = _segment_map.find({e, pass});
        if (segment_it == _segment_map.end())
            throw std::invalid_argument(
                "La solucion RCPP repite una pasada fuera de su multiplicidad");
        order.push_back(segment_it->second);
    }
    return order;
}

void ClusterGuidedHierholzer::validate_order(
    const std::vector<int>& order) const {
    if (order.empty())
        throw std::invalid_argument(
            "La solucion RCPP no forma un circuito euleriano completo");
    const PathEdge& first = _instance.edges[_segments[order.front()].first];
    const PathEdge& last = _instance.edges[_segments[order.back()].first];
    if (first.original_edge_id != -2 || first.from != _instance.deposit)
        throw std::invalid_argument(
            "El circuito euleriano no comienza con la salida del deposito");
    if (last.original_edge_id != -2 || last.to != _instance.deposit)
        throw std::invalid_argument(
            "El circuito euleriano no termina con el regreso al deposito");

    std::vector<bool> used(_segments.size(), false);
    const int K = _segments.size();
    for (int position = 0; position < K; ++position) {
        const int segment_index = order[position];
        if (segment_index < 0 || segment_index >= K || used[segment_index])
            throw std::invalid_argument(
                "El circuito euleriano contiene una pasada duplicada");
        used[segment_index] = true;
        if (position + 1 < K) {
            const PathEdge& current =
                _instance.edges[_segments[segment_index].first];
            const PathEdge& next =
                _instance.edges[_segments[order[position + 1]].first];
            if (current.to != next.from)
                throw std::invalid_argument(
                    "La solucion RCPP no forma un circuito euleriano continuo");
        }
    }
}
