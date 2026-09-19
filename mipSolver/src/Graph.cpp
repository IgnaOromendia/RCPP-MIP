#include "../lib/Graph.h"

Graph::Graph(string file_name) {
    ifstream f(file_name.c_str());

	if(f.fail()){
		cout << "Error en archivo " << file_name << "\n";
		exit(0);
	}

    int mEdges, mArcs, ignore;

	f >> ignore;
    f >> this->_n;
	f >> this->_adj_deposit_amount; 
	f >> mEdges;
	f >> mArcs;

    // Inicializa estructuras
    this->_deposit = this->_n;
    this->_is_adj_deposit.assign(this->_n, false);
    this->_adj_depoist_nodes.resize(this->_adj_deposit_amount);
    this->_all_edges.reserve(mEdges + mArcs);
    this->_arcs.reserve(mArcs);
    this->_adj.resize(this->_n + 2);

    // Lectura nodos adyacentes al depo
	for(int i = 0; i < this->_adj_deposit_amount; i++){
		int n; f >> n;
		this->_is_adj_deposit[n-1] = true;
        this->_adj_depoist_nodes[i] = n-1;
	}

    // Lectura de ejes
	int zone, from , to;
	float cost, demand;
    
    // Lectura de las aristas
    for(int i = 0; i < mEdges; i++){
        f >> from >> to >> zone >> cost >> demand;
        const int edge_id = i;
        const int requested_idx = zone != 0 ? static_cast<int>(this->_requested_idx_to_edge.size()) : -1;
        // Standard
        if (zone != 0) this->_requested_idx_to_edge.push_back(edge_id);
        this->_edges.emplace_back(edge_id, from-1, to-1, zone, cost, demand, requested_idx);
        // CG
		this->_all_edges.emplace_back(edge_id, from-1, to-1, zone, cost, demand, requested_idx, true);
        this->_adj[from-1].emplace_back(to-1, edge_id);
        this->_adj[to-1].emplace_back(from-1, edge_id);
	}

    // Lectura de los arcos
    for(int i = 0; i < mArcs; i++){
        f >> from >> to >> zone >> cost >> demand;
        // i is local to _arcs; edge_id indexes the shared _all_edges container.
        const int edge_id = mEdges + i;
        const int requested_idx = zone != 0 ? static_cast<int>(this->_requested_idx_to_edge.size()) : -1;
        // Standard
        if (zone != 0) this->_requested_idx_to_edge.push_back(edge_id);
        this->_arcs.emplace_back(edge_id, from-1, to-1, zone, cost, demand, requested_idx);
        // CG
		this->_all_edges.emplace_back(edge_id, from-1, to-1, zone, cost, demand, requested_idx);
        this->_adj[from-1].emplace_back(to-1, edge_id);
	}

    for(int v: this->_adj_depoist_nodes) {
        this->_adj[_deposit].emplace_back(v, this->_all_edges.size());
        this->_adj[v].emplace_back(_deposit, this->_all_edges.size());
        this->_all_edges.emplace_back(this->_all_edges.size(), this->_deposit, v, 0, 0, 0, -1, true);
    }        

    f.close();
}

Graph::~Graph() {}

int Graph::nodes_amount() const {
    return this->_n;
}

int Graph::deposit() const {
    return this->_deposit;
}

const vector<Node>& Graph::neighbours(int v) const {
    return this->_adj[v];
}

int Graph::requested_amount() const {
    return this->_requested_idx_to_edge.size();
}

const vector<const Edge*> Graph::requested() const {
    vector<const Edge*> result;
    result.reserve(this->_requested_idx_to_edge.size()); 

    for(int id: this->_requested_idx_to_edge)
        result.push_back(&this->_all_edges[id]);
    
        
    return result;
}

const Edge* Graph::edge_with_id(int id) const {
    return &this->_all_edges[id];
}
