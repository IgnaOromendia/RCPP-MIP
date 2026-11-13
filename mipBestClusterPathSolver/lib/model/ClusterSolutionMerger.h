#ifndef CLUSTER_SOLUTION_MERGER_H
#define CLUSTER_SOLUTION_MERGER_H

#include <graph/NeutralExpandedGraph.h>
#include <model/ClusterSubProblemBuilder.h>
#include <model/Solution.h>
#include <graph/SuperGraph.h>
#include <map>
#include <set>

// Reconstruye una unica Solution global a partir de las soluciones RCPP de los
// clusters. Conserva los neutral_in_out elegidos por el solver global, reemplaza
// el recorrido interno de cada cluster por su solucion local y vuelve a unir
// ambas partes mediante los portales de frontera.
class ClusterSolutionMerger {
public:
    ClusterSolutionMerger(
        const SuperGraph& global_super_graph,
        const NeutralExpandedGraph& instance,
        const vector<ClusterProblem>& problems);

    // Combina las soluciones agregadas, valida frontera, continuidad y servicio,
    // y recalcula el objetivo de la solucion resultante.
    Solution build_solution();

    // Agrega una solucion local respetando el orden del vector problems.
    void add_solution(Solution sol);

private:
    const SuperGraph& global_super_graph;
    const NeutralExpandedGraph& instance;

    const vector<ClusterProblem>& problems;
    vector<Solution> cluster_solutions;
    
    map<EdgeKey, int> deposit_arc_by_edge;

    map<int, long long> service_value_map, traversals_value_map, deposit_value_map;

    // choices[visit_id][entry] identifica el extremo virtual concreto elegido
    // por el vehiculo local de esa visita; true = entrada, false = salida.
    map<int, map<bool, int>> choices;

    void add_values_from_local_solution();
    void add_values_from_boundary_connectors();
    void validate_neutral_portal(
        const ClusterPortal& portal,
        int global_virtual_node, bool entry) const;
    void add_value_from_deposit_portal(
        int global_virtual_node, bool entry);
    void add_values_from_neutral_in_out();
    Solution merge() const;
    void validate_flow() const;
    void validate_service() const;
    void validate_connected() const;
};

#endif
