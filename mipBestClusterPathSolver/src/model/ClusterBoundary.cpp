#include <model/ClusterBoundary.h>
#include <model/ClusterSegmentProcessor.h>
#include <model/SolutionEulerCircuit.h>
#include <stdexcept>
#include <utility>

ClusterBoundaryExtractor::ClusterBoundaryExtractor(const SuperGraph& super_graph, const NeutralExpandedGraph& expanded_graph, const Solution& solution,
        const vector<ProvisionalConnection>& connections)
        : super_graph(super_graph), _expanded_graph(expanded_graph), solution(solution), connections(connections) {

    const auto add_node_cluster = [&](int node, int cluster) {
        const auto [found, inserted] = cluster_by_node.emplace(node, cluster);
        if (!inserted && found->second != cluster)
            throw invalid_argument("Un nodo expandido pertenece a mas de un cluster");
    };

    // Construye la pertenencia nodo-cluster usando solo registros fisicos. Los
    // conectores virtuales y los neutral_in_out sinteticos no definen pertenencia.
    for (const SuperArc& arc : super_graph.arcs()) {
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

map<int, ClusterBoundary> ClusterBoundaryExtractor::get_boundaries() {
    // El circuito ordenado aporta la informacion que perdia la frontera
    // agregada: indica que salida corresponde a cada entrada.
    vector<ClusterSolutionArc> extra;
    for (size_t i = 0; i < connections.size(); ++i) {
        const auto& link = connections[i];
        extra.emplace_back(i, link.from, link.to, SolutionArcKind::Provisional, link.count);
    }
    const vector<ClusterSolutionArc> circuit =
        SolutionEulerCircuit(super_graph.nodes_amount(), solution, extra).circuit();
    ClusterSegmentProcessor processor(super_graph, _expanded_graph, cluster_by_node);

    map<int, vector<ClusterVisit>> visits_by_cluster = processor.process(circuit);

    map<int, ClusterBoundary> boundaries;
    for (auto& [cluster, visits] : visits_by_cluster)
        boundaries[cluster].visits = std::move(visits);
    return boundaries;
}
