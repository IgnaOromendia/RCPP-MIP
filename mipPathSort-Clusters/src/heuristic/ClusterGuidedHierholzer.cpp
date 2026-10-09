#include <heuristic/ClusterGuidedHierholzer.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

ClusterGuidedHierholzer::ClusterGuidedHierholzer(
    const PathSortInstance& instance,
    const std::vector<int>& edge_clusters,
    const std::map<segment, int>& segment_map,
    const std::vector<segment>& segments,
    int lookahead_depth,
    int branch_width):
    _instance(instance), _edge_clusters(edge_clusters),
    _segment_map(segment_map), _segments(segments),
    _lookahead_depth(lookahead_depth), _branch_width(branch_width) {
    if (_lookahead_depth <= 0)
        throw std::invalid_argument(
            "La profundidad de lookahead debe ser positiva");
    if (_branch_width <= 0)
        throw std::invalid_argument(
            "El ancho de lookahead debe ser positivo");

    int cluster_count = 0;
    for (int cluster : _edge_clusters)
        cluster_count = std::max(cluster_count, cluster + 1);
    _cluster_sizes.assign(cluster_count, 0);
    for (int cluster : _edge_clusters)
        ++_cluster_sizes[cluster];
}

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
    std::vector<bool> visited_clusters(_cluster_sizes.size(), false);
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

        const segment passage = take_next_passage(
            outgoing, node, edge_stack, visited_clusters);

        edge_stack.push_back(passage.first);
        visited_clusters[_edge_clusters[passage.first]] = true;
        node_stack.push_back(_instance.edges[passage.first].to);
    }

    if (reversed_edges.size() != _segments.size())
        throw std::invalid_argument(
            "La solucion RCPP no forma un circuito euleriano completo");
    return reversed_edges;
}

segment ClusterGuidedHierholzer::take_next_passage(
    OutgoingByNode& outgoing,
    int node,
    const std::vector<int>& edge_stack,
    const std::vector<bool>& visited_clusters) const {
    const int previous_cluster = edge_stack.empty()
        ? -1
        : _edge_clusters[edge_stack.back()];
    std::vector<bool> seen_clusters = visited_clusters;

    const std::set<segment>& available = outgoing.at(node).all;
    segment selected = *available.begin();
    LookaheadScore selected_score{
        std::numeric_limits<long long>::max(),
        std::numeric_limits<int>::max(),
        0};

    std::vector<segment> candidates(available.begin(), available.end());
    std::stable_sort(
        candidates.begin(), candidates.end(),
        [&](const segment& left, const segment& right) {
            const int left_cluster = _edge_clusters[left.first];
            const int right_cluster = _edge_clusters[right.first];
            const int left_switch = left_cluster != previous_cluster;
            const int right_switch = right_cluster != previous_cluster;
            if (left_switch != right_switch) return left_switch < right_switch;
            return left < right;
        });
    if (candidates.size() > static_cast<std::size_t>(_branch_width))
        candidates.resize(_branch_width);

    for (const segment& candidate : candidates) {
        const LookaheadScore score = score_passage(
            outgoing, candidate, previous_cluster, _lookahead_depth,
            seen_clusters);
        if (better_score(score, selected_score) ||
            (!better_score(selected_score, score) && candidate < selected)) {
            selected = candidate;
            selected_score = score;
        }
    }

    erase_passage(outgoing, selected);
    return selected;
}

ClusterGuidedHierholzer::LookaheadScore
ClusterGuidedHierholzer::best_lookahead_score(
    OutgoingByNode& outgoing,
    int node,
    int previous_cluster,
    int remaining_depth,
    std::vector<bool>& seen_clusters) const {
    if (remaining_depth <= 0) return {};
    auto choices = outgoing.find(node);
    if (choices == outgoing.end() || choices->second.all.empty()) return {};

    std::vector<segment> candidates(
        choices->second.all.begin(), choices->second.all.end());
    std::stable_sort(
        candidates.begin(), candidates.end(),
        [&](const segment& left, const segment& right) {
            const int left_cluster = _edge_clusters[left.first];
            const int right_cluster = _edge_clusters[right.first];
            const int left_switch = left_cluster != previous_cluster;
            const int right_switch = right_cluster != previous_cluster;
            if (left_switch != right_switch) return left_switch < right_switch;
            return left < right;
        });
    if (candidates.size() > static_cast<std::size_t>(_branch_width))
        candidates.resize(_branch_width);

    LookaheadScore best{
        std::numeric_limits<long long>::max(),
        std::numeric_limits<int>::max(),
        0};
    for (const segment& candidate : candidates) {
        const LookaheadScore score = score_passage(
            outgoing, candidate, previous_cluster, remaining_depth,
            seen_clusters);
        if (better_score(score, best)) best = score;
    }
    return best;
}

ClusterGuidedHierholzer::LookaheadScore
ClusterGuidedHierholzer::score_passage(
    OutgoingByNode& outgoing,
    const segment& passage,
    int previous_cluster,
    int remaining_depth,
    std::vector<bool>& seen_clusters) const {
    const int cluster = _edge_clusters[passage.first];
    const bool switched = previous_cluster != -1 && cluster != previous_cluster;
    const bool reopened = switched && seen_clusters[cluster];
    const bool first_visit = !seen_clusters[cluster];

    LookaheadScore score;
    score.reopened_cluster_cost = reopened ? _cluster_sizes[cluster] : 0;
    score.cluster_switches = switched ? 1 : 0;
    score.steps = 1;

    erase_passage(outgoing, passage);
    if (first_visit) seen_clusters[cluster] = true;
    const LookaheadScore suffix = best_lookahead_score(
        outgoing, _instance.edges[passage.first].to, cluster,
        remaining_depth - 1, seen_clusters);
    if (first_visit) seen_clusters[cluster] = false;
    restore_passage(outgoing, passage);

    score.reopened_cluster_cost += suffix.reopened_cluster_cost;
    score.cluster_switches += suffix.cluster_switches;
    score.steps += suffix.steps;
    return score;
}

void ClusterGuidedHierholzer::erase_passage(
    OutgoingByNode& outgoing, const segment& passage) const {
    Outgoing& choices = outgoing.at(_instance.edges[passage.first].from);
    choices.all.erase(passage);
    const int cluster = _edge_clusters[passage.first];
    auto cluster_choices = choices.by_cluster.find(cluster);
    cluster_choices->second.erase(passage);
    if (cluster_choices->second.empty())
        choices.by_cluster.erase(cluster_choices);
}

void ClusterGuidedHierholzer::restore_passage(
    OutgoingByNode& outgoing, const segment& passage) const {
    Outgoing& choices = outgoing[_instance.edges[passage.first].from];
    choices.all.insert(passage);
    choices.by_cluster[_edge_clusters[passage.first]].insert(passage);
}

bool ClusterGuidedHierholzer::better_score(
    const LookaheadScore& left, const LookaheadScore& right) {
    if (left.reopened_cluster_cost != right.reopened_cluster_cost)
        return left.reopened_cluster_cost < right.reopened_cluster_cost;
    if (left.cluster_switches != right.cluster_switches)
        return left.cluster_switches < right.cluster_switches;
    return left.steps > right.steps;
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
