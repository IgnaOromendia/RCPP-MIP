from shapely.geometry import shape, Point, Polygon # type: ignore
from geometryTool import GeometryTool as gt
import numpy as np

class Grid:
    def __init__(self, graph, maxCells, points, geoJSONData):
        self.graph = graph

        self.adjGrid = [[]]
        self.numberOfEdges = 0

        self.maxCells = maxCells

        self.points = points
        self.polyZone = self.polyZone = shape(geoJSONData["features"][0]["geometry"])
        self.cellPolygons = {}

        # Grid extreme points
        self.max_lat = -float('inf')
        self.min_lat = float('inf')
        self.max_lon = -float('inf')
        self.min_lon = float('inf')

        # Grid dims
        self.height = 0
        self.width  = 0

        self.grid_top_left      = (1,0)
        self.grid_top_right     = (1,1)
        self.grid_bottom_right  = (0,1)
        self.grid_bottom_left   = (0,0)

        #Cell
        self.numberOfCells = 0
        self.cell_width = 0
        self.cell_height = 0
        self.cellCoordinates = []
        self.cellDemand = []

        # Map from node to cell
        self.nodeCellMap = {}

        # Distances
        self.cellDist = [[]]
        self.maxCellDist = 0

    def generate(self, coords, theta):
        self.extremeCoords(coords)

        self.rotatePolygon(theta)
        
        self.gridDimension()

        self.numberOfRowsAndCols()

        self.cellDimension()
        
        # Calcular las coordenadas de cada celda

        cellIndexMap = {}
        position = {}
        lastReferencePointTopLeft = self.grid_top_left
        referencePointRight = self.grid_top_right

        for i in range(self.rows):
            lastReferencePointBottomLeft = gt.newPointAtDistance(lastReferencePointTopLeft, self.grid_bottom_left, self.cell_height)
            topReferenceForNewColumn = lastReferencePointBottomLeft

            for j in range(self.cols):
                # Coordenadas de la celda
                corners = self.cellCoords(lastReferencePointTopLeft, lastReferencePointBottomLeft, referencePointRight)

                lastReferencePointTopLeft = corners[1]
                lastReferencePointBottomLeft = corners[2]

                cellPoly = Polygon([corners[0], corners[1], corners[2], corners[3]])

                demand = self.sumCellDemand(cellPoly, len(self.cellCoordinates))
                
                if demand == 0: continue

                self.addNewCell(cellPoly, demand, corners)

                current_index = len(self.cellCoordinates)

                cellIndexMap[(i, j)] = current_index
                position[current_index] = (i,j)

                self.connectWithUpperCell(cellIndexMap, i, j, current_index)

                self.conncectWithRightCell(cellIndexMap, i, j, current_index)

            lastReferencePointTopLeft = topReferenceForNewColumn
            referencePointRight = gt.newPointAtDistance(referencePointRight, self.grid_bottom_right, self.cell_height) 

        self.numberOfCells = len(self.adjGrid) - 1 # El 0 no lo contamos

        print(str(self.numberOfCells) + " x " + str(self.numberOfCells))

    # Grid attributes

    def extremeCoords(self, coords):
        for (lat, lon) in coords:
            self.max_lat = max(self.max_lat, lat)
            self.min_lat = min(self.min_lat, lat)
            
            self.max_lon = max(self.max_lon, lon)
            self.min_lon = min(self.min_lon, lon)

        #self.grid_top_left      = (self.min_lat, self.min_lon)
        self.grid_top_left      = (self.max_lat, self.max_lon)
        self.grid_top_right     = (self.max_lat, self.min_lon)
        #self.grid_bottom_right  = (self.max_lat, self.max_lon)
        self.grid_bottom_right  = (self.min_lat, self.min_lon)
        self.grid_bottom_left   = (self.min_lat, self.max_lon)

    def rotatePolygon(self, theta):
        currentGrid = [self.grid_top_left, self.grid_top_right, self.grid_bottom_right, self.grid_bottom_left]
        rotated = gt.rotatePolygon(np.array(currentGrid), theta)

        self.grid_top_left      = rotated[0]
        self.grid_top_right     = rotated[1]
        self.grid_bottom_right  = rotated[2]
        self.grid_bottom_left   = rotated[3]

    def gridDimension(self):
        self.height = gt.earthDistance(self.min_lat, self.min_lon, self.max_lat, self.min_lon)
        self.width  = gt.earthDistance(self.min_lat, self.min_lon,self. min_lat, self.max_lon)

    def numberOfRowsAndCols(self):
        self.rows = int(np.floor(np.sqrt(self.maxCells)))
        self.cols = int(np.ceil(self.maxCells / self.rows))

        while self.rows * self.cols < self.maxCells:
            self.rows += 1

    # Cell

    def cellDimension(self):
        # Dividimos por 111000 ya que 1 grado de latitud equivale a 111,000 metros
        self.cell_height =  (self.height / self.rows) / 111000
        self.cell_width  = (self.width / self.cols) / (111000 * np.cos(np.radians((self.max_lat + self.min_lat) / 2)))

    def sumCellDemand(self, cellPoly, cellId):
        demand = 0
        for node, point in self.points.items():
            if cellPoly.contains(point) and self.polyZone.contains(Point(point.y, point.x)):
                self.nodeCellMap[node] = cellId

        countedEdges = []

        for node, cell in self.nodeCellMap.items():
            if cell != cellId: continue

            for neighbour in self.graph.neighbours(node):
                sameCell        = self.nodeCellMap.get(neighbour, -1) == cell
                notProcessed    = not ((node, neighbour) in countedEdges or (neighbour, node) in countedEdges)
                inRequestedZone = gt.polygonContainsLine(self.points[node], self.points[neighbour], self.polyZone)

                if sameCell and notProcessed and inRequestedZone:
                    demand += self.graph.demand[node][neighbour]
                    countedEdges.append((node,neighbour))
            
        return demand

    def conncectWithRightCell(self, cellIndexMap, i, j, current_index):
        if j > 0 and (i,j-1) in cellIndexMap:  
            left_index = cellIndexMap[(i,j-1)] 
            self.adjGrid[current_index].append(left_index)
            self.adjGrid[left_index].append(current_index)  
            self.numberOfEdges += 2

    def connectWithUpperCell(self, cellIndexMap, i, j, current_index):
        if i > 0 and (i-1,j) in cellIndexMap:  
            top_index = cellIndexMap[(i-1,j)] 
            self.adjGrid[current_index].append(top_index)
            self.adjGrid[top_index].append(current_index) 
            self.numberOfEdges += 2

    def addNewCell(self, cellPoly, demand, corners):
        self.cellCoordinates.append(corners)
        self.cellPolygons[len(self.cellCoordinates) - 1] = cellPoly
        self.cellDemand.append(demand)
        self.adjGrid.append([])

    def cellCoords(self, topLeft, bottomLeft, referencePointRight):
        topRight = gt.newPointAtDistance(topLeft, referencePointRight, self.cell_width)    
        bottomRight = gt.newPointFromVectorSum(topLeft, topRight, bottomLeft)
        
        return topLeft, topRight, bottomRight, bottomLeft

    # Cell Distance

    def calculateCellDistances(self):
        n = len(self.cellCoordinates)

        self.cellDist = [[0] * n for _ in range(n)]

        for (i, iCoords) in enumerate(self.cellCoordinates):
            for j in range(i+1, n):
                jCoords = self.cellCoordinates[j]
                
                iLat, iLon = gt.centroidOf(iCoords[0], iCoords[2])
                jLat, jLon = gt.centroidOf(jCoords[0], jCoords[2])
                self.cellDist[i][j] = gt.earthDistance(iLat, iLon, jLat, jLon)
                self.cellDist[j][i] = self.cellDist[i][j]
                self.maxCellDist = max(self.maxCellDist, self.cellDist[i][j])
