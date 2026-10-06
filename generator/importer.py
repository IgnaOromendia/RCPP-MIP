"""Lectura de recursos geográficos y artefactos del generador OSM."""

import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from shapely.geometry import Polygon

from graph import Graph
from paths import GeneratorPaths, validate_map_name


class Importer:
    def __init__(self, mapName="", paths=None):
        self.mapName = validate_map_name(mapName) if mapName else ""
        self.paths = paths or GeneratorPaths.for_repository()

        # Compatibilidad temporal con consumidores que todavía abren esta ruta.
        self.inputsPath = str(self.paths.input_dir) + "/"

    def importTrainCrosses(self):
        return self.importPointsFromKML("cruces-ffc")

    def importLeftTurnSemaphores(self):
        return self.importPointsFromKML("Semaforos-giro-izq")

    def importSemaphores(self):
        return self.importPointsFromKML("Semaforos")

    def importPointsFromKML(self, fileName):
        root, namespace = self._read_kml(self.paths.resource(fileName + ".kml"))
        points = []
        for placemark in root.findall(".//kml:Placemark", namespace):
            coordinates = placemark.find(".//kml:Point/kml:coordinates", namespace)
            if coordinates is None or not coordinates.text:
                continue
            values = coordinates.text.strip().split(",")
            if len(values) < 2:
                raise ValueError(f"Coordenada KML inválida en {fileName}.kml.")
            points.append([float(values[1]), float(values[0])])
        return np.asarray(points, dtype=float).reshape((-1, 2))

    def importRestrictedAreas(self, fileName):
        root, namespace = self._read_kml(self.paths.resource(fileName))
        areas = []
        for polygon in root.findall(".//kml:Polygon", namespace):
            coordinates = polygon.find(".//kml:coordinates", namespace)
            if coordinates is None or not coordinates.text:
                continue
            points = [tuple(map(float, value.split(",")[:2]))
                      for value in coordinates.text.strip().split()]
            if len(points) >= 3:
                areas.append(Polygon(points))
        return areas

    def importcobblestoneStreets(self):
        filename = "empedrados.kml"
        root, namespace = self._read_kml(self.paths.resource(filename))
        sources = []
        targets = []
        for placemark in root.findall(".//kml:Placemark", namespace):
            lines = placemark.findall(".//kml:LineString", namespace)
            if len(lines) != 1:
                raise ValueError(
                    f"Cada Placemark de {filename} debe tener un LineString."
                )
            coordinates = lines[0].find(".//kml:coordinates", namespace)
            if coordinates is None or not coordinates.text:
                continue
            values = coordinates.text.strip().split()
            for index in range(len(values) - 1):
                sources.append(list(map(float, values[index].split(",")[:2][::-1])))
                targets.append(list(map(float, values[index + 1].split(",")[:2][::-1])))
        return (np.asarray(sources, dtype=float).reshape((-1, 2)),
                np.asarray(targets, dtype=float).reshape((-1, 2)))

    def importCellNodeMap(self):
        return self.importNodeCellMap()

    def importGraph(self):
        _, node_count, nearest, edges, arcs = self.importGraphRecords()
        # Graph reserva internamente el índice 0; los nodos viales usan 1..N.
        graph = Graph(node_count + 1)
        for source, target, zone, cost, demand in edges:
            graph.addWeightedEdge(source, target, cost, demand, zone)
        for source, target, zone, cost, demand in arcs:
            graph.addWeightedArc(source, target, cost, demand, zone)
        return graph, nearest

    def importGraphRecords(self):
        path = self.paths.graph(self._required_map_name())
        tokens = self._tokens(path)
        if len(tokens) < 5:
            raise ValueError(f"{path}: cabecera incompleta.")
        try:
            vehicles, nodes, deposit_count, edge_count, arc_count = map(int, tokens[:5])
        except ValueError as error:
            raise ValueError(f"{path}: cabecera inválida.") from error
        if vehicles < 1 or nodes < 1 or min(deposit_count, edge_count, arc_count) < 0:
            raise ValueError(f"{path}: cantidades fuera de rango.")

        expected = 5 + deposit_count + 5 * (edge_count + arc_count)
        if len(tokens) != expected:
            raise ValueError(
                f"{path}: se esperaban {expected} valores y se encontraron {len(tokens)}."
            )
        cursor = 5
        try:
            nearest = [int(value) for value in tokens[cursor:cursor + deposit_count]]
            cursor += deposit_count
            records = []
            for _ in range(edge_count + arc_count):
                source = int(tokens[cursor])
                target = int(tokens[cursor + 1])
                zone = int(tokens[cursor + 2])
                cost = float(tokens[cursor + 3])
                demand = float(tokens[cursor + 4])
                records.append((source, target, zone, cost, demand))
                cursor += 5
        except ValueError as error:
            raise ValueError(f"{path}: registro inválido.") from error

        for node in nearest:
            self._validate_node(node, nodes, path)
        if len(set(nearest)) != len(nearest):
            raise ValueError(f"{path}: nodos del depósito repetidos.")
        for source, target, zone, cost, demand in records:
            self._validate_node(source, nodes, path)
            self._validate_node(target, nodes, path)
            if zone < -1 or zone > vehicles:
                raise ValueError(f"{path}: zona {zone} fuera de -1..{vehicles}.")
            if not math.isfinite(cost) or cost <= 0:
                raise ValueError(f"{path}: costo no positivo o no finito.")
            if not math.isfinite(demand) or demand < 0:
                raise ValueError(f"{path}: demanda negativa o no finita.")
        return (vehicles, nodes, nearest, records[:edge_count],
                records[edge_count:])

    def importCellZones(self):
        path = self.paths.zonification(self._required_map_name())
        tokens = self._tokens(path)
        if not tokens:
            raise ValueError(f"{path}: archivo vacío.")
        try:
            cell_count = int(tokens[0])
        except ValueError as error:
            raise ValueError(f"{path}: cantidad de celdas inválida.") from error
        if cell_count < 1 or len(tokens) != 1 + 2 * cell_count:
            raise ValueError(f"{path}: zonificación incompleta o con datos adicionales.")
        zones = [None] * cell_count
        for index in range(cell_count):
            try:
                cell = int(tokens[1 + 2 * index])
                zone = int(tokens[2 + 2 * index])
            except ValueError as error:
                raise ValueError(f"{path}: zonificación inválida.") from error
            if cell < 0 or cell >= cell_count or zones[cell] is not None or zone < 0:
                raise ValueError(f"{path}: celda o zona fuera de rango.")
            zones[cell] = zone + 1
        if any(zone is None for zone in zones):
            raise ValueError(f"{path}: faltan celdas en la zonificación.")
        return zones

    def importNodeIDMap(self):
        path = self.paths.node_mapping(self._required_map_name())
        osm_to_node = {}
        node_to_osm = {}
        for line in self._lines(path):
            data = line.split()
            if len(data) != 2:
                raise ValueError(f"{path}: fila de mapeo inválida.")
            osm_to_node[data[0]] = int(data[1])
            node_to_osm[int(data[1])] = data[0]
        return osm_to_node, node_to_osm

    def importNodeCoordinates(self):
        path = self.paths.node_coordinates(self._required_map_name())
        coordinates = {}
        for line in self._lines(path):
            data = line.split()
            if len(data) != 3:
                raise ValueError(f"{path}: fila de coordenadas inválida.")
            coordinates[data[0]] = (float(data[1]), float(data[2]))
        return coordinates

    def importCellCoordinates(self):
        path = self.paths.grid_coordinates(self._required_map_name())
        result = []
        for line in self._lines(path):
            parts = line.split()
            if len(parts) != 5:
                raise ValueError(f"{path}: celda sin cuatro esquinas.")
            result.append(tuple(
                (float(part.split(",")[0]), float(part.split(",")[1]))
                for part in parts[1:]
            ))
        return result

    def importCellZone(self):
        return self.importCellZones()

    def importNodeCellMap(self):
        path = self.paths.cell_mapping(self._required_map_name())
        result = {}
        for line in self._lines(path):
            data = line.split()
            if len(data) != 2:
                raise ValueError(f"{path}: fila de mapeo inválida.")
            result[int(data[0])] = int(data[1])
        return result

    def importIllegalCurves(self):
        path = self.paths.turns(self._required_map_name())
        tokens = self._tokens(path)
        if len(tokens) < 2:
            raise ValueError(f"{path}: cabecera incompleta.")
        try:
            turn_count, illegal_count = int(tokens[0]), int(tokens[1])
        except ValueError as error:
            raise ValueError(f"{path}: cabecera inválida.") from error
        if min(turn_count, illegal_count) < 0 \
                or len(tokens) != 2 + 3 * (turn_count + illegal_count):
            raise ValueError(f"{path}: cantidades de giros inválidas.")
        start = 2 + 3 * turn_count
        return [tuple(map(int, tokens[index:index + 3]))
                for index in range(start, len(tokens), 3)]

    def importGeoJSON(self, geoJSONFile):
        path = self.paths.polygon(geoJSONFile)
        with self._open(path) as source:
            contents = json.load(source)
        if not isinstance(contents, dict):
            raise ValueError(f"{path}: el contenido GeoJSON debe ser un objeto.")
        features = contents.get("features")
        if contents.get("type") != "FeatureCollection" or not features \
                or not isinstance(features[0], dict) \
                or not isinstance(features[0].get("geometry"), dict):
            raise ValueError(
                f"{path}: se esperaba un FeatureCollection con al menos una geometría."
            )
        return contents

    def _required_map_name(self):
        if not self.mapName:
            raise ValueError("Esta operación requiere un nombre de mapa.")
        return self.mapName

    @staticmethod
    def _validate_node(node, node_count, path):
        if node < 1 or node > node_count:
            raise ValueError(f"{path}: nodo {node} fuera de 1..{node_count}.")

    @staticmethod
    def _open(path):
        try:
            return Path(path).open("r", encoding="utf-8")
        except OSError as error:
            raise OSError(f"No se pudo abrir {path}: {error.strerror}.") from error

    @classmethod
    def _lines(cls, path):
        with cls._open(path) as source:
            return [line.strip() for line in source if line.strip()]

    @classmethod
    def _tokens(cls, path):
        with cls._open(path) as source:
            return source.read().split()

    @classmethod
    def _read_kml(cls, path):
        with cls._open(path) as source:
            try:
                root = ET.fromstring(source.read())
            except ET.ParseError as error:
                raise ValueError(f"{path}: KML inválido: {error}.") from error
        namespace_uri = root.tag.split("}")[0].strip("{") if "}" in root.tag else ""
        if not namespace_uri:
            raise ValueError(f"{path}: KML sin namespace.")
        return root, {"kml": namespace_uri}
