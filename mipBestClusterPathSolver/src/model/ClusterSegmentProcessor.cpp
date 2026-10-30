#include <model/ClusterSegmentProcessor.h>
#include <stdexcept>
#include <utility>

ClusterSegmentProcessor::ClusterSegmentProcessor(
        const SuperGraph& super_graph,
        const NeutralExpandedGraph& expanded_graph,
        const map<int, NeutralInOut>& neutral_in_out_by_arc,
        const map<int, set<int>>& clusters_by_node)
        : _super_graph(super_graph),
          _expanded_graph(expanded_graph),
          neutral_in_out_by_arc(neutral_in_out_by_arc),
          clusters_by_node(clusters_by_node) {}

pair<int, int> ClusterSegmentProcessor::resolve_adj_deposit_node(int virtual_node) const {
    const int global_node = _super_graph.original_node(virtual_node);
    const auto found = clusters_by_node.find(global_node);
    if (found == clusters_by_node.end() || found->second.size() != 1)
        throw invalid_argument("Un conector del deposito no identifica un unico cluster");
    const int cluster = *found->second.begin();
    return {cluster, global_node};
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

void ClusterSegmentProcessor::process_internal_segment(const ClusterSolutionArc& segment) {
    if (!visiting_cluster) return;

    active_visit.internal_arc_ids.push_back(segment.id);

    const SuperArc& arc = *_super_graph.super_arc_with_id(segment.id);
    
    if (arc.edge_id >= 0 and _expanded_graph.edge_has_cluster(arc.edge_id, active_visit.cluster))
        active_visit.has_physical_arc = true;

    if (segment.kind == SolutionArcKind::Service)
        active_visit.service_edge_ids.push_back(arc.edge_id);
}

void ClusterSegmentProcessor::process_cluster_crossing(const ClusterSolutionArc& segment, const NeutralInOut& crossing) {
    const SuperArc& arc = *_super_graph.super_arc_with_id(segment.id);
    const ClusterPortal exit{ClusterPortalKind::NeutralInOut, arc.id, _super_graph.original_node(arc.from), 1, false};
    const ClusterPortal entry{ClusterPortalKind::NeutralInOut, arc.id, _super_graph.original_node(arc.to), 1, true};

    close_visit(crossing.source_cluster, exit);
    open_visit(crossing.target_cluster, entry);
}

void ClusterSegmentProcessor::process_segment(const ClusterSolutionArc& segment) {
    // Segmento conectado con el deposito original abre o cierra un subproblema
    if (segment.kind == SolutionArcKind::Deposit) {
        process_deposit_segment(segment);
        return;
    }

    // Segmentos in-out neutrales - cirran de un subproblema y apertura de otro 
    const auto crossing = neutral_in_out_by_arc.find(segment.id);
    if (crossing != neutral_in_out_by_arc.end()) {
        process_cluster_crossing(segment, crossing->second);
        return;
    }

    process_internal_segment(segment);
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
