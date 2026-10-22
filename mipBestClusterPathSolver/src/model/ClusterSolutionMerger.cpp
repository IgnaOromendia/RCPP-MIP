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
        if (arc.edge_id >= 0 and instance.arc_clusters.at(arc.edge_id) > 0)
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

    // Comprueba que cada registro requerido (zone != 0) se haya servido una vez.
    // record sigue el orden comun de edges seguido por arcs usado para construir
    // instance.arc_clusters.
    int record = 0;
    auto validate_records = [&](const vector<InstanceEdge>& edges) {
        for (const InstanceEdge& edge : edges) {
            if (edge.zone != 0 && service_by_record[record] != 1)
                throw invalid_argument(
                    "La solucion combinada no conserva todos los servicios");
            ++record;
        }
    };
    validate_records(instance.rcpp.edges);
    validate_records(instance.rcpp.arcs);
}

void ClusterSolutionMerger::add_solution(Solution sol) {
    cluster_solutions.push_back(sol);
}

// Incorpora el servicio y los recorridos internos de cada cluster.
void ClusterSolutionMerger::add_values_from_local_solution() {
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
            const SuperArc& local_arc = *problem.super_graph->super_arc_with_id(value.id);
            const bool entry = value.from == -1;
            const int local_node = entry ? local_arc.to : local_arc.from;
            const int global_virtual = problem.mapping.local_to_global_node.at(local_node);
            const int global_node = global_super_graph.original_node(global_virtual);
            choices[problem.cluster][{global_node, entry}].push_back({global_virtual, value.value});
        }
    }
}

// Agrega los conectores de frontera y garantiza el flujo entre clusters.
void ClusterSolutionMerger::add_values_from_boundary_connectors() {
    // Empareja cada portal exigido por la solucion global con las elecciones de
    // frontera realizadas por la solucion local del cluster correspondiente.
    for (const ClusterProblem& problem : problems) {
        auto& cluster_choices = choices[problem.cluster];
        for (const ClusterPortal& portal : problem.boundary.portals) {

            // Un portal puede usarse varias veces y una misma frontera puede
            // repartirse entre varios nodos virtuales equivalentes. Se consume
            // la multiplicidad hasta satisfacer exactamente portal.count.
            long long remaining = portal.count;
            for (BoundaryGateChoice& gate : cluster_choices.at({portal.global_node, portal.entry})) {
                if (remaining == 0) break;

                const long long amount = min(remaining, gate.count);
                if (amount == 0) continue;

                if (portal.kind == ClusterPortalKind::NeutralInOut)
                    add_value_from_neutral_in_out_portal(portal, gate, amount);
                else
                    add_value_from_deposit_portal(portal, gate, amount);

                gate.count -= amount;
                remaining -= amount;
            }

            // Faltaron usos locales para cubrir la multiplicidad requerida por
            // este portal de la solucion global.
            if (remaining != 0)
                throw invalid_argument(
                    "La solucion solution no respeta la multiplicidad de frontera");
        }

        // La comprobacion inversa evita aceptar entradas o salidas adicionales
        // creadas por la solucion local que no existian en la frontera global.
        for (const auto& [key, values] : cluster_choices) {
            (void) key;
            for (const BoundaryGateChoice& choice : values)
                if (choice.count != 0)
                    throw invalid_argument(
                        "La solucion solution agrega usos de frontera");
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
    for (const ArcValue<long long>& value : global_solution.traversals)
        if (value.value > 0 && neutral_in_out_ids.count(value.id))
            traversals_value_map[value.id] += value.value;
}
