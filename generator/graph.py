class Graph:
    def __init__(self, n = 1):
        self.n = n

        self.nodeIds = []

        self.adj = [[] for _ in range(n)]
        self.adjT = [[] for _ in range(n)]

        self.req_edges = []
        self.not_req_edges = []

        self.req_arcs = []
        self.not_req_arcs = []

        # Attributes
        self.dist   = [[0] * n for _ in range(n)]
        self.demand = [[0] * n for _ in range(n)]
        self.time   = [[0] * n for _ in range(n)]

    # Edges

    def isAnEdge(self, u, v):
        return (u, v) in self.req_edges or (u, v) in self.not_req_edges or (v, u) in self.req_edges or (v, u) in self.not_req_edges 

    def edgeTimeString(self, v, u):
        return f"{self.time[v][u]:.12g}"
    
    def edgeDemandString(self, v, u):
        return f"{self.demand[v][u]:.12g}"

    # Neighbours

    def incidentNodes(self, v):
        return self.adjT[v].copy()

    def neighbours(self, v):
        return self.adj[v].copy()

    # Special nodes

    def dOutZeroNodes(self):
        return [v for v in range(1, self.n) if self.dIN(v) >= 1 and self.dOUT(v) == 0]
    
    def dInZeroNodes(self):
        return [v for v in range(1, self.n) if self.dIN(v) == 0 and self.dOUT(v) == 1]
    
    def oneNeighbourNodes(self):
        return [v for v in range(1, self.n) if self.dIN(v) == 1 and self.dOUT(v) == 1 and self.adj[v] == self.adjT[v]]

    # Degrees

    def dOUT(self, v):
        return len(self.adj[v])
    
    def dIN(self,v):
        return len(self.adjT[v])

    # Adding

    def addNode(self):
        self.adj.append([])
        self.adjT.append([])
        self.n += 1

    def addArc(self, v, u, isRequested):
        self.adj[v].append(u)
        self.adjT[u].append(v)

        if isRequested: self.req_arcs.append((v,u))
        else: self.not_req_arcs.append((v,u))

    def addEdge(self, v, u, isRequested):
        self.adj[v].append(u)
        self.adj[u].append(v)

        self.adjT[v].append(u)
        self.adjT[u].append(v)

        if isRequested: self.req_edges.append((v,u))
        else: self.not_req_edges.append((v,u))

    def addWeightedEdge(self, v, u, time, demand, zone):
        self.addEdge(v, u, zone != 0)

        self.demand[v][u] = demand
        self.demand[u][v] = demand

        self.time[v][u] = time
        self.time[u][v] = time

    def addWeightedArc(self, v, u, time, demand, zone):
        self.addArc(v, u, zone != 0)
        
        self.demand[v][u] = demand

        self.time[v][u] = time

    # Remove

    def removeEdge(self, v, u):
        if u in self.adj[v]: self.adj[v].remove(u)
        if v in self.adj[u]: self.adj[u].remove(v)

        if u in self.adjT[v]: self.adjT[v].remove(u)
        if v in self.adjT[u]: self.adjT[u].remove(v)

        self.removeFromEdgeList((v, u))
        self.removeFromEdgeList((u, v))

    def removeFromEdgeList(self, way):
        if way in self.req_arcs: self.req_arcs.remove(way)
        if way in self.not_req_arcs: self.not_req_arcs.remove(way)

        if way in self.req_edges: self.req_edges.remove(way)
        if way in self.not_req_edges: self.not_req_edges.remove(way)

    # Reassing

    def reassingIds(self):
        adjCopy = self.adj.copy()

        nodos_validos = [i for i, vecinos in enumerate(adjCopy) if len(vecinos) > 0]
        
        if 0 not in nodos_validos:
            nodos_validos.append(0)
        
        newIdMap = {0: 0}  
        for i, nodo_viejo in enumerate(nodos_validos):
            if nodo_viejo != 0:  
                newIdMap[nodo_viejo] = i + 1  

        self.adj = [
            [] if i == 0 else [newIdMap[v] for v in vecinos if v in newIdMap]
            for i, vecinos in enumerate(adjCopy) if len(vecinos) > 0 or i == 0
        ]

        self.n = len(self.adj)

        self.reassingInEdges(self.req_edges, newIdMap)
        self.reassingInEdges(self.not_req_edges, newIdMap)

        self.reassingInEdges(self.req_arcs, newIdMap)
        self.reassingInEdges(self.not_req_arcs, newIdMap)

        return newIdMap
    
    def reassingInEdges(self, edges, newIdMap):
        ways = edges.copy()
        edges.clear()
        for (v,u) in ways:
            edges.append((newIdMap[v], newIdMap[u]))

    # Print

    def printGraphData(self):
        print("Grafo generado:\n" + str(self.n) + " nodos\n"
                                  + "-----\n" 
                                  + str(len(self.req_edges) + len(self.not_req_edges)) + " aristas\n" 
                                  + str(len(self.req_edges)) + " obligatorias\n" 
                                  + str(len(self.not_req_edges)) + " no obligatorias\n"
                                  + "-----\n" 
                                  + str(len(self.req_arcs) + len(self.not_req_arcs)) + " arcos\n"
                                  + str(len(self.req_arcs)) + " obligatorios\n" 
                                  + str(len(self.not_req_arcs)) + " no obligatorios")
