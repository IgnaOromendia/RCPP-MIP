#ifndef CLUSTER_BOUNDARY_H
#define CLUSTER_BOUNDARY_H

#include <graph/NeutralExpandedGraph.h>
#include <model/Solution.h>
#include <model/SubtourBreaker.h>
#include <graph/SuperGraph.h>
#include <algorithm>
#include <map>
#include <vector>

enum class ClusterPortalKind { NeutralInOut, GlobalDeposit };

// Ejemplo: u -> v_c1 -> v_c2 -> k cruza de c1 a c2 por el super-arco 42,
// que une el nodo virtual 11 (en v_c1 desde y) con el 20 (en k desde v_c2).
// Los dos portales de esa ocurrencia del cruce son:
//   salida de c1:  {NeutralInOut, 42, v_c1, 11}
//   entrada a c2: {NeutralInOut, 42, v_c2, 20}
// global_node identifica la representacion del neutral; global_virtual_node
// fija el extremo de la calle elegida. Ambos portales comparten global_arc_id.
struct ClusterPortal {
    ClusterPortalKind kind;
    int global_arc_id;
    int global_node;
    // Extremo exacto del cruce en el supergrafo global. El deposito usa -1.
    int global_virtual_node = -1;
};

struct ClusterVisit {
    int id;
    ClusterPortal entry;
    ClusterPortal exit;
    vector<int> service_edge_ids;
    bool passthrough = false;
    // Una visita formada solo por conectores de giro no tiene un recorrido
    // interno que optimizar. Esos arcos globales se conservan al fusionar.
    vector<int> passthrough_arc_ids;

    bool is_passthrough() const noexcept {
        return passthrough;
    }
};

class ClusterBoundary {
public:
    vector<ClusterVisit> visits;

    size_t optimized_visit_count() const {
        return static_cast<size_t>(std::count_if(
            visits.begin(), visits.end(),
            [](const ClusterVisit& visit) {
                return !visit.is_passthrough();
            }));
    }
};

class ClusterBoundaryExtractor {
public:
    ClusterBoundaryExtractor(
        const SuperGraph& super_graph,
        const NeutralExpandedGraph& instance,
        const Solution& solution,
        const vector<ProvisionalConnection>& connections = {});

    map<int, ClusterBoundary> get_boundaries();

private:
    const SuperGraph& super_graph;
    const NeutralExpandedGraph& _expanded_graph;
    const Solution& solution;
    vector<ProvisionalConnection> connections;

    map<int, int> cluster_by_node;
};

#endif
