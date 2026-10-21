#include <model/ClusterBoundary.h>
#include <set>
#include <stdexcept>
#include <utility>

ClusterBoundaryExtractor::ClusterBoundaryExtractor(
        const SuperGraph& super_graph,
        const BestClusterPathInstance& instance,
        const Solution& solution)
        : super_graph(super_graph), instance(instance), solution(solution) {
    // Las transiciones se identifican por sus extremos dirigidos. Este indice
    // permite relacionar cada arista del archivo in/out con su SuperArc.
    for (const SuperArc& arc : super_graph.arcs())
        arc_id_by_edge.emplace(EdgeKey(arc.from, arc.to), arc.id);

    // Una transicion entre clusters debe existir en el supergrafo y estar
    // representada por un registro sintetico (cluster 0).
    for (const ClusterTransition& transition : instance.transitions) {
        transition_by_edge.emplace(transition.edge, transition);

        const auto arc_id_it = arc_id_by_edge.find(transition.edge);

        if (arc_id_it == arc_id_by_edge.end())
            throw invalid_argument("Una transicion no pertenece al supergrafo");

        const SuperArc& arc = *super_graph.super_arc_with_id(arc_id_it->second);

        if (arc.edge_id < 0 || instance.record_clusters.at(arc.edge_id) != 0)
            throw invalid_argument("Una transicion no esta marcada como registro sintetico");

        transition_records.insert(arc.edge_id);
    }

    // La validacion inversa evita registros sinteticos sin una definicion de
    // origen y destino en la lista de transiciones.
    for (size_t record = 0; record < instance.record_clusters.size(); ++record)
        if (instance.record_clusters[record] == 0 and !transition_records.count(record))
            throw invalid_argument("Un registro sintetico no figura en las transiciones");

    // Construye la pertenencia nodo-cluster usando solo registros fisicos. Los
    // conectores virtuales y las transiciones sinteticas no definen pertenencia.
    
    for (const SuperArc& arc : super_graph.arcs()) {
        if (arc.edge_id < 0 or arc.edge_id >= instance.record_clusters.size())continue;
        const int cluster = instance.record_clusters[arc.edge_id];
        if (cluster <= 0) continue;
        clusters_by_node[super_graph.original_node(arc.from)].insert(cluster);
        clusters_by_node[super_graph.original_node(arc.to)].insert(cluster);
    }
}

void ClusterBoundaryExtractor::add_portal(int cluster, ClusterPortal portal) {
    // ClusterBoundary representa la frontera completa del cluster. Los flows
    // agrupan las multiplicidades por nodo, mientras que portals conserva cada
    // cruce concreto para poder reconstruirlo posteriormente.
    ClusterBoundary& boundary = boundaries[cluster];
    boundary.cluster = cluster;

    for (BoundaryFlow& flow : boundary.flows) {
        if (flow.global_node != portal.global_node) continue;

        if (portal.entry)
            flow.entries += portal.count;
        else
            flow.exits += portal.count;

        boundary.portals.push_back(portal);
        return;
    }

    boundary.flows.push_back(BoundaryFlow(portal));
    boundary.portals.push_back(portal);
}

map<int, ClusterBoundary> ClusterBoundaryExtractor::get_boundaries() {
    // Solo las transiciones usadas por la solucion forman parte de una
    // frontera. El cluster de origen recibe una salida en el extremo from y el
    // cluster de destino una entrada en el extremo to. original_node traduce
    // el nodo virtual del supergrafo al nodo correspondiente del grafo global.
    for (const ArcValue<long long>& segment : solution.traversals) {
        if (segment.value <= 0) continue;
        const auto found = transition_by_edge.find({segment.from, segment.to});
        if (found == transition_by_edge.end()) continue;

        const ClusterTransition& transition = found->second;
        const SuperArc& arc = *super_graph.super_arc_with_id(segment.id);

        add_portal(transition.source_cluster,
            {ClusterPortalKind::Transition, arc.id,
             super_graph.original_node(arc.from),
             segment.value, false});
        add_portal(transition.target_cluster,
            {ClusterPortalKind::Transition, arc.id,
             super_graph.original_node(arc.to),
             segment.value, true});
    }


    // El deposito global tambien actua como frontera. D -> nodo es una entrada
    // al cluster y nodo -> D es una salida. El extremo interno debe identificar
    // un unico cluster para que el portal no sea ambiguo.
    for (const ArcValue<long long>& value : solution.deposit_traversals) {
        if (value.value <= 0) continue;
        const SuperArc& arc = *super_graph.super_arc_with_id(value.id);
        const bool entry = value.from == -1;
        const int virtual_node = entry ? arc.to : arc.from;
        const int global_node = super_graph.original_node(virtual_node);
        const auto found = clusters_by_node.find(global_node);
        if (found == clusters_by_node.end() || found->second.size() != 1)
            throw invalid_argument(
                "Un conector del deposito no identifica un unico cluster");
        add_portal(*found->second.begin(),
            {ClusterPortalKind::GlobalDeposit, arc.id, global_node, value.value, entry});
    }

    // Cada subproblema de cluster necesita tantas entradas como salidas para
    // poder encadenar sus recorridos en la unica ruta global del vehiculo.
    validate_balances();

    // El extractor entrega la coleccion construida; no se reutiliza despues.
    return std::move(boundaries);
}

void ClusterBoundaryExtractor::validate_balances() const {
    for (const auto& [cluster, boundary] : boundaries) {
        (void) cluster;
        long long entries = 0;
        long long exits = 0;

        // Los portales que comparten nodo ya estan consolidados en flows.
        for (const BoundaryFlow& flow : boundary.flows) {
            entries += flow.entries;
            exits += flow.exits;
        }
        if (entries != exits)
            throw invalid_argument(
                "Las entradas y salidas del cluster no estan balanceadas");
    }
}

