#ifndef CLUSTER_GUIDED_HIERHOLZER_H
#define CLUSTER_GUIDED_HIERHOLZER_H

#include <model/PathSortInstance.h>
#include <map>
#include <set>
#include <vector>

class ClusterGuidedHierholzer {
public:
    ClusterGuidedHierholzer(
        const PathSortInstance& instance,
        const std::vector<int>& edge_clusters,
        const std::map<segment, int>& segment_map,
        const std::vector<segment>& segments,
        int lookahead_depth = 1,
        int branch_width = 8);

    std::vector<int> build() const;

private:
    struct Outgoing {
        std::set<segment> all;
        std::map<int, std::set<segment>> by_cluster;
    };

    using OutgoingByNode = std::map<int, Outgoing>;

    struct LookaheadScore {
        long long reopened_cluster_cost = 0;
        int cluster_switches = 0;
        int steps = 0;
    };

    OutgoingByNode build_outgoing() const;
    std::vector<int> build_reversed_edge_order(OutgoingByNode outgoing) const;
    segment take_next_passage(
        OutgoingByNode& outgoing,
        int node,
        const std::vector<int>& edge_stack,
        const std::vector<bool>& visited_clusters) const;
    LookaheadScore best_lookahead_score(
        OutgoingByNode& outgoing,
        int node,
        int previous_cluster,
        int remaining_depth,
        std::vector<bool>& seen_clusters) const;
    LookaheadScore score_passage(
        OutgoingByNode& outgoing,
        const segment& passage,
        int previous_cluster,
        int remaining_depth,
        std::vector<bool>& seen_clusters) const;
    void erase_passage(OutgoingByNode& outgoing, const segment& passage) const;
    void restore_passage(OutgoingByNode& outgoing, const segment& passage) const;
    static bool better_score(
        const LookaheadScore& left,
        const LookaheadScore& right);
    std::vector<int> map_edges_to_segments(
        const std::vector<int>& reversed_edges) const;
    void validate_order(const std::vector<int>& order) const;

    const PathSortInstance& _instance;
    const std::vector<int>& _edge_clusters;
    const std::map<segment, int>& _segment_map;
    const std::vector<segment>& _segments;
    int _lookahead_depth;
    int _branch_width;
    std::vector<int> _cluster_sizes;
};

#endif
