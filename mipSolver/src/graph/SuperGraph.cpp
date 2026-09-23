#include "../../lib/graph/SuperGraph.h"
#include "../../lib/graph/Graph.h"
#include <algorithm>
#include <queue>
#include <random>
#include <stdexcept>

SuperGraph::SuperGraph(const Graph& original, const vector<Turn>& turns, const vector<Turn>& illegal_turns) {
    if (original.deposit() < 0) throw std::invalid_argument("Grafo sin construir");
    const Graph* graph = &original;
    // In-memory instances need not arrive with their prohibitions sorted.
    auto sorted_illegal_turns = illegal_turns;
    std::sort(sorted_illegal_turns.begin(), sorted_illegal_turns.end());
    this->_arcs.reserve(2*(2*graph->_edges.size() + graph->_arcs.size()));
    this->_original_to_virtual.resize(graph->_n);

    for (const Edge& edge: graph->_edges)
        this->add_super_arc_from_edge(graph, edge);

    for (const Edge& arc: graph->_arcs)
        this->add_super_arc_from_arc(graph, arc);

    this->_deposit = this->_n;

    this->add_super_arcs_between_nodes(turns, sorted_illegal_turns);

    this->add_super_arcs_with_deposit();    

}

void SuperGraph::add_super_arc_from_edge(const Graph* graph, const Edge &edge) {
    this->add_new_arc(graph, edge, this->_m + 1);
    this->add_new_arc(graph, edge, this->_m - 1, true);    
}

void SuperGraph::add_super_arc_from_arc(const Graph* graph, const Edge &arc) { 
    this->add_new_arc(graph, arc);
}

void SuperGraph::add_super_arcs_between_nodes(const vector<Turn> &turns, const vector<Turn> &illegal_turns) {
    vector<SuperArc> arcs_to_add; 
    arcs_to_add.reserve(this->_arcs.size() * 2); // Es un poco mucho capaz pero es seguro

    // Para cada par uv chequeamos contra todo par wk tal que u != k y v == w
    for(SuperArc& arc1: this->_arcs) {
        if (arc1.cost == 0) continue;
        int u = this->_virtual_to_original[arc1.from];
        int v = this->_virtual_to_original[arc1.to];

        for(SuperArc& arc2: this->_arcs) {
            if (arc2.cost == 0 or arc1.pair == arc2.id) continue;
            int w = this->_virtual_to_original[arc2.from];
            int k = this->_virtual_to_original[arc2.to];

            // Si no se conectan o generan una vuelta en U no los conectamos
            if (v != w or u == k) continue;

            Turn possible_turn = Turn(u, v, k);

            // Si es un giro prohibido no lo agregamos
            if (binary_search(illegal_turns.begin(), illegal_turns.end(), possible_turn)) continue;

            this->_node_to_super_in.add(arc1.to, this->_m);
            this->_node_to_super_out.add(arc2.from, this->_m);

            SuperArc virtual_arc = SuperArc(this->_m++, -1, -1, arc1.to, arc2.from, 0, 0, 0, -1);
            arcs_to_add.push_back(virtual_arc);

            this->add_to_adj_list(arc1.to, arc2.from, 0);
        }
    }

    this->_arcs.insert(this->_arcs.end(), arcs_to_add.begin(), arcs_to_add.end());
}

void SuperGraph::add_super_arcs_with_deposit() {
    // deposit_node_seeds contiene los ids de los nodos adyacentes al depo, pero lo queremos cambiar a que sea los id de los super arcos
    const vector<int> deposit_node_seeds = this->_adj_deposit_node;
    const vector<int> node_deposit_seeds = this->_adj_node_deposit;
    this->_adj_deposit_node.clear();
    this->_adj_node_deposit.clear();

    vector<SuperArc> arcs_to_add;
    arcs_to_add.reserve(deposit_node_seeds.size() + node_deposit_seeds.size());
    this->_is_adj_deposit_node.assign(this->_n + 1, 0);
    this->_is_adj_node_deposit.assign(this->_n + 1, 0);

    // Deposit -> node
    for (int seed_id: deposit_node_seeds) {
        const SuperArc& arc = this->_arcs[seed_id];
        if (arc.edge_id == -1) continue;

        this->_node_to_super_in.add(this->_deposit, this->_m);
        this->_node_to_super_out.add(arc.from, this->_m);
        this->_is_adj_deposit_node[arc.from] = true;

        SuperArc depo_node_arc = SuperArc(this->_m++, -2, -1, this->_deposit, arc.from, 0, 0, 0, -1);
        this->_adj_deposit_node.push_back(depo_node_arc.id);
        arcs_to_add.push_back(depo_node_arc);
        this->add_to_adj_list(this->_deposit, arc.from, 0);
    }

    // Node -> deposit
    for (int seed_id: node_deposit_seeds) {
        const SuperArc& arc = this->_arcs[seed_id];
        if (arc.edge_id == -1) continue;

        this->_node_to_super_in.add(arc.to, this->_m);
        this->_node_to_super_out.add(this->_deposit, this->_m);
        this->_is_adj_node_deposit[arc.to] = true;

        SuperArc node_depo_arc = SuperArc(this->_m++, -2, -1, arc.to, this->_deposit, 0, 0, 0, -1);
        this->_adj_node_deposit.push_back(node_depo_arc.id);
        arcs_to_add.push_back(node_depo_arc);
        this->add_to_adj_list(arc.to, this->_deposit, 0);
    }

    this->_arcs.insert(this->_arcs.end(), arcs_to_add.begin(), arcs_to_add.end());
}

void SuperGraph::add_adj_depo(const Graph* graph, int from, int to) {
    if (graph->_is_adj_deposit[from]) this->_adj_deposit_node.push_back(this->_m);
    if (graph->_is_adj_deposit[to]) this->_adj_node_deposit.push_back(this->_m);
}

void SuperGraph::add_node_map(int from, int to) {
    this->_node_to_super_in.add(this->_n, this->_m);
    this->_node_to_super_out.add(this->_n+1, this->_m);

    this->_original_to_virtual[from].push_back(this->_n);
    this->_original_to_virtual[to].push_back(this->_n+1);

    this->_virtual_to_original.push_back(from);
    this->_virtual_to_original.push_back(to);
}

void SuperGraph::add_to_adj_list(int from, int to, double cost) {\
    if (this->_adj.size() < max(from, to) + 1) 
        this->_adj.resize(max(from, to) + 1);

    // cout << _adj.size() << " " << max(from, to) << endl;

    this->_adj[from].emplace_back(to, cost);
}

void SuperGraph::add_new_arc(const Graph* graph, const Edge& edge, int pair_id, bool invert_direction) {
    int from = invert_direction ? edge.to   : edge.from;
    int to   = invert_direction ? edge.from : edge.to;
    
    // Deposit - nodes
    this->add_adj_depo(graph, from, to);

    // Maps
    this->add_node_map(from, to);
 
    // Super Arc
    SuperArc super_arc = edge.to_super_arc(this->_m, this->_n, this->_n+1, pair_id);

    // Add to adjacency list
    this->add_to_adj_list(this->_n, this->_n+1, edge.cost);

    // Count new nodes
    this->_n += 2;

    // Add to arc list
    this->_arcs.push_back(super_arc);
    this->_m++;
}

int SuperGraph::arcs_amount() const {
    return this->_m;
}

int SuperGraph::nodes_amount() const {
    return this->_n;
}

const vector<SuperArc>& SuperGraph::arcs() const {
    return this->_arcs;
}

const SuperArc* SuperGraph::super_arc_with_id(int arc_id) const {
    return &this->_arcs[arc_id];
}

const vector<const SuperArc*> SuperGraph::super_arcs_from(int v) const {
    if (not this->_node_to_super_in.contains(v)) return vector<const SuperArc*>();
    return this->super_arcs_for_ids(this->_node_to_super_in.get(v));
}

const vector<const SuperArc*> SuperGraph::super_arcs_to(int v) const {
    if (not this->_node_to_super_out.contains(v)) return vector<const SuperArc*>();
    return this->super_arcs_for_ids(this->_node_to_super_out.get(v));
}

const vector<const SuperArc*> SuperGraph::super_arcs_adj_depo_node() const {
    return this->super_arcs_for_ids(this->_adj_deposit_node);
}

const vector<const SuperArc*> SuperGraph::super_arcs_adj_node_depo() const {
    return this->super_arcs_for_ids(this->_adj_node_deposit);
}

vector<EdgeKey> SuperGraph::random_edge_neighborhood(int d) const {
    if (_n == 0) return {};
    static std::mt19937 generator(std::random_device{}());
    const int start = std::uniform_int_distribution<int>(0, _n - 1)(generator);
    return bfs_tree(start, d);
}

vector<EdgeKey> SuperGraph::bfs_tree(int start, int d) const {
    vector<int> distance(_n, -1);
    vector<pair<int, int>> result;
    queue<int> pending;
    distance[start] = 0;
    pending.push(start);

    while (!pending.empty()) {
        const int u = pending.front();
        pending.pop();
        if (distance[u] == d) continue;

        for (const Node& v : _adj[u]) {
            if (v.id == _deposit || distance[v.id] != -1) continue;
            distance[v.id] = distance[u] + 1;
            result.emplace_back(u, v.id);
            pending.push(v.id);
        }
    }

    return result;
}

const vector<const SuperArc*> SuperGraph::super_arcs_for_ids(const vector<int>& node_ids) const {
    vector<const SuperArc*> result;
    result.reserve(node_ids.size()); 

    for(int id: node_ids) 
        result.push_back(&this->_arcs[id]);  
    
    return result;
}

bool SuperGraph::is_adj_depo_node(int v) const {
    return this->_is_adj_deposit_node[v];
}

bool SuperGraph::is_adj_node_depo(int v) const {
    return this->_is_adj_node_deposit[v];
}

int SuperGraph::deposit() const {
    return this->_deposit;
}

void SuperGraph::calculate_depo_dists() {
    // Sparse Dijkstra
    priority_queue<pair<long long, int> > heap;
    vector<bool> visited(this->_adj.size(), false);
    this->_depo_dist.assign(this->_adj.size(), 1e9);

    this->_depo_dist[this->_deposit] = 0;
    heap.push(make_pair(0, this->_deposit));

    while(!heap.empty()) {
        int u = heap.top().second; heap.pop();

        if(visited[u]) continue;
        visited[u] = true;

        for(Node v: this->_adj[u]) {
            if (this->_depo_dist[v.id] > _depo_dist[u] + v.cost) {
                _depo_dist[v.id] = _depo_dist[u] + v.cost;
                heap.push(make_pair(-_depo_dist[v.id], v.id));
            }
        }   
    }
}

const vector<double>& SuperGraph::deposit_dist() const {
    return this->_depo_dist;
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

    for (int v = 0; v < this->_n; v++) 
        out << v + 1 << ": " << this->_virtual_to_original[v] + 1 << endl;
    

    out.close();
    
    ofstream sg("sg" + to_string(this->_adj.size()) + ".dat");
    if (!sg.is_open()) {
        cerr << "Error al abrir el archivo de salida.\n";
        return;
    }

    sg << this->_adj.size() << "\n";

    for (int v = 0; v < this->_adj.size(); v++) {
        for (Node u : this->_adj[v])
            sg << v << " " << u.id << " " << u.cost << "\n";
    }

    sg.close();

}
