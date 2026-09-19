#include "../lib/Graph.h"

Graph::Graph() {}

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
        // Standard
        if (zone != 0) this->_requested_idx_to_edge.push_back(i);
        this->_edges.emplace_back(i, from-1, to-1, zone, cost, demand, this->_requested_idx_to_edge.size() - 1);
        // CG
		this->_all_edges.emplace_back(i, from-1, to-1, zone, cost, demand, this->_requested_idx_to_edge.size() - 1, true);
        this->_adj[from-1].emplace_back(to-1, i);
        this->_adj[to-1].emplace_back(from-1, i);
	}

    // Lectura de los arcos
    for(int i = 0; i < mArcs; i++){
        f >> from >> to >> zone >> cost >> demand;
        // Standard
        if (zone != 0) this->_requested_idx_to_edge.push_back(i);
        this->_arcs.emplace_back(i, from-1, to-1, zone, cost, demand, this->_requested_idx_to_edge.size() - 1);
        // CG
		this->_all_edges.emplace_back(i + mEdges, from-1, to-1, zone, cost, demand, this->_requested_idx_to_edge.size() - 1);
        this->_adj[from-1].emplace_back(to-1, i + mEdges);
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