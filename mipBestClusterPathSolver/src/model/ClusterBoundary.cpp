#include <model/ClusterBoundary.h>
#include <model/ClusterSegmentProcessor.h>
#include <model/SolutionEulerCircuit.h>
#include <stdexcept>
#include <utility>

ClusterBoundaryExtractor::ClusterBoundaryExtractor(const SuperGraph& super_graph, const NeutralExpandedGraph& expanded_graph, const Solution& solution)
        : super_graph(super_graph), _expanded_graph(expanded_graph), solution(solution) {        

    const auto add_node_cluster = [&](int node, int cluster) {
        const auto [found, inserted] = cluster_by_node.emplace(node, cluster);
        if (!inserted && found->second != cluster)
            throw invalid_argument("Un nodo expandido pertenece a mas de un cluster");
    };

    // Construye la pertenencia nodo-cluster usando solo registros fisicos. Los
    // conectores virtuales y los neutral_in_out sinteticos no definen pertenencia.
    for (const SuperArc& arc : super_graph.arcs()) {
        global_arc_by_edge.emplace(EdgeKey(arc.from, arc.to), arc.id);

        if (arc.edge_id < 0 or 
            arc.edge_id >= expanded_graph.edges_amount() or
            expanded_graph.is_neutral_edge(arc.edge_id))
            continue;

        const int cluster = expanded_graph.edge_cluster(arc.edge_id);
        add_node_cluster(super_graph.original_node(arc.from), cluster);
        add_node_cluster(super_graph.original_node(arc.to), cluster);
    }

    for (int node = 0; node < expanded_graph.nodes_amount(); ++node)
        if (cluster_by_node.count(node) == 0)
            throw invalid_argument("Un nodo expandido no pertenece a ningun cluster");
}

const map<EdgeKey, int>& ClusterBoundaryExtractor::get_global_arc_by_edge() const {
    return global_arc_by_edge;
}

void ClusterBoundaryExtractor::add_portal(int cluster, const ClusterPortal& portal) {
    // ClusterBoundary representa la frontera completa del cluster. Los gates
    // agrupan las multiplicidades por nodo, mientras que portals conserva cada
    // cruce concreto para poder reconstruirlo posteriormente.
    ClusterBoundary& boundary = boundaries[cluster];
    boundary.cluster = cluster;

    for (BoundaryGate& flow : boundary.gates) {
        if (flow.global_node != portal.global_node) continue;

        if (portal.entry)
            flow.entries += portal.count;
        else
            flow.exits += portal.count;

        boundary.portals.push_back(portal);
        return;
    }

    boundary.gates.push_back(BoundaryGate(portal));
    boundary.portals.push_back(portal);
}

// Conserva la pareja entrada-salida de esta recorrida por el cluster.
void ClusterBoundaryExtractor::add_visit(int cluster, ClusterVisit visit) {
    ClusterBoundary& boundary = boundaries[cluster];
    boundary.cluster = cluster;

    // Ademas registra ambos extremos en la frontera agregada. add_portal
    // conserva cada cruce en portals y acumula sus multiplicidades por nodo
    // en gates, que luego se usan para validar el balance del cluster.
    add_portal(cluster, visit.entry);
    add_portal(cluster, visit.exit);
    boundary.visits.push_back(std::move(visit));
}

map<int, ClusterBoundary> ClusterBoundaryExtractor::get_boundaries() {
    // El circuito ordenado aporta la informacion que perdia la frontera
    // agregada: indica que salida corresponde a cada entrada.
    const vector<ClusterSolutionArc> circuit =
        SolutionEulerCircuit(super_graph.nodes_amount(), solution).circuit();
    ClusterSegmentProcessor processor(super_graph, _expanded_graph, cluster_by_node);

    map<int, vector<ClusterVisit>> visits_by_cluster = processor.process(circuit);

    for (auto& [cluster, visits] : visits_by_cluster)
        for (ClusterVisit& visit : visits)
            add_visit(cluster, std::move(visit));

    validate_balances();

    // El extractor entrega la coleccion construida; no se reutiliza despues.
    return std::move(boundaries);
}

void ClusterBoundaryExtractor::validate_balances() const {
    for (const auto& [cluster, boundary] : boundaries) {
        (void) cluster;
        long long entries = 0;
        long long exits = 0;

        // Los portales que comparten nodo ya estan consolidados en gates.
        for (const BoundaryGate& flow : boundary.gates) {
            entries += flow.entries;
            exits += flow.exits;
        }

        if (entries != exits)
            throw invalid_argument("Las entradas y salidas del cluster no estan balanceadas");

        if (entries != static_cast<long long>(boundary.visits.size()))
            throw invalid_argument("Las visitas del cluster no coinciden con su frontera");
    }
}

