#include <model/ClusterSubProblemBuilder.h>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

// Una arista fisica queda identificada por el registro original y por la
// orientacion de sus extremos. El registro por si solo no distingue las dos
// orientaciones generadas para una arista no dirigida.
using PhysicalArcKey = tuple<int, int, int>;

vector<ClusterProblem> ClusterSubProblemBuilder::get_subproblems() {
    // Los subproblemas se crean a partir de las fronteras efectivamente usadas
    // por la solucion global. Cada frontera corresponde a un cluster que debe
    // resolverse de forma independiente.
    service_assignments();

    // Estos indices permiten traducir luego los IDs creados por cada
    // SuperGraph local a los IDs equivalentes del SuperGraph global.
    map<PhysicalArcKey, int> global_physical_arcs;
    map<EdgeKey, int> global_arcs;
    for (const SuperArc& arc : _global_super_graph.arcs()) {
        global_arcs.emplace(EdgeKey(arc.from, arc.to), arc.id);
        if (arc.edge_id >= 0) {
            int from = _global_super_graph.original_node(arc.from);
            int to = _global_super_graph.original_node(arc.to);
            global_physical_arcs.emplace(PhysicalArcKey(arc.edge_id, from, to), arc.id);
        }
    }

    // Construimos todos los problemas
    for (const auto& [cluster, boundary] : _boundaries) {
        problem_index[cluster] = problems.size();
        problems.emplace_back(cluster, boundary, _expanded_graph);

        ClusterProblem& problem = problems.back();

        set<int> deposit_nodes;
        int local_vehicle = 0;

        for (const ClusterVisit& visit : boundary.visits) {
            if (visit.is_passthrough()) continue;
            ++local_vehicle;
            deposit_nodes.insert(visit.entry.global_node);
            deposit_nodes.insert(visit.exit.global_node);
            for (int edge_id : visit.service_edge_ids)
                if (!local_vehicle_by_global_edge[cluster].emplace(edge_id, local_vehicle).second)
                    throw invalid_argument("Un servicio global pertenece a mas de una visita");
        }

        problem.instance.deposit_nodes.assign(deposit_nodes.begin(), deposit_nodes.end());
    }

    // Asignamos las aristas y arcos a cada problema correspondiente
    assign_edges(_expanded_graph.expanded_edge_ids(), true);
    assign_edges(_expanded_graph.expanded_arc_ids(), false);

    // Construimos los grafos de cada subproblmea
    for (ClusterProblem& problem : problems) {
        const auto ids_it = global_edge_by_cluster.find(problem.cluster);
        const vector<int>& global_edge_ids = ids_it->second;

        problem.graph = make_unique<Graph>(problem.instance);
        problem.super_graph = make_unique<SuperGraph>(
            *problem.graph,
            problem.instance.turns,
            problem.instance.illegal_turns);

        // Mapear los arcos físicos locales.
        for (const SuperArc& arc : problem.super_graph->arcs()) {
            if (arc.edge_id < 0) continue;

            const int global_edge_id = global_edge_ids.at(arc.edge_id);
            const int from = problem.super_graph->original_node(arc.from);
            const int to = problem.super_graph->original_node(arc.to);

            const int global_arc_id = global_physical_arcs.at(PhysicalArcKey(global_edge_id, from, to));
            const SuperArc& global_arc = *_global_super_graph.super_arc_with_id(global_arc_id);

            problem.add_mapping(arc, global_arc);
        }

        // Mapear los giros internos.
        for (const SuperArc& arc : problem.super_graph->arcs()) {
            if (arc.edge_id != -1) continue;

            int from = problem.local_to_global_node(arc.from);
            int to = problem.local_to_global_node(arc.to);

            problem.mapping.local_to_global_arc[arc.id] = global_arcs.at(EdgeKey(from, to));
        }
    }

    return std::move(problems);
}

// Conserva los IDs globales cuyo servicio aparece en la solucion para validar
// que no falte ninguna arista requerida antes de construir los subproblemas.
void ClusterSubProblemBuilder::service_assignments() {
    for (const ArcValue<long long>& value : _global_solution.service) {
        if (value.value <= 0) continue;
        const SuperArc& arc = *_global_super_graph.super_arc_with_id(value.id);

        // En una arista no dirigida pueden existir SuperArcs que no
        // representan la orientacion elegida para prestar el servicio.
        if (arc.zone != -1 && arc.zone != value.vehicle) continue;
        serviced_edges.insert(arc.edge_id);
    }
}

void ClusterSubProblemBuilder::assign_edges(
    const vector<int>& global_edge_ids,
    bool bidirectional) {
    for (int global_edge_id : global_edge_ids) {
        const int cluster = _expanded_graph.edge_cluster(global_edge_id);

        // Los registros del cluster 0 son conectores sinteticos (in-out) entre clusters.
        // Forman parte del supergrafo global, pero no del recorrido interno de
        // ningun subproblema.
        if (cluster == 0) continue;

        const auto problem_it = problem_index.find(cluster);
        if (problem_it == problem_index.end()) {
            // Un cluster formado solo por registros opcionales puede no haber
            // sido visitado por la solucion global y, por lo tanto, no tener
            // portales ni un subproblema que optimizar.
            if (serviced_edges.count(global_edge_id) != 0)
                throw invalid_argument(
                    "El registro global servido " + to_string(global_edge_id) +
                    " pertenece al cluster " + to_string(cluster) +
                    ", pero ese cluster no tiene una frontera activa");
            continue;
        }
        const size_t problem_idx = problem_it->second;

        ClusterProblem& problem = problems.at(problem_idx);
        const Edge& edge = *_expanded_graph.edge_with_id(global_edge_id);
        InstanceEdge problem_edge{
            edge.from,
            edge.to,
            edge.zone,
            edge.cost,
            edge.demand,
        };

        if (edge.zone != 0) {
            problem_edge.zone = 0;
            if (serviced_edges.count(global_edge_id) != 0) {
                const auto assigned = local_vehicle_by_global_edge[cluster].find(global_edge_id);
                if (assigned == local_vehicle_by_global_edge[cluster].end())
                    throw invalid_argument("Un servicio global no pertenece a ninguna visita");
                problem_edge.zone = assigned->second;
            }
        }

        if (bidirectional)
            problem.instance.edges.push_back(problem_edge);
        else 
            problem.instance.arcs.push_back(problem_edge);

        global_edge_by_cluster[cluster].push_back(global_edge_id);
    }
}
