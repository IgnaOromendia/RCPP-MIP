"""Exportación de instancias y artefactos auxiliares del generador OSM."""

import math
from pathlib import Path
import tempfile
import xml.etree.ElementTree as ET
from xml.dom import minidom

from paths import GeneratorPaths, validate_map_name


def _atomic_write(path, contents):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_name = None
    try:
        with tempfile.NamedTemporaryFile(
                mode="w", encoding="utf-8", dir=path.parent,
                prefix=f".{path.name}.", delete=False) as temporary:
            temporary.write(contents)
            temporary_name = temporary.name
        Path(temporary_name).replace(path)
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return path


class Exporter:
    def __init__(self, fileName, paths=None):
        self.fileName = validate_map_name(fileName)
        self.paths = paths or GeneratorPaths.for_repository()

        # Compatibilidad para el visualizador heredado mientras migra a Path.
        self.inputsPath = str(self.paths.input_dir) + "/"

    def exportToOSMFile(self, osmElement):
        parsed = minidom.parseString(ET.tostring(osmElement, "utf-8"))
        return _atomic_write(
            self.paths.osm(self.fileName), parsed.toprettyxml(indent="   ")
        )

    def exportAsInput(self, graph, nearestNodesToDeposit, amountOfTrucks,
                      zones=None):
        real_nodes = graph.n - 1
        if real_nodes < 1:
            raise ValueError("El grafo debe contener al menos un nodo vial.")
        if not isinstance(amountOfTrucks, int) or isinstance(amountOfTrucks, bool) \
                or amountOfTrucks < 1:
            raise ValueError("La cantidad de vehículos debe ser un entero positivo.")

        nearest = [int(node) for node in nearestNodesToDeposit]
        if len(set(nearest)) != len(nearest):
            raise ValueError("Los nodos adyacentes al depósito no pueden repetirse.")
        for node in nearest:
            self._validate_node(node, real_nodes, "adyacente al depósito")

        if zones is None:
            zones = [-1] * graph.n
        if len(zones) != graph.n:
            raise ValueError("La zonificación no coincide con el tamaño interno del grafo.")
        for zone in zones[1:]:
            if not isinstance(zone, int) or isinstance(zone, bool) \
                    or zone < -1 or zone > amountOfTrucks:
                raise ValueError(
                    f"La zona {zone} está fuera de -1..{amountOfTrucks}."
                )

        edges = list(graph.req_edges) + list(graph.not_req_edges)
        arcs = list(graph.req_arcs) + list(graph.not_req_arcs)
        lines = [
            f"{amountOfTrucks} {real_nodes} {len(nearest)} "
            f"{len(edges)} {len(arcs)}"
        ]
        lines.extend(str(node) for node in nearest)

        for edge in graph.req_edges:
            zone = self._required_zone(edge, zones, amountOfTrucks)
            lines.append(self.edgeToString(edge[0], edge[1], graph, zone))
        for edge in graph.not_req_edges:
            lines.append(self.edgeToString(edge[0], edge[1], graph, 0))
        for arc in graph.req_arcs:
            zone = self._required_zone(arc, zones, amountOfTrucks)
            lines.append(self.edgeToString(arc[0], arc[1], graph, zone))
        for arc in graph.not_req_arcs:
            lines.append(self.edgeToString(arc[0], arc[1], graph, 0))

        return _atomic_write(self.paths.graph(self.fileName), "\n".join(lines) + "\n")

    def exportCoordinates(self, nodesCoordinates):
        lines = [f"{node} {coords[0]} {coords[1]}"
                 for node, coords in nodesCoordinates]
        return _atomic_write(
            self.paths.node_coordinates(self.fileName), "\n".join(lines) + "\n"
        )

    def exportNodeIDMap(self, nodeMap):
        lines = [f"{osmNode} {node}" for osmNode, node in nodeMap]
        return _atomic_write(
            self.paths.node_mapping(self.fileName), "\n".join(lines) + "\n"
        )

    def exportGridGraph(self, grid):
        lines = [f"{grid.numberOfCells} {grid.numberOfEdges}"]
        lines.extend(f"{d:.4f}" for d in grid.cellDemand)
        for node in range(1, grid.numberOfCells + 1):
            lines.extend(f"{node} {neighbour}" for neighbour in grid.adjGrid[node])
        return _atomic_write(
            self.paths.grid_graph(self.fileName), "\n".join(lines) + "\n"
        )

    def exportGridCoordinates(self, cellCoordinates, nodeCellMap):
        coordinate_lines = []
        for index, coordinates in enumerate(cellCoordinates):
            coordinate_lines.append(
                f"{index} " + " ".join(self.coordToString(c) for c in coordinates)
            )
        coordinate_path = _atomic_write(
            self.paths.grid_coordinates(self.fileName),
            "\n".join(coordinate_lines) + "\n",
        )

        map_lines = [f"{node} {cell}" for node, cell in nodeCellMap]
        mapping_path = _atomic_write(
            self.paths.cell_mapping(self.fileName), "\n".join(map_lines) + "\n"
        )
        return coordinate_path, mapping_path

    def exportCellDistance(self, cellDist, maxDist):
        lines = [f"{len(cellDist)} {maxDist}"]
        lines.extend(" ".join(str(value) for value in row) for row in cellDist)
        return _atomic_write(
            self.paths.cell_distances(self.fileName), "\n".join(lines) + "\n"
        )

    def exportCurves(self, curves, illegalCurves, node_count=None):
        curves = list(curves)
        illegalCurves = list(illegalCurves)
        if node_count is not None:
            for context, values in (("giro", curves),
                                    ("giro prohibido", illegalCurves)):
                for turn in values:
                    if len(turn) != 3 or len(set(turn)) != 3:
                        raise ValueError(f"{context} inválido: {turn}.")
                    for node in turn:
                        self._validate_node(node, node_count, context)

        lines = [f"{len(curves)} {len(illegalCurves)}"]
        lines.extend(f"{u} {v} {w}" for u, v, w in curves)
        lines.extend(f"{u} {v} {w}" for u, v, w in illegalCurves)
        return _atomic_write(
            self.paths.turns(self.fileName), "\n".join(lines) + "\n"
        )

    def exportAttributes(self, time, demand):
        return (
            self.exportMatrix(self.paths.time_attributes(self.fileName), time),
            self.exportMatrix(self.paths.demand_attributes(self.fileName), demand),
        )

    @staticmethod
    def coordToString(coord):
        return f"{coord[0]},{coord[1]}"

    def edgeToString(self, src, dst, graph, zone=-1):
        real_nodes = graph.n - 1
        self._validate_node(src, real_nodes, "origen")
        self._validate_node(dst, real_nodes, "destino")
        cost = graph.time[src][dst]
        demand = graph.demand[src][dst]
        if not math.isfinite(cost) or cost <= 0:
            raise ValueError(f"Costo inválido para {src} -> {dst}: {cost}.")
        if not math.isfinite(demand) or demand < 0:
            raise ValueError(f"Demanda inválida para {src} -> {dst}: {demand}.")
        return (f"{src} {dst} {zone} {graph.edgeTimeString(src, dst)} "
                f"{graph.edgeDemandString(src, dst)}")

    @staticmethod
    def exportMatrix(filePath, matrix):
        lines = [str(len(matrix))]
        lines.extend(" ".join(str(value) for value in row) for row in matrix)
        return _atomic_write(filePath, "\n".join(lines) + "\n")

    @staticmethod
    def _validate_node(node, node_count, context):
        if not isinstance(node, int) or isinstance(node, bool) \
                or node < 1 or node > node_count:
            raise ValueError(f"{context}: nodo {node} fuera de 1..{node_count}.")

    @staticmethod
    def _required_zone(edge, zones, vehicle_count):
        source_zone = int(zones[edge[0]])
        target_zone = int(zones[edge[1]])
        zone = source_zone if source_zone == target_zone and source_zone > 0 else -1
        if zone > vehicle_count:
            raise ValueError(
                f"La zona {zone} excede la cantidad de vehículos ({vehicle_count})."
            )
        return zone
