from graph import Graph
from osmGraph import OsmGraph
from grid import Grid
from exporter import Exporter
from importer import Importer
from paths import GeneratorPaths

class Generator:
    def __init__(self, cells=0, paths=None):
        self.paths = paths or GeneratorPaths.for_repository()
        # OSM 
        self.osmGraph = None

        # Graph
        self.graph = Graph()

        # Grid
        self.grid = None
  
        # Zones
        self.zone = []
        self.mixZone = []
        self.amountOfCells = cells

        # Amount of Trucks
        self.amountOfTrucks = 2

        # Truck speed m/seg
        self.truckSpeed = 5.55

        # Demand (2 bolsas por cuadra)
        self.stretDemand = 2

        # Deposit location
        self.depositCoords = (-34.5196029, -58.5550397)
        
    def generateFrom2GeoJSON(self, totalZoneJSON, geoJSONData, gridReferenceStreet,
                             mapName, generateGrid=True):
        if generateGrid and (
                not isinstance(self.amountOfCells, int)
                or isinstance(self.amountOfCells, bool)
                or self.amountOfCells < 1):
            raise ValueError("La cantidad de celdas debe ser un entero positivo.")
        self.osmGraph = OsmGraph(
            self.graph, geoJSONData, self.depositCoords, paths=self.paths
        )
        self.grid = None

        self.osmGraph.addElementsFrom(totalZoneJSON,mapName)
        self.osmGraph.removeDeadEnds()
        self.osmGraph.reassingAdjListIds()
        self.osmGraph.calculateGraphMetrics(self.truckSpeed, self.stretDemand)
        self.osmGraph.detectNearstNodesToDeposit()
        self.osmGraph.detectCurves()

        # self.graph.printGraphData()
        
        if generateGrid:
            self.grid = Grid(
                self.graph, self.amountOfCells, self.osmGraph.points, geoJSONData
            )
            self.grid.generate(
                self.osmGraph.nodesCoords(),
                self.osmGraph.angleOfStreet(gridReferenceStreet),
            )
            self.grid.calculateCellDistances()
 
    def applyZonificationTo(self, mapName):
        importer = Importer(mapName, self.paths)

        graph, nearstNodesToDeposit = importer.importGraph()
        nodeCellMap = importer.importCellNodeMap()
        cellZones   = importer.importCellZones()

        zone = [0] * graph.n

        # Asignar la zona a cada nodo
        for node, cell in nodeCellMap.items():
            if node < 1 or node >= graph.n:
                raise ValueError(f"Nodo {node} fuera del grafo al aplicar zonas.")
            if cell < 0 or cell >= len(cellZones):
                raise ValueError(f"Celda {cell} fuera de la zonificación.")
            zone[node] = cellZones[cell]

        self.amountOfTrucks = max(zone)
        if self.amountOfTrucks < 1:
            raise ValueError("La zonificación no asignó ningún vehículo.")

        # Exportamos el nuevo grafo
        exporter = Exporter(mapName, self.paths)

        return exporter.exportAsInput(
            graph, nearstNodesToDeposit, self.amountOfTrucks, zone
        )

    # Export
    def export(self, fileName):
        exporter = Exporter(fileName, self.paths)

        written = []
        written.append(exporter.exportToOSMFile(self.osmGraph.osm))
        written.append(exporter.exportAsInput(
            self.graph, self.osmGraph.nearstNodesToDepo, self.amountOfTrucks
        ))

        written.append(exporter.exportCoordinates(self.osmGraph.nodesCoordinates.items()))
        written.append(exporter.exportNodeIDMap(self.osmGraph.nodeOsmToNodeMap.items()))

        if self.grid is not None:
            written.append(exporter.exportGridGraph(self.grid))
            written.extend(exporter.exportGridCoordinates(
                self.grid.cellCoordinates, self.grid.nodeCellMap.items()
            ))
            written.append(exporter.exportCellDistance(
                self.grid.cellDist, self.grid.maxCellDist
            ))
        
        written.append(exporter.exportCurves(
            self.osmGraph.curves, self.osmGraph.illegal_curves,
            node_count=self.graph.n - 1,
        ))
        written.extend(exporter.exportAttributes(self.graph.time, self.graph.demand))
        return written
    
