#include "../../lib/graph/SuperGraph.h"
#include "../../lib/graph/Graph.h"
#include <algorithm>
#include <queue>
#include <random>
#include <stdexcept>

SuperGraph::SuperGraph(const Graph& original, const vector<Turn>& turns, const vector<Turn>& illegal_turns, const set<ExtendedTurn>& illegal_ext_turns) {
    if (original.deposit() < 0) throw invalid_argument("Grafo sin construir");
    const Graph* graph = &original;

    // In-memory instances need not arrive with their prohibitions sorted.
    auto sorted_illegal_turns = illegal_turns;
    sort(sorted_illegal_turns.begin(), sorted_illegal_turns.end());
    _arcs.reserve(2*(2*graph->_edges.size() + graph->_arcs.size()));
    _original_to_virtual.resize(graph->_n);

    for (int id: graph->_edges)
        add_super_arc_from_edge(graph, graph->_all_edges[id]);

    map<EdgeKey, int> neutral_arc_by_edgeKey;

    for (int id: graph->_arcs) {
        const Edge& arc = graph->_all_edges[id];

        if (graph->is_neutral_edge(id))
            neutral_arc_by_edgeKey.emplace(EdgeKey(arc.from, arc.to), id);
        else
            add_super_arc_from_arc(graph, arc);
    }

    _deposit = _n;

    add_super_arcs_between_nodes(original, sorted_illegal_turns, illegal_ext_turns, neutral_arc_by_edgeKey);

    add_super_arcs_with_deposit();

}

void SuperGraph::add_super_arc_from_edge(const Graph* graph, const Edge &edge) {
    add_new_arc(graph, edge, _m + 1);
    add_new_arc(graph, edge, _m - 1, true);    
}

void SuperGraph::add_super_arc_from_arc(const Graph* graph, const Edge &arc) { 
    add_new_arc(graph, arc);
}

void SuperGraph::add_super_arcs_between_nodes(const Graph& graph,
    const vector<Turn>& illegal_turns, const set<ExtendedTurn>& illegal_ext_turns,
    const map<EdgeKey, int>& neutral_arc_by_edgeKey) {
    vector<SuperArc> arcs_to_add;
    arcs_to_add.reserve(_arcs.size() * 2);

    // Los cruces conectan directamente dos calles fisicas: no comparten un
    // super-arco neutral que pierda la relacion entre llegada y salida.
    for (const SuperArc& arc1 : _arcs) {
        const int u = _virtual_to_original[arc1.from];
        const int v = _virtual_to_original[arc1.to];

        for (const SuperArc& arc2 : _arcs) {
            if (arc1.pair == arc2.id) continue;
            const int w = _virtual_to_original[arc2.from];
            const int k = _virtual_to_original[arc2.to];

            // Dos representaciones del mismo neutral tambien forman una U.
            if (u == k or 
                neutral_arc_by_edgeKey.count({u, k}) or
                neutral_arc_by_edgeKey.count({k, u})) continue;

            int edge_id = -1;
            double cost = 0;
            const bool crosses_neutral = v != w;
            if (crosses_neutral) {
                if (neutral_arc_by_edgeKey.count({v, w}) == 0) continue;

                const int neutral_id = neutral_arc_by_edgeKey.at({v, w});

                if (illegal_ext_turns.count(ExtendedTurn(u, v, w, k))) continue;

                const Edge& neutral = *graph.edge_with_id(neutral_id);
                edge_id = neutral.id;
                cost = neutral.cost;
            } else if (binary_search(illegal_turns.begin(), illegal_turns.end(), Turn(u, v, k))) {
                continue;
            }

            _node_to_super_in.add(arc1.to, _m);
            _node_to_super_out.add(arc2.from, _m);
            arcs_to_add.emplace_back(_m++, edge_id, -1, arc1.to, arc2.from, 0, cost, 0, -1, crosses_neutral);
            add_to_adj_list(arc1.to, arc2.from);
        }
    }

    _arcs.insert(_arcs.end(), arcs_to_add.begin(), arcs_to_add.end());
}

void SuperGraph::add_super_arcs_with_deposit() {
    // deposit_node_seeds contiene los ids de los nodos adyacentes al depo, pero lo queremos cambiar a que sea los id de los super arcos
    const vector<int> deposit_node_seeds = _adj_deposit_node;
    const vector<int> node_deposit_seeds = _adj_node_deposit;
    _adj_deposit_node.clear();
    _adj_node_deposit.clear();

    vector<SuperArc> arcs_to_add;
    arcs_to_add.reserve(deposit_node_seeds.size() + node_deposit_seeds.size());
    _is_adj_deposit_node.assign(_n + 1, 0);
    _is_adj_node_deposit.assign(_n + 1, 0);

    // Deposit -> node
    for (int seed_id: deposit_node_seeds) {
        const SuperArc& arc = _arcs[seed_id];
        if (arc.edge_id == -1) continue;

        _node_to_super_in.add(_deposit, _m);
        _node_to_super_out.add(arc.from, _m);
        _is_adj_deposit_node[arc.from] = true;

        SuperArc depo_node_arc = SuperArc(_m++, -2, -1, _deposit, arc.from, 0, 0, 0, -1);
        _adj_deposit_node.push_back(depo_node_arc.id);
        arcs_to_add.push_back(depo_node_arc);
        add_to_adj_list(_deposit, arc.from);
    }

    // Node -> deposit
    for (int seed_id: node_deposit_seeds) {
        const SuperArc& arc = _arcs[seed_id];
        if (arc.edge_id == -1) continue;

        _node_to_super_in.add(arc.to, _m);
        _node_to_super_out.add(_deposit, _m);
        _is_adj_node_deposit[arc.to] = true;

        SuperArc node_depo_arc = SuperArc(_m++, -2, -1, arc.to, _deposit, 0, 0, 0, -1);
        _adj_node_deposit.push_back(node_depo_arc.id);
        arcs_to_add.push_back(node_depo_arc);
        add_to_adj_list(arc.to, _deposit);
    }

    _arcs.insert(_arcs.end(), arcs_to_add.begin(), arcs_to_add.end());
}

void SuperGraph::add_adj_depo(const Graph* graph, int from, int to) {
    if (graph->_is_adj_deposit[from]) _adj_deposit_node.push_back(_m);
    if (graph->_is_adj_deposit[to]) _adj_node_deposit.push_back(_m);
}

void SuperGraph::add_node_map(int from, int to) {
    _node_to_super_in.add(_n, _m);
    _node_to_super_out.add(_n+1, _m);

    _original_to_virtual[from].push_back(_n);
    _original_to_virtual[to].push_back(_n+1);

    _virtual_to_original.push_back(from);
    _virtual_to_original.push_back(to);
}

void SuperGraph::add_to_adj_list(int from, int to) {
    if (_adj.size() < max(from, to) + 1) 
        _adj.resize(max(from, to) + 1);

    _adj[from].push_back(to);
}

void SuperGraph::add_new_arc(const Graph* graph, const Edge& edge, int pair_id, bool invert_direction) {
    int from = invert_direction ? edge.to   : edge.from;
    int to   = invert_direction ? edge.from : edge.to;
    
    // Deposit - nodes
    if (graph->allows_deposit_access(edge.id)) add_adj_depo(graph, from, to);

    // Maps
    add_node_map(from, to);
 
    // Super Arc
    SuperArc super_arc = edge.to_super_arc(
        _m, _n, _n+1, pair_id,
        graph->is_neutral_edge(edge.id));

    // Add to adjacency list
    add_to_adj_list(_n, _n+1);

    // Count new nodes
    _n += 2;

    // Add to arc list
    _arcs.push_back(super_arc);
    _m++;
}

int SuperGraph::original_node(int virtual_node) const {
    if (virtual_node < 0 || virtual_node >= static_cast<int>(_virtual_to_original.size()))
        throw out_of_range("Nodo virtual fuera de rango");
    return _virtual_to_original[virtual_node];
}

const vector<const SuperArc*> SuperGraph::super_arcs_from(int v) const {
    if (not _node_to_super_in.contains(v)) return vector<const SuperArc*>();
    return super_arcs_for_ids(_node_to_super_in.get(v));
}

const vector<const SuperArc*> SuperGraph::super_arcs_to(int v) const {
    if (not _node_to_super_out.contains(v)) return vector<const SuperArc*>();
    return super_arcs_for_ids(_node_to_super_out.get(v));
}

GraphSolution SuperGraph::to_graph_solution(const Solution& solution) const {
    GraphSolution projected;
    projected.objective = solution.objective;

    auto project = [&](const vector<ArcValue<long long>>& values,
                       map<int, long long>& multiplicities) {
        for (const ArcValue<long long>& value : values) {
            const SuperArc& arc = *super_arc_with_id(value.id);
            if (arc.edge_id >= 0 && value.value != 0)
                multiplicities[arc.edge_id] += value.value;
        }
    };

    project(solution.service, projected.service);
    project(solution.traversals, projected.traversals);
    return projected;
}

void SuperGraph::random_edge_neighborhood(edgeKeySet& edges, int d) const {
    if (_n == 0) return;
    static mt19937 generator(random_device{}());
    const int start = uniform_int_distribution<int>(0, _n - 1)(generator);
    add_edge_neighborhood(edges, start, d, arcs_amount());
}

void SuperGraph::add_edge_neighborhood(edgeKeySet& selected_edges, int start, int d, size_t max_amount) const {
    if (start < 0 || start >= _n or d < 0)
        throw invalid_argument("Vecindad BFS invalida");
    if (max_amount == 0) return;

    vector<int> distance(_n, -1);
    queue<int> pending;
    distance[start] = 0;
    pending.push(start);

    while (!pending.empty()) {
        const int u = pending.front();
        pending.pop();
        if (distance[u] == d) continue;

        for (int v : _adj[u]) {
            if (v == _deposit || distance[v] != -1) continue;
            distance[v] = distance[u] + 1;
            pending.push(v);
        }
    }

    // Agregar not tree edges
    for (const SuperArc& arc : _arcs) {
        EdgeKey edge;
        bool include = false;

        if (arc.edge_id == -2) {
            // Solution and RCPPSolver represent the synthetic deposit as -1.
            if (arc.from == _deposit && distance[arc.to] >= 0 && distance[arc.to] < d) {
                edge = {-1, arc.to};
                include = true;
            } else if (arc.to == _deposit && distance[arc.from] >= 0 && distance[arc.from] < d) {
                edge = {arc.from, -1};
                include = true;
            }
        } else if (distance[arc.from] >= 0 && distance[arc.to] >= 0) {
            edge = {arc.from, arc.to};
            include = true;
        }

        if (!include) continue;
        if (selected_edges.size() == max_amount) break;
        selected_edges.insert(edge);
    }
}

const vector<const SuperArc*> SuperGraph::super_arcs_for_ids(const vector<int>& node_ids) const {
    vector<const SuperArc*> result;
    result.reserve(node_ids.size()); 

    for(int id: node_ids) 
        result.push_back(&_arcs[id]);  
    
    return result;
}

bool SuperGraph::is_adj_depo_node(int v) const {
    return _is_adj_deposit_node[v];
}

bool SuperGraph::is_adj_node_depo(int v) const {
    return _is_adj_node_deposit[v];
}

int SuperGraph::deposit() const {
    return _deposit;
}

// Testing
void SuperGraph::write() {
    ofstream out("supergrafo.out");
    if (!out.is_open()) {
        cerr << "Error al abrir el archivo de salida.\n";
        return;
    }

    out << "Arcos:\n";
    for (const auto& sn : _arcs) {
        out << "ID: " << sn.id
            << " (" << sn.from +1 << " -> " << sn.to + 1 << ")"
            << " | EDGE: " << sn.edge_id
            << " | REQ_ID: " << sn.requested_idx
            << " | Zona: " << sn.zone
            << " | Costo: " << sn.cost
            << " | Demanda: " << sn.demand
            << " | Obligatorio: " << (sn.requested? "Sí" : "No") << "\n";
    }

    out << "\nNodos\n";

    for (int v = 0; v < _n; v++) 
        out << v + 1 << ": " << _virtual_to_original[v] + 1 << endl;
    

    out.close();
    
    ofstream sg("sg" + to_string(_adj.size()) + ".dat");
    if (!sg.is_open()) {
        cerr << "Error al abrir el archivo de salida.\n";
        return;
    }

    sg << _adj.size() << "\n";

    for (int v = 0; v < _adj.size(); v++) {
        for (const SuperArc* arc : super_arcs_from(v))
            sg << v << " " << arc->to << " " << arc->cost << "\n";
    }

    sg.close();

}
