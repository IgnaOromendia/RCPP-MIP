#include <model/ClusterSegmentProcessor.h>
#include <stdexcept>
#include <utility>

ClusterSegmentProcessor::ClusterSegmentProcessor(
        const SuperGraph& super_graph,
        const NeutralExpandedGraph& expanded_graph,
        const map<int, int>& cluster_by_node)
        : _super_graph(super_graph),
          _expanded_graph(expanded_graph),
          cluster_by_node(cluster_by_node) {}

pair<int, int> ClusterSegmentProcessor::resolve_adj_deposit_node(int virtual_node) const {
    const int global_node = _super_graph.original_node(virtual_node);
    return {cluster_by_node.at(global_node), global_node};
}

void ClusterSegmentProcessor::open_visit(int cluster, const ClusterPortal& entry) {
    if (visiting_cluster) throw invalid_argument("El circuito abre dos visitas de cluster simultaneas");
    active_visit = ActiveClusterVisit{cluster, entry, {}, {}, false};
    visiting_cluster = true;
}

void ClusterSegmentProcessor::close_visit(int expected_cluster, const ClusterPortal& exit) {
    if (!visiting_cluster or active_visit.cluster != expected_cluster)
        throw invalid_argument("El circuito sale de un cluster distinto al visitado");
    visits_by_cluster[expected_cluster].push_back(active_visit.finish(next_visit_id++, exit));
    visiting_cluster = false;
}

void ClusterSegmentProcessor::process_deposit_segment(const ClusterSolutionArc& segment) {
    const bool entry = segment.from == -1;
    const int virtual_node = entry ? segment.to : segment.from;
    const auto [cluster, global_node] = resolve_adj_deposit_node(virtual_node);
    const ClusterPortal portal {ClusterPortalKind::GlobalDeposit, segment.id, global_node, 1, entry};

    if (entry)
        open_visit(cluster, portal);
    else
        close_visit(cluster, portal);
}

void ClusterSegmentProcessor::process_internal_segment(const ClusterSolutionArc& segment, const SuperArc& arc) {
    if (!visiting_cluster) return;

    active_visit.internal_arc_ids.push_back(segment.id);

    if (arc.edge_id >= 0 and _expanded_graph.edge_has_cluster(arc.edge_id, active_visit.cluster))
        active_visit.has_physical_arc = true;

    if (segment.kind == SolutionArcKind::Service)
        active_visit.service_edge_ids.push_back(arc.edge_id);
}

void ClusterSegmentProcessor::process_cluster_crossing(const SuperArc& arc) {
    const int source_node = _super_graph.original_node(arc.from);
    const int target_node = _super_graph.original_node(arc.to);
    const int source_cluster = cluster_by_node.at(source_node);
    const int target_cluster = cluster_by_node.at(target_node);

    if (source_cluster == target_cluster)
        throw invalid_argument("Un neutral_in_out no conecta clusters distintos");

    const ClusterPortal exit{
        ClusterPortalKind::NeutralInOut, arc.id, source_node, 1, false};
    const ClusterPortal entry{
        ClusterPortalKind::NeutralInOut, arc.id, target_node, 1, true};

    close_visit(source_cluster, exit);
    open_visit(target_cluster, entry);
}

void ClusterSegmentProcessor::process_segment(const ClusterSolutionArc& segment) {
    // Segmento conectado con el deposito original abre o cierra un subproblema
    if (segment.kind == SolutionArcKind::Deposit) {
        process_deposit_segment(segment);
        return;
    }

    const SuperArc& arc = *_super_graph.super_arc_with_id(segment.id);

    // Un neutral_in_out cierra la visita actual y abre la del cluster vecino.
    if (arc.is_neutral) {
        process_cluster_crossing(arc);
        return;
    }

    process_internal_segment(segment, arc);
}

map<int, vector<ClusterVisit>> ClusterSegmentProcessor::process(const vector<ClusterSolutionArc>& circuit) {
    visiting_cluster = false;
    next_visit_id = 1;
    visits_by_cluster.clear();

    for (const ClusterSolutionArc& segment : circuit)
        process_segment(segment);

    if (visiting_cluster)
        throw invalid_argument("El circuito deja una visita de cluster abierta");

    return std::move(visits_by_cluster);
}
