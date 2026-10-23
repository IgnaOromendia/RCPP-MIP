#ifndef CLUSTER_SOLUTION_MERGER_H
#define CLUSTER_SOLUTION_MERGER_H

#include <model/BestClusterPathInstance.h>
#include <model/ClusterSubProblemBuilder.h>
#include <model/Solution.h>
#include <graph/SuperGraph.h>
#include <map>
#include <set>

// Cada recorrido al deposito artificial de un subproblema representa una
// eleccion concreta de nodo virtual por el cual entrar o salir del cluster.
struct BoundaryGateChoice {
    int global_virtual_node;
};

// Reconstruye una unica Solution global a partir de las soluciones RCPP de los
// clusters. Conserva los neutral_in_out elegidos por el solver global, reemplaza
// el recorrido interno de cada cluster por su solucion local y vuelve a unir
// ambas partes mediante los portales de frontera.
class ClusterSolutionMerger {
public:
    ClusterSolutionMerger(
        const SuperGraph& global_super_graph,
        const BestClusterPathInstance& instance,
        const Solution& global_solution,
        const vector<ClusterProblem>& problems,
        const map<EdgeKey, int>& global_arc_by_edge,
        const set<int>& neutral_in_out_ids)
        : global_super_graph(global_super_graph),
          instance(instance),
          global_solution(global_solution),
          problems(problems),
          global_arc_by_edge(global_arc_by_edge),
          neutral_in_out_ids(neutral_in_out_ids) {}

    // Combina las soluciones agregadas, valida frontera, continuidad y servicio,
    // y recalcula el objetivo de la solucion resultante.
    Solution build_solution();

    // Agrega una solucion local respetando el orden del vector problems.
    void add_solution(Solution sol);

private:
    const SuperGraph& global_super_graph;
    const BestClusterPathInstance& instance;

    const Solution& global_solution;
    const vector<ClusterProblem>& problems;
    vector<Solution> cluster_solutions;
    
    const map<EdgeKey, int>& global_arc_by_edge;
    const set<int>& neutral_in_out_ids;

    map<int, long long> service_value_map, traversals_value_map, deposit_value_map;

    // choices[visit_id][entry] identifica el extremo virtual concreto elegido
    // por el vehiculo local de esa visita.
    map<int, map<bool, BoundaryGateChoice>> choices;

    void add_values_from_local_solution();
    void add_values_from_boundary_connectors();
    void add_value_from_neutral_in_out_portal(
        const ClusterPortal& portal,
        const BoundaryGateChoice& gate,
        long long amount);
    void add_value_from_deposit_portal(
        const ClusterPortal& portal,
        const BoundaryGateChoice& gate,
        long long amount);
    void add_values_from_neutral_in_out();
    Solution merge() const;
    void validate_flow() const;
    void validate_service() const;
    void validate_connected() const;
};

#endif
