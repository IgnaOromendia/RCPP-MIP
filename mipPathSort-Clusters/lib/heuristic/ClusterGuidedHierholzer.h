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
        const std::vector<segment>& segments);

    std::vector<int> build() const;

private:
    struct Outgoing {
        std::set<segment> all;
        std::map<int, std::set<segment>> by_cluster;
    };

    using OutgoingByNode = std::map<int, Outgoing>;

    OutgoingByNode build_outgoing() const;
    std::vector<int> build_reversed_edge_order(OutgoingByNode outgoing) const;
    segment take_next_passage(Outgoing& choices, int previous_edge) const;
    std::vector<int> map_edges_to_segments(
        const std::vector<int>& reversed_edges) const;
    void validate_order(const std::vector<int>& order) const;

    const PathSortInstance& _instance;
    const std::vector<int>& _edge_clusters;
    const std::map<segment, int>& _segment_map;
    const std::vector<segment>& _segments;
};

#endif
