"""Visualización HTML de zonas, tramos y giros prohibidos."""

import argparse
import sys

import folium  # type: ignore

from importer import Importer
from paths import (GeneratorPaths, add_path_arguments, paths_from_arguments,
                   validate_map_name)


class ZonePlotter:
    def __init__(self, paths=None):
        self.paths = paths or GeneratorPaths.for_repository()
        self.nodeCell = {}
        self.nodeOsmToNodeMap = {}
        self.nodeToNodeOsmMap = {}
        self.nodesCoordinates = {}
        self.cellCoords = []
        self.zone = []
        self.adj = [[]]
        self.zones = []
        self.illegalCurves = []
        self.colors = ["gray", "blue", "red", "orange", "purple", "green"]
        self.colors_cell = ["#A9A9A9", "#D3D3D3"]

    def readData(self, mapName):
        importer = Importer(mapName, self.paths)
        self.nodeOsmToNodeMap, self.nodeToNodeOsmMap = importer.importNodeIDMap()
        self.nodesCoordinates = importer.importNodeCoordinates()
        self.cellCoords = importer.importCellCoordinates()
        self.zone = importer.importCellZone()
        self.nodeCell = importer.importNodeCellMap()
        self.illegalCurves = importer.importIllegalCurves()

        _, nodes, _, edges, arcs = importer.importGraphRecords()
        self.adj = [[] for _ in range(nodes + 1)]
        self.zones = [0] * (nodes + 1)
        for source, target, zone, _, _ in edges:
            self.adj[source].append(target)
            self.adj[target].append(source)
            self.zones[source] = zone
            self.zones[target] = zone
        for source, target, zone, _, _ in arcs:
            self.adj[source].append(target)
            self.zones[source] = zone
            self.zones[target] = zone

    def drawGrid(self, zoneMap):
        for index, coords in enumerate(self.cellCoords):
            folium.Polygon(
                locations=coords,
                color="white",
                fill=True,
                fill_color=self.colors_cell[index % len(self.colors_cell)],
                fill_opacity=0.3,
            ).add_to(zoneMap)

    def drawPoints(self, zoneMap):
        for node, coords in self.nodesCoordinates.items():
            internal = self.nodeOsmToNodeMap[node]
            cell = self.nodeCell.get(internal)
            zone = self.zone[cell] if cell is not None else 0
            color = self._color(zone)
            folium.CircleMarker(
                location=coords, radius=1, color=color, fill=True,
                fill_color=color, fill_opacity=0.7,
            ).add_to(zoneMap)

    def drawLines(self, zoneMap):
        for source in range(1, len(self.adj)):
            source_cell = self.nodeCell.get(source)
            for target in self.adj[source]:
                target_cell = self.nodeCell.get(target)
                source_zone = self.zone[source_cell] if source_cell is not None else 0
                target_zone = self.zone[target_cell] if target_cell is not None else 0
                zone = source_zone if source_zone == target_zone and source_zone > 0 else 0
                coordinates = [
                    self.nodesCoordinates[self.nodeToNodeOsmMap[source]],
                    self.nodesCoordinates[self.nodeToNodeOsmMap[target]],
                ]
                folium.PolyLine(
                    locations=coordinates, color=self._color(zone),
                    weight=2, opacity=0.8,
                ).add_to(zoneMap)

    def drawCurves(self, zoneMap):
        for source, intersection, target in self.illegalCurves:
            first = [
                self.nodesCoordinates[self.nodeToNodeOsmMap[source]],
                self.nodesCoordinates[self.nodeToNodeOsmMap[intersection]],
            ]
            second = [
                self.nodesCoordinates[self.nodeToNodeOsmMap[intersection]],
                self.nodesCoordinates[self.nodeToNodeOsmMap[target]],
            ]
            folium.PolyLine(locations=first, color="red", weight=2,
                            opacity=0.8).add_to(zoneMap)
            folium.PolyLine(locations=second, color="red", weight=2,
                            opacity=0.8).add_to(zoneMap)

    def plotPointsZone(self, mapName, showGrid):
        zone_map = self._map()
        if showGrid:
            self.drawGrid(zone_map)
        self.drawPoints(zone_map)
        return self._save(zone_map, self.paths.plot(mapName))

    def plotLineZones(self, mapName, showGrid):
        zone_map = self._map()
        if showGrid:
            self.drawGrid(zone_map)
        self.drawLines(zone_map)
        return self._save(zone_map, self.paths.plot(mapName, "_lines"))

    def plotIllegalCurves(self, mapName):
        zone_map = self._map()
        self.drawCurves(zone_map)
        return self._save(zone_map, self.paths.plot(mapName, "_turns"))

    def _map(self):
        if 1 not in self.nodeToNodeOsmMap:
            raise ValueError("No hay un nodo 1 para centrar el mapa.")
        center = self.nodesCoordinates[self.nodeToNodeOsmMap[1]]
        return folium.Map(location=center, zoom_start=14)

    @staticmethod
    def _save(zone_map, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        zone_map.save(str(path))
        return path

    def _color(self, zone):
        if zone <= 0:
            return self.colors[0]
        return self.colors[1 + (zone - 1) % (len(self.colors) - 1)]


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(description="Genera mapas HTML de una zonificación.")
    parser.add_argument("mapa", help="Nombre de la instancia zonificada.")
    parser.add_argument(
        "mostrar_grilla", type=int, choices=(0, 1),
        help="1 para dibujar la grilla; 0 para ocultarla.",
    )
    add_path_arguments(parser)
    options = parser.parse_args(arguments)
    try:
        validate_map_name(options.mapa)
    except ValueError as error:
        parser.error(str(error))
    return options


def main(arguments=None):
    options = parse_arguments(arguments)
    try:
        plotter = ZonePlotter(paths_from_arguments(options))
        plotter.readData(options.mapa)
        outputs = [
            plotter.plotPointsZone(options.mapa, options.mostrar_grilla),
            plotter.plotLineZones(options.mapa, options.mostrar_grilla),
            plotter.plotIllegalCurves(options.mapa),
        ]
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    print("Mapas generados:")
    for output in outputs:
        print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
