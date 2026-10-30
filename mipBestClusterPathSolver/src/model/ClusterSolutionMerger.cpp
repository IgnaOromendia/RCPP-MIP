#include <model/ClusterSolutionMerger.h>
#include <algorithm>
#include <stdexcept>

namespace {
// Recupera el ID del SuperArc global dirigido que une dos nodos virtuales. Los
// conectores necesarios para recomponer la ruta deben existir en el supergrafo;
// su ausencia indica una inconsistencia entre el grafo global y el subproblema.
int find_arc(const map<EdgeKey, int>& arcs, int from, int to) {
    const auto found = arcs.find({from, to});
    if (found == arcs.end())
        throw invalid_argument("No existe el conector global requerido");
    return found->second;
}
}

Solution ClusterSolutionMerger::build_solution() {
    add_values_from_local_solution();
    add_values_from_boundary_connectors();
    add_values_from_neutral_in_out();

    Solution merged = merge();

    validate_flow();
    validate_service();
    validate_connected();

    return merged;
}

Solution ClusterSolutionMerger::merge() const {
    // Materializa los mapas acumulados en el formato vectorial de Solution.
    // Cada subproblema tiene un unico vehiculo, por eso el vehiculo exportado es 1.
    Solution merged;

    for (const auto& [arc_id, value] : service_value_map) {
        const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
        merged.service.emplace_back(arc, value);
        merged.objective += arc.cost * value;
    }

    for (const auto& [arc_id, value] : traversals_value_map) {
        const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
        merged.traversals.emplace_back(arc, value);

        // Solo los recorridos sobre registros fisicos de clusters positivos
        // aportan costo. Se excluyen giros, conectores de deposito y
        // neutral_in_out sinteticas.
        if (arc.edge_id >= 0 and instance.edge_cluster(arc.edge_id) > 0)
            merged.objective += arc.cost * value;
    }

    // Solution representa el extremo deposito con -1 aunque el SuperArc global
    // utilice internamente el ID del nodo virtual del deposito.
    for (const auto& [arc_id, value] : deposit_value_map) {
        const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
        merged.deposit_traversals.push_back({
            arc.id,
            arc.from == global_super_graph.deposit() ? -1 : arc.from,
            arc.to == global_super_graph.deposit() ? -1 : arc.to,
            1,
            value,
        });
    }
    return merged;
}

void ClusterSolutionMerger::validate_flow() const {
    // Verifica continuidad en todos los nodos distintos del deposito. Se usa la
    // convencion salida positiva / entrada negativa; al final todo nodo interno
    // debe tener balance cero contando servicio, recorridos y frontera.
    map<int, long long> balance;
    auto apply = [&](const map<int, long long>& values) {
        for (const auto& [arc_id, value] : values) {
            const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
            balance[arc.from] += value;
            balance[arc.to] -= value;
        }
    };
    apply(service_value_map);
    apply(traversals_value_map);

    // El deposito puede tener balance global no nulo en esta comprobacion, por
    // lo que solo se carga el extremo interno de cada deposit_traversal.
    for (const auto& [arc_id, value] : deposit_value_map) {
        const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
        if (arc.from != global_super_graph.deposit())
            balance[arc.from] += value;
        if (arc.to != global_super_graph.deposit())
            balance[arc.to] -= value;
    }

    // Cualquier residuo señala que falta o sobra una pieza al unir clusters,
    // neutral_in_out, giros y deposito.
    for (const auto& [node, value] : balance) {
        (void) node;
        if (value != 0)
            throw invalid_argument(
                "La solucion combinada no conserva continuidad");
    }
}

void ClusterSolutionMerger::validate_service() const {
    // Acumula el servicio por registro de la instancia original. Las dos
    // orientaciones de una arista no dirigida comparten edge_id y por eso se
    // validan conjuntamente.
    map<int, long long> service_by_record;
    for (const auto& [arc_id, value] : service_value_map) {
        const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
        service_by_record[arc.edge_id] += value;
    }

    // Comprueba que cada registro requerido se haya servido una vez. Graph
    // conserva los IDs globales aunque haya conectores de deposito intercalados.
    for (const Edge* edge : instance.requested())
        if (service_by_record[edge->id] != 1)
            throw invalid_argument(
                "La solucion combinada no conserva todos los servicios");
}

void ClusterSolutionMerger::validate_connected() const {
    map<int, set<int>> adjacency;
    set<int> used_nodes;
    auto add = [&](const map<int, long long>& values) {
        for (const auto& [arc_id, value] : values) {
            if (value <= 0) continue;
            const SuperArc& arc = *global_super_graph.super_arc_with_id(arc_id);
            adjacency[arc.from].insert(arc.to);
            adjacency[arc.to].insert(arc.from);
            used_nodes.insert(arc.from);
            used_nodes.insert(arc.to);
        }
    };
    add(service_value_map);
    add(traversals_value_map);
    add(deposit_value_map);

    set<int> reached = {global_super_graph.deposit()};
    vector<int> pending = {global_super_graph.deposit()};
    for (size_t index = 0; index < pending.size(); ++index)
        for (int neighbour : adjacency[pending[index]])
            if (reached.insert(neighbour).second)
                pending.push_back(neighbour);
    for (int node : used_nodes)
        if (!reached.count(node))
            throw invalid_argument(
                "La solucion combinada no esta conectada al deposito");
}

void ClusterSolutionMerger::add_solution(Solution sol) {
    cluster_solutions.push_back(sol);
}

// Incorpora el servicio y los recorridos internos de cada cluster.
void ClusterSolutionMerger::add_values_from_local_solution() {
    if (cluster_solutions.size() != problems.size())
        throw invalid_argument("Faltan soluciones locales de clusters");

    for (size_t i = 0; i < problems.size(); ++i) {
        const ClusterProblem& problem = problems[i];
        const Solution& solution = cluster_solutions[i];

        // Convierte los IDs locales en globales y acumula las multiplicidades
        // de X (servicio) e Y (recorrido sin servicio).
        for (const ArcValue<long long>& value : solution.service) {
            if (value.value <= 0) continue;
            service_value_map[problem.mapping.local_to_global_arc.at(value.id)] += value.value;
        }

        for (const ArcValue<long long>& value : solution.traversals) {
            if (value.value <= 0) continue;
            traversals_value_map[problem.mapping.local_to_global_arc.at(value.id)] += value.value;
        }

        // Registramos qué nodo virtual eligió el solver para reconstruir después el
        // conector real hacia un neutral_in_out o hacia el deposito global.
        for (const ArcValue<long long>& value : solution.deposit_traversals) {
            if (value.value <= 0) continue;
            if (value.value != 1 || value.vehicle < 1 ||
                value.vehicle > problem.instance.vehicles)
                throw invalid_argument("La solucion local contiene una frontera de visita invalida");

            const SuperArc& local_arc = *problem.super_graph->super_arc_with_id(value.id);
            const bool entry = value.from == -1;
            const int local_node = entry ? local_arc.to : local_arc.from;
            const int global_virtual = problem.mapping.local_to_global_node.at(local_node);
            int local_vehicle = 0;
            const ClusterVisit* selected_visit = nullptr;
            for (const ClusterVisit& visit : problem.boundary.visits) {
                if (visit.is_passthrough()) continue;
                if (++local_vehicle == value.vehicle) {
                    selected_visit = &visit;
                    break;
                }
            }
            if (!selected_visit)
                throw invalid_argument(
                    "La solucion local referencia una visita inexistente");
            const int visit_id = selected_visit->id;
            if (!choices[visit_id].emplace(entry,
                    BoundaryGateChoice{global_virtual}).second)
                throw invalid_argument(
                    "La solucion local duplica un extremo de visita");
        }
    }
}

// Agrega los conectores de frontera y garantiza el flujo entre clusters.
void ClusterSolutionMerger::add_values_from_boundary_connectors() {
    for (const ClusterProblem& problem : problems) {
        for (const ClusterVisit& visit : problem.boundary.visits) {
            if (visit.is_passthrough()) {
                for (int arc_id : visit.passthrough_arc_ids)
                    ++traversals_value_map[arc_id];

                if (visit.entry.kind == ClusterPortalKind::GlobalDeposit)
                    ++deposit_value_map[visit.entry.global_arc_id];

                if (visit.exit.kind == ClusterPortalKind::GlobalDeposit)
                    ++deposit_value_map[visit.exit.global_arc_id];

                continue;
            }

            const auto found = choices.find(visit.id);
            if (found == choices.end() || !found->second.count(true) ||
                !found->second.count(false))
                throw invalid_argument("La solucion local no respeta los extremos de la visita");

            const BoundaryGateChoice& entry = found->second.at(true);
            const BoundaryGateChoice& exit = found->second.at(false);

            if (visit.entry.kind == ClusterPortalKind::NeutralInOut)
                add_value_from_neutral_in_out_portal(visit.entry, entry, 1);
            else
                add_value_from_deposit_portal(visit.entry, entry, 1);
                
            if (visit.exit.kind == ClusterPortalKind::NeutralInOut)
                add_value_from_neutral_in_out_portal(visit.exit, exit, 1);
            else
                add_value_from_deposit_portal(visit.exit, exit, 1);
        }
    }
}

// Reconstruye el enlace entre el nodo virtual elegido dentro del cluster y el
// conector global entre clusters (neutros in-out).
void ClusterSolutionMerger::add_value_from_neutral_in_out_portal(const ClusterPortal& portal, const BoundaryGateChoice& gate, long long amount) {
    const SuperArc& neutral_in_out = *global_super_graph.super_arc_with_id(portal.global_arc_id);
    const int from = portal.entry ? neutral_in_out.to : gate.global_virtual_node;
    const int to = portal.entry ? gate.global_virtual_node : neutral_in_out.from;
    traversals_value_map[global_arc_by_edge.at({from, to})] += amount;
}

// Para un portal de deposito se reconstruye directamente el arco entre el
// deposito global y el nodo virtual elegido.
void ClusterSolutionMerger::add_value_from_deposit_portal(const ClusterPortal& portal, const BoundaryGateChoice& gate, long long amount) {
    const int deposit_node = global_super_graph.deposit();
    const int from = portal.entry ? deposit_node : gate.global_virtual_node;
    const int to = portal.entry ? gate.global_virtual_node : deposit_node;
    deposit_value_map[global_arc_by_edge.at({from, to})] += amount;
}

// Agrega los arcos entre los nodos neutros in-out.
void ClusterSolutionMerger::add_values_from_neutral_in_out() {
    // Cada cruce del circuito conectado cierra exactamente una visita. Derivar
    // las multiplicidades desde esas salidas conserva exactamente las
    // ocurrencias del circuito global validado por el extractor.
    for (const ClusterProblem& problem : problems)
        for (const ClusterVisit& visit : problem.boundary.visits)
            if (visit.exit.kind == ClusterPortalKind::NeutralInOut)
                traversals_value_map[visit.exit.global_arc_id] +=
                    visit.exit.count;
}
