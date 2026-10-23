#include <model/ClusterBoundary.h>
#include <model/ClusterSegmentProcessor.h>
#include <model/SolutionEulerCircuit.h>
#include <set>
#include <stdexcept>
#include <utility>

ClusterBoundaryExtractor::ClusterBoundaryExtractor(const SuperGraph& super_graph, const BestClusterPathInstance& instance, const Solution& solution)
        : super_graph(super_graph), instance(instance), solution(solution) {
    // Los neutral_in_out se identifican por sus extremos dirigidos. Este indice
    // permite relacionar cada arista del archivo in/out con su SuperArc.
    for (const SuperArc& arc : super_graph.arcs())
        global_arc_by_edge.emplace(EdgeKey(arc.from, arc.to), arc.id);

    // Un neutral_in_out entre clusters debe existir en el supergrafo y estar
    // representada por un registro sintetico (cluster 0).
    for (const NeutralInOut& neutral_in_out : instance.neutral_in_outs) {
        const auto arc_id_it = global_arc_by_edge.find(neutral_in_out.edge);

        if (arc_id_it == global_arc_by_edge.end())
            throw invalid_argument("Un neutral_in_out no pertenece al supergrafo");

        const SuperArc& arc = *super_graph.super_arc_with_id(arc_id_it->second);

        if (arc.edge_id < 0 || instance.arc_clusters.at(arc.edge_id) != 0)
            throw invalid_argument("Un neutral_in_out no esta marcado como registro sintetico");

        neutral_in_out_records.insert(arc.edge_id);
        neutral_in_out_ids.insert(arc.id);
        neutral_in_out_by_arc.emplace(arc.id, neutral_in_out);
    }

    // La validacion inversa evita registros sinteticos sin una definicion de
    // origen y destino en la lista de neutral_in_out.
    for (size_t arc = 0; arc < instance.arc_clusters.size(); ++arc)
        if (instance.arc_clusters[arc] == 0 and !neutral_in_out_records.count(arc))
            throw invalid_argument("Un registro sintetico no figura en los neutral_in_out");

    // Construye la pertenencia nodo-cluster usando solo registros fisicos. Los
    // conectores virtuales y los neutral_in_out sinteticos no definen pertenencia.
    for (const SuperArc& arc : super_graph.arcs()) {
        if (arc.edge_id < 0 or arc.edge_id >= instance.arc_clusters.size())continue;
        const int cluster = instance.arc_clusters[arc.edge_id];
        if (cluster <= 0) continue;
        clusters_by_node[super_graph.original_node(arc.from)].insert(cluster);
        clusters_by_node[super_graph.original_node(arc.to)].insert(cluster);
    }
}

const map<EdgeKey, int>& ClusterBoundaryExtractor::get_global_arc_by_edge() const {
    return global_arc_by_edge;
}

const set<int>& ClusterBoundaryExtractor::get_neutral_in_out_ids() const {
    return neutral_in_out_ids;
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
    const vector<OrderedSolutionArc> circuit = SolutionEulerCircuit::build(solution);
    ClusterSegmentProcessor processor(super_graph, instance, neutral_in_out_by_arc, clusters_by_node);

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

