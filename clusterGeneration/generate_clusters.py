#!/usr/bin/env python3
"""Generate graph-edge clusters and optionally apply them to a solver solution."""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
import csv
from dataclasses import dataclass, replace
import heapq
from html import escape
from pathlib import Path
import math
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


SECTION_HEADERS = {
    "---- X ----": "service",
    "---- Y ----": "traversals",
    "---- YDK & YKD ----": "deposit",
    "---- F ----": "ignored",
    "---- FDK ----": "ignored",
}
VARIABLE = re.compile(r"^([XY])_([^_]+)_([^_]+)_([1-9][0-9]*)\s*=\s*([0-9]+)$")
OUTPUT_NAME = re.compile(r"^min_dist_([1-9][0-9]*)\.dat$")
CLUSTER_COLORS = (
    "#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c", "#0891b2",
    "#db2777", "#65a30d", "#4f46e5", "#ca8a04", "#0f766e", "#c026d3",
)
CLUSTER_ABSORPTION_RATIO = 3.0
MAX_RESIDUAL_CLUSTER_EDGES = 14
MAX_DIVIDER_PAIRS = 24
STREET_NAME_CHANGE_WEIGHT = 0.01


@dataclass(frozen=True)
class Edge:
    index: int
    source: str
    target: str
    vehicle: int
    service_count: int
    traversal_count: int

    @property
    def passages(self) -> int:
        return self.service_count + self.traversal_count


@dataclass(frozen=True)
class CoordinateGeometry:
    node_count: int
    coordinates: dict[str, tuple[float, float]]
    original_edges: list[tuple[str, str]]
    virtual_to_original: dict[str, str]
    virtual_to_edge: dict[str, int]
    deposit_adjacent: str
    zones: list[int] | None = None


def node_sort_key(node: str) -> tuple[int, int]:
    return (0, 0) if node == "D" else (1, int(node))


def edge_key(source: str, target: str) -> tuple[str, str]:
    return ((source, target) if node_sort_key(source) <= node_sort_key(target)
            else (target, source))


def read_coordinate_geometry(graph_path: Path,
                             coordinate_path: Path | None = None) -> CoordinateGeometry:
    try:
        tokens = graph_path.read_text(encoding="utf-8").split()
    except OSError as error:
        raise ValueError(f"no se pudo leer {graph_path}: {error}") from error
    if len(tokens) < 5:
        raise ValueError(f"{graph_path}: cabecera incompleta")
    try:
        _, node_count, deposit_count, edge_count, arc_count = map(int, tokens[:5])
    except ValueError as error:
        raise ValueError(f"{graph_path}: cabecera invalida") from error
    if node_count <= 0 or deposit_count <= 0 or edge_count < 0 or arc_count < 0:
        raise ValueError(f"{graph_path}: cantidades invalidas")
    expected_tokens = 5 + deposit_count + 5 * (edge_count + arc_count)
    if len(tokens) != expected_tokens:
        raise ValueError(f"{graph_path}: cantidad de campos invalida")
    try:
        deposit_nodes = [int(token) for token in tokens[5:5 + deposit_count]]
    except ValueError as error:
        raise ValueError(f"{graph_path}: nodo de deposito invalido") from error
    if any(node < 1 or node > node_count for node in deposit_nodes):
        raise ValueError(f"{graph_path}: nodo de deposito fuera de rango")

    original_edges: list[tuple[str, str]] = []
    zones: list[int] = []
    virtual_to_original: dict[str, str] = {}
    virtual_to_edge: dict[str, int] = {}
    offset = 5 + deposit_count
    orientation = 0
    for edge_index in range(edge_count + arc_count):
        record = tokens[offset + 5 * edge_index:offset + 5 * (edge_index + 1)]
        try:
            source, target, zone = int(record[0]), int(record[1]), int(record[2])
        except ValueError as error:
            raise ValueError(f"{graph_path}: registro de arista invalido") from error
        if not (1 <= source <= node_count and 1 <= target <= node_count):
            raise ValueError(f"{graph_path}: extremo de arista fuera de rango")
        original_edges.append((str(source), str(target)))
        zones.append(zone)

        def add_orientation(orientation_source: int, orientation_target: int) -> None:
            nonlocal orientation
            virtual_to_original[str(2 * orientation + 1)] = str(orientation_source)
            virtual_to_original[str(2 * orientation + 2)] = str(orientation_target)
            virtual_to_edge[str(2 * orientation + 1)] = edge_index
            virtual_to_edge[str(2 * orientation + 2)] = edge_index
            orientation += 1

        add_orientation(source, target)
        if edge_index < edge_count:
            add_orientation(target, source)

    coordinates: dict[str, tuple[float, float]] = {}
    if coordinate_path is None:
        return CoordinateGeometry(
            node_count, coordinates, original_edges, virtual_to_original,
            virtual_to_edge, str(deposit_nodes[0]), zones
        )
    try:
        with coordinate_path.open(encoding="utf-8", newline="") as coordinate_file:
            reader = csv.DictReader(coordinate_file)
            if reader.fieldnames != ["node_id", "x", "y"]:
                raise ValueError(f"{coordinate_path}: cabecera invalida")
            for row_number, row in enumerate(reader, start=2):
                try:
                    node = int(row["node_id"])
                    x, y = float(row["x"]), float(row["y"])
                except (TypeError, ValueError) as error:
                    raise ValueError(
                        f"{coordinate_path}: linea {row_number}: coordenada invalida"
                    ) from error
                if node < 1 or node > node_count or not math.isfinite(x) or not math.isfinite(y):
                    raise ValueError(
                        f"{coordinate_path}: linea {row_number}: coordenada fuera de rango"
                    )
                if str(node) in coordinates:
                    raise ValueError(f"{coordinate_path}: nodo duplicado {node}")
                coordinates[str(node)] = (x, y)
    except OSError as error:
        raise ValueError(f"no se pudo leer {coordinate_path}: {error}") from error
    if set(coordinates) != {str(node) for node in range(1, node_count + 1)}:
        raise ValueError(f"{coordinate_path}: faltan coordenadas de nodos")
    return CoordinateGeometry(
        node_count, coordinates, original_edges, virtual_to_original,
        virtual_to_edge, str(deposit_nodes[0]), zones
    )


def read_neutral_nodes(path: Path, node_count: int) -> dict[int, tuple[str, ...]]:
    """Read explicit graph nodes that must act as cluster boundaries."""
    try:
        lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines()
                 if line.strip()]
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if not lines or lines[0] != "node_id reasons":
        raise ValueError(f"{path}: cabecera de nodos neutros invalida")
    result: dict[int, tuple[str, ...]] = {}
    for line_number, line in enumerate(lines[1:], start=2):
        fields = line.split()
        if len(fields) != 2:
            raise ValueError(f"{path}: linea {line_number}: nodo neutro invalido")
        try:
            node = int(fields[0])
        except ValueError as error:
            raise ValueError(
                f"{path}: linea {line_number}: identificador invalido"
            ) from error
        reasons = tuple(sorted(set(fields[1].split(","))))
        if (node < 1 or node > node_count or node in result or not reasons
                or any(not reason for reason in reasons)):
            raise ValueError(f"{path}: linea {line_number}: nodo neutro invalido")
        result[node] = reasons
    return result


def read_osm_street_names(
    osm_path: Path, node_mapping_path: Path, geometry: CoordinateGeometry,
) -> dict[tuple[str, str], str]:
    """Map graph endpoints to normalized OSM street names."""
    try:
        mapping_lines = node_mapping_path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"no se pudo leer {node_mapping_path}: {error}") from error
    osm_to_internal: dict[str, str] = {}
    for line_number, line in enumerate(mapping_lines, start=1):
        fields = line.split()
        if len(fields) != 2 or fields[0] in osm_to_internal:
            raise ValueError(
                f"{node_mapping_path}: linea {line_number}: mapeo invalido"
            )
        osm_to_internal[fields[0]] = fields[1]

    try:
        root = ET.parse(osm_path).getroot()
    except (OSError, ET.ParseError) as error:
        raise ValueError(f"no se pudo leer {osm_path}: {error}") from error

    graph_keys = {edge_key(*endpoints) for endpoints in geometry.original_edges}
    names_by_edge: dict[tuple[str, str], set[str]] = defaultdict(set)
    for way in root.findall("way"):
        references = [node.get("ref") for node in way.findall("nd")]
        if len(references) != 2 or any(reference is None for reference in references):
            continue
        tags = {tag.get("k"): tag.get("v") for tag in way.findall("tag")}
        raw_name = tags.get("name")
        if not raw_name or references[0] not in osm_to_internal \
                or references[1] not in osm_to_internal:
            continue
        key = edge_key(
            osm_to_internal[references[0]], osm_to_internal[references[1]]
        )
        if key in graph_keys:
            names_by_edge[key].add(" ".join(raw_name.split()).casefold())

    result = {
        key: next(iter(names))
        for key, names in names_by_edge.items()
        if len(names) == 1
    }
    if not result:
        raise ValueError(
            f"{osm_path}: no hay nombres de calle compatibles con el grafo"
        )
    return result


def write_neutral_nodes(
    path: Path, neutral_node_data: dict[int, tuple[str, ...]], node_count: int
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["node_id reasons"]
    for node, reasons in sorted(neutral_node_data.items()):
        if node < 1 or node > node_count or not reasons:
            raise ValueError(f"nodo neutro fuera de rango: {node}")
        lines.append(f"{node} {','.join(sorted(set(reasons)))}")
    try:
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def read_solution(path: Path) -> list[Edge]:
    """Read positive X + Y multiplicities, grouped by directed edge and vehicle."""
    counts: dict[tuple[str, str, int], list[int]] = {}
    order: list[tuple[str, str, int]] = []
    seen_variables: set[tuple[str, str, str, int]] = set()
    section: str | None = None

    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error

    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line or line.startswith("OBJ:"):
            continue
        if line in SECTION_HEADERS:
            section = SECTION_HEADERS[line]
            continue
        if section == "ignored":
            continue
        if section not in {"service", "traversals", "deposit"}:
            raise ValueError(f"{path}: linea {line_number}: contenido fuera de una seccion conocida")

        match = VARIABLE.fullmatch(line)
        if match is None:
            raise ValueError(f"{path}: linea {line_number}: variable invalida")
        kind, source, target, raw_vehicle, raw_value = match.groups()
        expected = "X" if section == "service" else "Y"
        if kind != expected:
            raise ValueError(f"{path}: linea {line_number}: variable en una seccion incorrecta")
        for node in (source, target):
            if node != "D" and (not node.isdigit() or int(node) <= 0):
                raise ValueError(f"{path}: linea {line_number}: nodo invalido")
        if section == "deposit" and source != "D" and target != "D":
            raise ValueError(f"{path}: linea {line_number}: arco de deposito sin D")
        if section != "deposit" and (source == "D" or target == "D"):
            raise ValueError(f"{path}: linea {line_number}: arco de deposito en seccion incorrecta")

        key = (source, target, int(raw_vehicle))
        variable_key = (kind, source, target, int(raw_vehicle))
        if variable_key in seen_variables:
            raise ValueError(f"{path}: linea {line_number}: variable duplicada")
        seen_variables.add(variable_key)
        if key not in counts:
            counts[key] = [0, 0]
            order.append(key)
        slot = 0 if kind == "X" else 1
        counts[key][slot] = int(raw_value)

    edges = []
    for key in order:
        service_count, traversal_count = counts[key]
        if service_count + traversal_count <= 0:
            continue
        source, target, vehicle = key
        edges.append(Edge(len(edges) + 1, source, target, vehicle,
                          service_count, traversal_count))
    if not edges:
        raise ValueError(f"{path}: la solucion no contiene aristas con pasadas positivas")
    return edges


def edge_adjacency(edges: list[Edge],
                   neutral_nodes: set[str] | None = None) -> list[set[int]]:
    """Build edge adjacency without crossing an explicit neutral boundary.

    Edges whose two endpoints are neutral form a traversable corridor among
    themselves. Approaches may end at that corridor, but cluster growth does
    not pass from an approach to the corridor or to the opposite approach.
    """
    neutral = neutral_nodes or set()
    incident: dict[str, list[int]] = defaultdict(list)
    corridor = []
    for position, edge in enumerate(edges):
        incident[edge.source].append(position)
        if edge.target != edge.source:
            incident[edge.target].append(position)
        corridor.append(edge.source in neutral and edge.target in neutral)

    adjacency = [set() for _ in edges]
    for node, positions in incident.items():
        eligible = (
            [position for position in positions if corridor[position]]
            if node in neutral else positions
        )
        for left_index, left in enumerate(eligible):
            for right in eligible[left_index + 1:]:
                adjacency[left].add(right)
                adjacency[right].add(left)
    return adjacency


def absorb_imbalanced_clusters(edges: list[Edge], assignments: list[int],
                               size_ratio: float = CLUSTER_ABSORPTION_RATIO,
                               neutral_nodes: set[str] | None = None) -> list[int]:
    """Let an adjacent large cluster absorb one at most a third of its size."""
    merged = list(assignments)
    adjacency = edge_adjacency(edges, neutral_nodes)
    while True:
        sizes: dict[int, int] = defaultdict(int)
        for edge, cluster in zip(edges, merged):
            sizes[cluster] += 1
        adjacent_pairs = set()
        for position, neighbours in enumerate(adjacency):
            for neighbour in neighbours:
                left, right = merged[position], merged[neighbour]
                if left != right:
                    adjacent_pairs.add((min(left, right), max(left, right)))
        candidates: list[tuple[int, int]] = []
        for left, right in adjacent_pairs:
            if sizes[left] >= size_ratio * sizes[right]:
                candidates.append((right, left))
            elif sizes[right] >= size_ratio * sizes[left]:
                candidates.append((left, right))
        if not candidates:
            break

        # Absorb the smallest remainder first. This avoids a large cluster
        # growing prematurely and swallowing several otherwise balanced ones.
        absorbed, absorber = min(
            candidates,
            key=lambda pair: (sizes[pair[0]], -sizes[pair[1]], pair[0], pair[1]),
        )
        merged = [absorber if cluster == absorbed else cluster for cluster in merged]

    compact_ids = {
        cluster: compact
        for compact, cluster in enumerate(sorted(set(merged)), start=1)
    }
    return [compact_ids[cluster] for cluster in merged]


def bfs_clusters(edges: list[Edge], percentage: float,
                 neutral_nodes: set[str] | None = None) -> list[int]:
    """Assign connected BFS clusters, then absorb strongly undersized neighbors."""
    if not math.isfinite(percentage) or percentage <= 0 or percentage > 100:
        raise ValueError("k debe ser un porcentaje mayor que 0 y menor o igual que 100")
    target_size = max(1, math.ceil(len(edges) * percentage / 100.0))
    adjacency = edge_adjacency(edges, neutral_nodes)

    assignments = [0] * len(edges)
    cluster = 0
    for seed in range(len(edges)):
        if assignments[seed] != 0:
            continue
        cluster += 1
        queue: deque[int] = deque()

        def add_edge(position: int) -> None:
            assignments[position] = cluster
            queue.append(position)

        add_edge(seed)
        assigned = 1
        while queue and assigned < target_size:
            current = queue.popleft()
            for position in sorted(adjacency[current]):
                if assignments[position] != 0:
                    continue
                add_edge(position)
                assigned += 1
                if assigned == target_size:
                    break
    return absorb_imbalanced_clusters(
        edges, assignments, neutral_nodes=neutral_nodes
    )


def graph_edges(geometry: CoordinateGeometry) -> list[Edge]:
    return [
        Edge(index, source, target, 1, 0, 0)
        for index, (source, target) in enumerate(geometry.original_edges, start=1)
    ]


def required_graph_edges(geometry: CoordinateGeometry) -> list[Edge]:
    """Return service-zone records; zone 0 roads are optional connectors."""
    edges = graph_edges(geometry)
    if geometry.zones is None:
        return edges
    if len(geometry.zones) != len(edges):
        raise ValueError("las zonas no coinciden con las aristas del grafo")
    required = [edge for edge, zone in zip(edges, geometry.zones) if zone != 0]
    if not required:
        raise ValueError("el grafo no contiene aristas en la zona obligatoria")
    return required


def complete_optional_assignments(
    geometry: CoordinateGeometry,
    required_edges: list[Edge],
    required_assignments: list[int],
) -> tuple[list[Edge], list[int]]:
    """Assign optional roads to the nearest required cluster without shaping it."""
    edges = graph_edges(geometry)
    adjacency = edge_adjacency(edges)
    best: list[tuple[int, int] | None] = [None] * len(edges)
    pending: list[tuple[int, int, int]] = []
    for edge, cluster in zip(required_edges, required_assignments):
        position = edge.index - 1
        best[position] = (0, cluster)
        heapq.heappush(pending, (0, cluster, position))

    while pending:
        distance, cluster, position = heapq.heappop(pending)
        if best[position] != (distance, cluster):
            continue
        for neighbour in adjacency[position]:
            candidate = (distance + 1, cluster)
            if best[neighbour] is not None and best[neighbour] <= candidate:
                continue
            best[neighbour] = candidate
            heapq.heappush(pending, (*candidate, neighbour))

    missing = [position + 1 for position, value in enumerate(best) if value is None]
    if missing:
        raise ValueError(
            f"la arista opcional {missing[0]} no esta conectada a la zona obligatoria"
        )
    return edges, [value[1] for value in best if value is not None]


def _road_distance(
    coordinates: dict[str, tuple[float, float]], source: str, target: str
) -> float:
    source_x, source_y = coordinates[source]
    target_x, target_y = coordinates[target]
    latitude = math.radians((source_x + target_x) / 2.0)
    return max(
        math.hypot(
            target_x - source_x,
            (target_y - source_y) * math.cos(latitude),
        ),
        1e-12,
    )


def _turn_angle(
    coordinates: dict[str, tuple[float, float]],
    previous: str, current: str, following: str,
) -> float:
    previous_x, previous_y = coordinates[previous]
    current_x, current_y = coordinates[current]
    following_x, following_y = coordinates[following]
    first = (current_x - previous_x, current_y - previous_y)
    second = (following_x - current_x, following_y - current_y)
    first_length, second_length = math.hypot(*first), math.hypot(*second)
    if first_length < 1e-12 or second_length < 1e-12:
        return 0.0
    cosine = max(-1.0, min(1.0, (
        first[0] * second[0] + first[1] * second[1]
    ) / (first_length * second_length)))
    return math.acos(cosine)


def _local_components(
    edges: list[Edge], neutral_nodes: set[str],
    positions: set[int] | None = None,
) -> list[set[int]]:
    """Return non-boundary edge components without crossing neutral nodes."""
    selected = set(range(len(edges))) if positions is None else set(positions)
    active = {
        position for position in selected
        if not (edges[position].source in neutral_nodes
                and edges[position].target in neutral_nodes)
    }
    incident: dict[str, list[int]] = defaultdict(list)
    for position in active:
        edge = edges[position]
        if edge.source not in neutral_nodes:
            incident[edge.source].append(position)
        if edge.target not in neutral_nodes and edge.target != edge.source:
            incident[edge.target].append(position)
    adjacency = {position: set() for position in active}
    for neighbours in incident.values():
        for left_index, left in enumerate(neighbours):
            for right in neighbours[left_index + 1:]:
                adjacency[left].add(right)
                adjacency[right].add(left)

    components = []
    unseen = set(active)
    while unseen:
        start = min(unseen)
        component = {start}
        pending = deque([start])
        unseen.remove(start)
        while pending:
            current = pending.popleft()
            for neighbour in adjacency[current]:
                if neighbour in unseen:
                    unseen.remove(neighbour)
                    component.add(neighbour)
                    pending.append(neighbour)
        components.append(component)
    return components


def _absorb_regions_across_neutral_streets(
    edges: list[Edge], components: list[set[int]], neutral_nodes: set[str],
    maximum_residual_size: int = MAX_RESIDUAL_CLUSTER_EDGES,
) -> list[set[int]]:
    """Consolidate sub-15-edge remnants without using a large cluster as source."""
    if not components:
        return []

    cluster_by_position = {
        position: cluster
        for cluster, component in enumerate(components, start=1)
        for position in component
    }
    clusters_by_neutral_node: dict[str, set[int]] = defaultdict(set)
    for position, cluster in cluster_by_position.items():
        edge = edges[position]
        for node in (edge.source, edge.target):
            if node in neutral_nodes:
                clusters_by_neutral_node[node].add(cluster)

    adjacent_pairs: set[tuple[int, int]] = set()

    def connect(clusters: set[int]) -> None:
        ordered = sorted(clusters)
        for left_index, left in enumerate(ordered):
            for right in ordered[left_index + 1:]:
                adjacent_pairs.add((left, right))

    for clusters in clusters_by_neutral_node.values():
        connect(clusters)

    neutral_adjacency: dict[str, set[str]] = defaultdict(set)
    for edge in edges:
        if edge.source in neutral_nodes and edge.target in neutral_nodes:
            neutral_adjacency[edge.source].add(edge.target)
            neutral_adjacency[edge.target].add(edge.source)
    unseen_neutral = set(neutral_adjacency)
    while unseen_neutral:
        start = min(unseen_neutral, key=node_sort_key)
        corridor_nodes = {start}
        pending_nodes = deque([start])
        unseen_neutral.remove(start)
        while pending_nodes:
            current = pending_nodes.popleft()
            for neighbour in neutral_adjacency[current]:
                if neighbour in unseen_neutral:
                    unseen_neutral.remove(neighbour)
                    corridor_nodes.add(neighbour)
                    pending_nodes.append(neighbour)
        connect({
            cluster
            for node in corridor_nodes
            for cluster in clusters_by_neutral_node[node]
        })

    parent = {cluster: cluster for cluster in range(1, len(components) + 1)}

    def representative(cluster: int) -> int:
        while parent[cluster] != cluster:
            parent[cluster] = parent[parent[cluster]]
            cluster = parent[cluster]
        return cluster

    while True:
        sizes: dict[int, int] = defaultdict(int)
        for cluster, component in enumerate(components, start=1):
            sizes[representative(cluster)] += len(component)
        neighbours: dict[int, set[int]] = defaultdict(set)
        for original_left, original_right in adjacent_pairs:
            left = representative(original_left)
            right = representative(original_right)
            if left != right:
                neighbours[left].add(right)
                neighbours[right].add(left)

        absorbers_by_residual = {
            cluster: {
                neighbour for neighbour in neighbours[cluster]
                if sizes[neighbour] > size
            }
            for cluster, size in sizes.items()
            if size <= maximum_residual_size
        }
        residuals = [
            cluster for cluster, absorbers in absorbers_by_residual.items()
            if absorbers
        ]
        if not residuals:
            break
        absorbed = min(residuals, key=lambda item: (sizes[item], item))
        absorber = min(
            absorbers_by_residual[absorbed],
            key=lambda item: (-sizes[item], item),
        )
        parent[absorbed] = absorber

    merged: dict[int, set[int]] = defaultdict(set)
    for cluster, component in enumerate(components, start=1):
        merged[representative(cluster)].update(component)
    return [merged[cluster] for cluster in sorted(merged)]


def _straightest_path(
    edges: list[Edge], positions: set[int], source: str, target: str,
    neutral_nodes: set[str], coordinates: dict[str, tuple[float, float]],
    street_names: dict[tuple[str, str], str] | None = None,
) -> list[str] | None:
    node_adjacency: dict[str, set[str]] = defaultdict(set)
    for position in positions:
        edge = edges[position]
        node_adjacency[edge.source].add(edge.target)
        node_adjacency[edge.target].add(edge.source)

    start = (None, source)
    names = street_names or {}
    distances = {start: (0.0, 0)}
    predecessors: dict[tuple[str | None, str], tuple[str | None, str]] = {}
    pending = [(0.0, 0, 0, 0, None, source)]
    counter = 0
    final_state = None
    while pending:
        cost, name_discontinuity, hops, _, previous, current = heapq.heappop(pending)
        state = (previous, current)
        if (cost, name_discontinuity) != distances.get(state):
            continue
        if current == target:
            final_state = state
            break
        if current in neutral_nodes and current != source:
            continue
        for following in sorted(node_adjacency[current], key=node_sort_key):
            if following == previous:
                continue
            if following in neutral_nodes and following != target:
                continue
            length = _road_distance(coordinates, current, following)
            bend = (
                _turn_angle(coordinates, previous, current, following)
                if previous is not None else 0.0
            )
            current_name = (
                names.get(edge_key(previous, current))
                if previous is not None else None
            )
            following_name = names.get(edge_key(current, following))
            if current_name == following_name:
                name_change = 0
            elif current_name is None and following_name is None:
                name_change = 0
            elif current_name is None or following_name is None:
                name_change = 1
            else:
                name_change = 2
            new_cost = cost + length * (
                1.0 + 2.0 * (bend / math.pi) ** 2
                + STREET_NAME_CHANGE_WEIGHT * name_change
            )
            new_distance = (new_cost, name_discontinuity + name_change)
            next_state = (current, following)
            if new_distance >= distances.get(
                    next_state, (float("inf"), sys.maxsize)):
                continue
            distances[next_state] = new_distance
            predecessors[next_state] = state
            counter += 1
            heapq.heappush(
                pending, (*new_distance, hops + 1, counter, current, following)
            )
    if final_state is None:
        return None
    path = [final_state[1]]
    state = final_state
    while state != start:
        state = predecessors[state]
        path.append(state[1])
    path.reverse()
    return path


def _path_straightness(
    path: list[str], coordinates: dict[str, tuple[float, float]]
) -> float:
    if len(path) < 2:
        return float("inf")
    direct = _road_distance(coordinates, path[0], path[-1])
    travelled = sum(
        _road_distance(coordinates, source, target)
        for source, target in zip(path, path[1:])
    )
    bends = sum(
        _turn_angle(coordinates, previous, current, following)
        for previous, current, following in zip(path, path[1:], path[2:])
    )
    return travelled / direct + bends / max(1, len(path) - 2)


def _divider_side_balance(
    path: list[str], edges: list[Edge], parts: list[set[int]],
    coordinates: dict[str, tuple[float, float]],
) -> float | None:
    source_x, source_y = coordinates[path[0]]
    target_x, target_y = coordinates[path[-1]]
    direction_x, direction_y = target_x - source_x, target_y - source_y
    negative = positive = on_axis = 0
    for position in {item for part in parts for item in part}:
        edge = edges[position]
        first_x, first_y = coordinates[edge.source]
        second_x, second_y = coordinates[edge.target]
        midpoint_x = (first_x + second_x) / 2.0
        midpoint_y = (first_y + second_y) / 2.0
        cross = (
            direction_x * (midpoint_y - source_y)
            - direction_y * (midpoint_x - source_x)
        )
        if cross < -1e-12:
            negative += 1
        elif cross > 1e-12:
            positive += 1
        else:
            on_axis += 1
    while on_axis:
        if negative <= positive:
            negative += 1
        else:
            positive += 1
        on_axis -= 1
    if negative == 0 or positive == 0:
        return None
    return abs(positive - negative) / (positive + negative)


def _best_straight_divider(
    edges: list[Edge], component: set[int], neutral_nodes: set[str],
    coordinates: dict[str, tuple[float, float]],
    street_names: dict[tuple[str, str], str] | None = None,
) -> tuple[list[str], list[set[int]]] | None:
    component_nodes = {
        node for position in component
        for node in (edges[position].source, edges[position].target)
    }
    terminals = sorted(component_nodes & neutral_nodes, key=node_sort_key)
    if len(terminals) < 2:
        return None
    pairs = sorted(
        (
            (-_road_distance(coordinates, source, target), source, target)
            for index, source in enumerate(terminals)
            for target in terminals[index + 1:]
        ),
        key=lambda item: (item[0], node_sort_key(item[1]), node_sort_key(item[2])),
    )[:MAX_DIVIDER_PAIRS]

    best = None
    for _, source, target in pairs:
        path = _straightest_path(
            edges, component, source, target, neutral_nodes, coordinates,
            street_names,
        )
        if path is None or len(path) < 3:
            continue
        candidate_neutral = neutral_nodes | set(path)
        parts = _local_components(edges, candidate_neutral, component)
        if len(parts) < 2 or not all(parts):
            continue
        balance = _divider_side_balance(path, edges, parts, coordinates)
        if balance is None:
            continue
        score = (
            balance,
            _path_straightness(path, coordinates),
            len(parts),
            len(path),
            node_sort_key(source),
            node_sort_key(target),
        )
        if best is None or score < best[0]:
            best = (score, path, parts)
    if best is None:
        return None
    return best[1], best[2]


def straight_divider_clusters(
    geometry: CoordinateGeometry, percentage: float,
    neutral_node_data: dict[int, tuple[str, ...]],
    street_names: dict[tuple[str, str], str] | None = None,
) -> tuple[list[Edge], list[int], int, dict[int, tuple[str, ...]], int]:
    """Recursively bisect OSM zones with straight neutral-to-neutral streets."""
    if not math.isfinite(percentage) or percentage <= 0 or percentage > 100:
        raise ValueError("k debe ser un porcentaje mayor que 0 y menor o igual que 100")
    if not geometry.coordinates:
        raise ValueError("la estrategia straight-dividers requiere --coords")
    edges = required_graph_edges(geometry)
    if not edges:
        raise ValueError("el grafo no contiene aristas ni arcos")
    required_nodes = {
        int(node) for edge in edges for node in (edge.source, edge.target)
    }
    effective_reasons = {
        node: set(reasons) for node, reasons in neutral_node_data.items()
        if node in required_nodes
    }
    neutral = {str(node) for node in effective_reasons}
    target_size = max(1, math.ceil(len(edges) * percentage / 100.0))
    components = _local_components(edges, neutral)
    unsplittable: set[frozenset[int]] = set()
    divider_count = 0

    while True:
        candidates = sorted(
            (component for component in components
             if len(component) > target_size
             and frozenset(component) not in unsplittable),
            key=lambda component: (-len(component), min(component)),
        )
        if not candidates:
            break
        component = candidates[0]
        divider = _best_straight_divider(
            edges, component, neutral, geometry.coordinates, street_names
        )
        if divider is None:
            unsplittable.add(frozenset(component))
            continue
        path, parts = divider
        divider_count += 1
        for node in path:
            numeric_node = int(node)
            effective_reasons.setdefault(numeric_node, set()).add("divider")
        neutral.update(path)
        components.remove(component)
        components.extend(parts)

    components.sort(key=lambda component: min(component))
    # A neutral street remains a partition boundary, except for residual
    # regions of fewer than fifteen edges.  Small neighbours may consolidate with
    # each other, but a merged region stops being a source as soon as it reaches
    # that limit, preventing the old large-cluster cascade.
    components = _absorb_regions_across_neutral_streets(
        edges, components, neutral
    )
    assignments = [0] * len(edges)
    for cluster, component in enumerate(components, start=1):
        for position in component:
            assignments[position] = cluster

    incident: dict[str, list[int]] = defaultdict(list)
    for position, edge in enumerate(edges):
        incident[edge.source].append(position)
        incident[edge.target].append(position)
    pending = {position for position, cluster in enumerate(assignments) if cluster == 0}
    while pending:
        progressed = False
        for position in sorted(pending):
            edge = edges[position]
            neighbouring_clusters = {
                assignments[neighbour]
                for node in (edge.source, edge.target)
                for neighbour in incident[node]
                if assignments[neighbour] > 0
            }
            if not neighbouring_clusters:
                continue
            assignments[position] = min(neighbouring_clusters)
            pending.remove(position)
            progressed = True
            break
        if not progressed:
            fallback = 1 if components else 1
            for position in pending:
                assignments[position] = fallback
            pending.clear()

    normalized_reasons = {
        node: tuple(sorted(reasons))
        for node, reasons in effective_reasons.items()
    }
    # Neutral-to-neutral corridor records must receive a positive cluster for
    # the file format, but they are boundaries rather than cluster contents.
    # Report the largest actual region, not that bookkeeping assignment.
    maximum_region_size = max((len(component) for component in components), default=0)
    all_edges, all_assignments = complete_optional_assignments(
        geometry, edges, assignments
    )
    return (all_edges, all_assignments, maximum_region_size, normalized_reasons,
            divider_count)


def cluster_original_graph(edges: list[Edge], percentage: float,
                           geometry: CoordinateGeometry) -> tuple[list[int], int, int]:
    """Cluster distinct original edges, then expand their cluster to solution arcs."""
    active_original_ids: set[int] = set()
    endpoints_by_solution_edge: list[tuple[int | None, int | None]] = []
    for edge in edges:
        try:
            source_id = None if edge.source == "D" else geometry.virtual_to_edge[edge.source]
            target_id = None if edge.target == "D" else geometry.virtual_to_edge[edge.target]
        except KeyError as error:
            raise ValueError(f"nodo virtual inexistente en el grafo: {error.args[0]}") from error
        endpoints_by_solution_edge.append((source_id, target_id))
        if source_id is not None:
            active_original_ids.add(source_id)
        if target_id is not None:
            active_original_ids.add(target_id)

    if not active_original_ids:
        raise ValueError("la solucion no recorre aristas del grafo original")

    original_ids = sorted(active_original_ids)
    original_edges = [
        Edge(position + 1, *geometry.original_edges[edge_id], 0, 0, 0)
        for position, edge_id in enumerate(original_ids)
    ]
    original_assignments = bfs_clusters(original_edges, percentage)
    cluster_by_original_id = dict(zip(original_ids, original_assignments))

    expanded_assignments: list[int] = []
    for source_id, target_id in endpoints_by_solution_edge:
        # A road super-arc has the same original edge at both endpoints. A turn
        # connector joins two originals and belongs to the one entered next;
        # an arrival at the deposit instead inherits the preceding original.
        inherited_id = target_id if target_id is not None else source_id
        if inherited_id is None:
            raise ValueError("arista virtual sin arista original adyacente")
        expanded_assignments.append(cluster_by_original_id[inherited_id])
    original_cluster_sizes: dict[int, int] = defaultdict(int)
    for cluster in original_assignments:
        original_cluster_sizes[cluster] += 1
    return expanded_assignments, len(original_edges), max(original_cluster_sizes.values())


def read_graph_clusters(path: Path, geometry: CoordinateGeometry
                        ) -> tuple[dict[int, int], float | None, int]:
    """Read the pre-solver partition, indexed by original graph record."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error

    percentage: float | None = None
    rows: list[tuple[int, int, str, str]] = []
    header_found = False
    expected_header = (
        "arista cluster origen destino vehiculo servicio recorridos pasadas"
    )
    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("#"):
            fields = line[1:].split()
            if len(fields) == 2 and fields[0] == "porcentaje_objetivo":
                try:
                    percentage = float(fields[1])
                except ValueError as error:
                    raise ValueError(
                        f"{path}: linea {line_number}: porcentaje invalido"
                    ) from error
                if (not math.isfinite(percentage) or percentage <= 0 or
                        percentage > 100):
                    raise ValueError(
                        f"{path}: linea {line_number}: porcentaje fuera de rango"
                    )
            continue
        if not header_found:
            if line != expected_header:
                raise ValueError(f"{path}: linea {line_number}: cabecera invalida")
            header_found = True
            continue
        fields = line.split()
        if len(fields) != 8:
            raise ValueError(f"{path}: linea {line_number}: fila invalida")
        try:
            edge_id, cluster, vehicle, service, traversals, passages = (
                int(fields[index]) for index in (0, 1, 4, 5, 6, 7)
            )
        except ValueError as error:
            raise ValueError(f"{path}: linea {line_number}: fila invalida") from error
        if (edge_id <= 0 or cluster <= 0 or vehicle != 1 or service != 0 or
                traversals != 0 or passages != 0):
            raise ValueError(
                f"{path}: linea {line_number}: cluster pre-solver invalido"
            )
        rows.append((edge_id, cluster, fields[2], fields[3]))

    if not header_found:
        raise ValueError(f"{path}: falta la cabecera")
    expected_ids = set(range(1, len(geometry.original_edges) + 1))
    actual_ids = {edge_id for edge_id, _cluster, _source, _target in rows}
    if len(rows) != len(actual_ids) or actual_ids != expected_ids:
        raise ValueError(
            f"{path}: debe asignar exactamente una vez cada arista del grafo"
        )
    assignments: dict[int, int] = {}
    for edge_id, cluster, source, target in rows:
        expected_source, expected_target = geometry.original_edges[edge_id - 1]
        if (source, target) != (expected_source, expected_target):
            raise ValueError(
                f"{path}: arista {edge_id}: extremos no coinciden con el grafo"
            )
        assignments[edge_id - 1] = cluster
    sizes: dict[int, int] = defaultdict(int)
    for cluster in assignments.values():
        sizes[cluster] += 1
    return assignments, percentage, max(sizes.values())


def project_graph_clusters(edges: list[Edge], geometry: CoordinateGeometry,
                           cluster_by_original_id: dict[int, int]) -> list[int]:
    """Project one existing graph partition onto every active super-arc."""
    assignments: list[int] = []
    for edge in edges:
        try:
            source_id = (None if edge.source == "D"
                         else geometry.virtual_to_edge[edge.source])
            target_id = (None if edge.target == "D"
                         else geometry.virtual_to_edge[edge.target])
        except KeyError as error:
            raise ValueError(
                f"nodo virtual inexistente en el grafo: {error.args[0]}"
            ) from error
        inherited_id = target_id if target_id is not None else source_id
        if inherited_id is None:
            raise ValueError("arista virtual sin arista original adyacente")
        assignments.append(cluster_by_original_id[inherited_id])
    return assignments


def cluster_graph(geometry: CoordinateGeometry, percentage: float,
                  neutral_nodes: set[str] | None = None
                  ) -> tuple[list[Edge], list[int], int]:
    """Cluster required records, then attach optional roads without shaping them."""
    edges = required_graph_edges(geometry)
    if not edges:
        raise ValueError("el grafo no contiene aristas ni arcos")
    required_nodes = {node for edge in edges for node in (edge.source, edge.target)}
    effective_neutral = (neutral_nodes or set()) & required_nodes
    assignments = bfs_clusters(edges, percentage, effective_neutral)
    sizes: dict[int, int] = defaultdict(int)
    for cluster in assignments:
        sizes[cluster] += 1
    all_edges, all_assignments = complete_optional_assignments(
        geometry, edges, assignments
    )
    return all_edges, all_assignments, max(sizes.values())


def write_clusters(path: Path, edges: list[Edge], assignments: list[int], percentage: float,
                   clustered_edge_count: int | None = None,
                   maximum_cluster_size: int | None = None,
                   neutral_node_count: int = 0, strategy: str = "bfs",
                   divider_count: int = 0) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    clustered_edge_count = len(edges) if clustered_edge_count is None else clustered_edge_count
    target_size = max(1, math.ceil(clustered_edge_count * percentage / 100.0))
    maximum_cluster_size = (
        target_size if maximum_cluster_size is None else maximum_cluster_size
    )
    try:
        with path.open("w", encoding="utf-8") as output:
            output.write(f"# porcentaje_objetivo {percentage:g}\n")
            # Keep the legacy count for readers that consume this comment.
            output.write(f"# aristas_activas {len(edges)}\n")
            output.write(f"# aristas_originales_activas {clustered_edge_count}\n")
            output.write(f"# aristas_supergrafo_activas {len(edges)}\n")
            output.write(f"# aristas_objetivo_por_cluster {target_size}\n")
            output.write(f"# factor_absorcion_clusters {CLUSTER_ABSORPTION_RATIO:g}\n")
            output.write(f"# max_aristas_por_cluster {maximum_cluster_size}\n")
            output.write(f"# nodos_neutros_forzados {neutral_node_count}\n")
            output.write(f"# estrategia {strategy}\n")
            output.write(f"# caminos_divisores {divider_count}\n")
            output.write("arista cluster origen destino vehiculo servicio recorridos pasadas\n")
            for edge, cluster in zip(edges, assignments):
                output.write(
                    f"{edge.index} {cluster} {edge.source} {edge.target} {edge.vehicle} "
                    f"{edge.service_count} {edge.traversal_count} {edge.passages}\n"
                )
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def primary_node_clusters(edges: list[Edge], assignments: list[int]) -> dict[str, int]:
    votes: dict[str, dict[int, int]] = defaultdict(lambda: defaultdict(int))
    for edge, cluster in zip(edges, assignments):
        votes[edge.source][cluster] += 1
        votes[edge.target][cluster] += 1
    return {
        node: min(counts, key=lambda cluster: (-counts[cluster], cluster))
        for node, counts in votes.items()
    }


def graph_layout(edges: list[Edge], assignments: list[int],
                 node_clusters: dict[str, int]) -> dict[str, tuple[float, float]]:
    """Return a deterministic, cluster-aware layout for the active graph."""
    nodes = sorted({node for edge in edges for node in (edge.source, edge.target)},
                   key=node_sort_key)
    if len(nodes) == 1:
        return {nodes[0]: (400.0, 420.0)}

    # Large supergraphs contain hundreds of virtual nodes. Grouping each node
    # near the cluster incident to most of its edges makes the partition visible
    # without unreadable labels or a ring of nodes around an empty canvas.
    if len(nodes) > 80:
        groups: dict[int, list[str]] = defaultdict(list)
        for node in nodes:
            groups[node_clusters[node]].append(node)
        cluster_ids = sorted(groups)
        columns = math.ceil(math.sqrt(len(cluster_ids)))
        rows = math.ceil(len(cluster_ids) / columns)
        cell_width, cell_height = 700.0 / columns, 700.0 / rows
        positions: dict[str, tuple[float, float]] = {}
        for group_index, cluster in enumerate(cluster_ids):
            group = sorted(groups[cluster], key=node_sort_key)
            group_column, group_row = group_index % columns, group_index // columns
            node_columns = max(1, math.ceil(math.sqrt(len(group) * cell_width / cell_height)))
            node_rows = math.ceil(len(group) / node_columns)
            for node_index, node in enumerate(group):
                row, column = divmod(node_index, node_columns)
                if row % 2:
                    column = node_columns - 1 - column
                positions[node] = (
                    50.0 + group_column * cell_width
                    + (column + 0.5) * cell_width / node_columns,
                    90.0 + group_row * cell_height
                    + (row + 0.5) * cell_height / node_rows,
                )
        return positions

    center_x, center_y, radius = 400.0, 420.0, 310.0
    positions = {
        node: (
            center_x + radius * math.cos(2 * math.pi * index / len(nodes) - math.pi / 2),
            center_y + radius * math.sin(2 * math.pi * index / len(nodes) - math.pi / 2),
        )
        for index, node in enumerate(nodes)
    }
    # Avoid quadratic work only for exceptionally large solutions.
    if len(nodes) > 1200:
        return positions

    connections = sorted({tuple(sorted((edge.source, edge.target), key=node_sort_key))
                          for edge in edges if edge.source != edge.target},
                         key=lambda pair: (node_sort_key(pair[0]), node_sort_key(pair[1])))
    area = 680.0 * 680.0
    ideal = math.sqrt(area / len(nodes))
    temperature = 55.0
    iterations = 80 if len(nodes) <= 250 else (45 if len(nodes) <= 600 else 30)
    for _ in range(iterations):
        displacement = {node: [0.0, 0.0] for node in nodes}
        for left_index, left in enumerate(nodes):
            left_x, left_y = positions[left]
            for right in nodes[left_index + 1:]:
                right_x, right_y = positions[right]
                dx, dy = left_x - right_x, left_y - right_y
                distance = max(math.hypot(dx, dy), 0.01)
                force = ideal * ideal / distance
                force_x, force_y = dx / distance * force, dy / distance * force
                displacement[left][0] += force_x
                displacement[left][1] += force_y
                displacement[right][0] -= force_x
                displacement[right][1] -= force_y
        for left, right in connections:
            left_x, left_y = positions[left]
            right_x, right_y = positions[right]
            dx, dy = left_x - right_x, left_y - right_y
            distance = max(math.hypot(dx, dy), 0.01)
            force = distance * distance / ideal
            force_x, force_y = dx / distance * force, dy / distance * force
            displacement[left][0] -= force_x
            displacement[left][1] -= force_y
            displacement[right][0] += force_x
            displacement[right][1] += force_y
        for node in nodes:
            dx, dy = displacement[node]
            length = max(math.hypot(dx, dy), 0.01)
            move = min(length, temperature)
            x, y = positions[node]
            positions[node] = (
                min(750.0, max(50.0, x + dx / length * move)),
                min(790.0, max(80.0, y + dy / length * move)),
            )
        temperature *= 0.94
    return positions


def coordinate_layout(geometry: CoordinateGeometry) -> dict[str, tuple[float, float]]:
    if not geometry.coordinates:
        raise ValueError("faltan coordenadas para ubicar el grafo original")
    coordinates = geometry.coordinates
    min_x = min(x for x, _ in coordinates.values())
    max_x = max(x for x, _ in coordinates.values())
    min_y = min(y for _, y in coordinates.values())
    max_y = max(y for _, y in coordinates.values())
    raw_width, raw_height = max(max_x - min_x, 1e-9), max(max_y - min_y, 1e-9)
    scale = min(680.0 / raw_width, 680.0 / raw_height)
    drawing_width, drawing_height = raw_width * scale, raw_height * scale
    x_offset = 50.0 + (680.0 - drawing_width) / 2.0
    y_offset = 90.0 + (680.0 - drawing_height) / 2.0
    positions = {
        node: (x_offset + (x - min_x) * scale, y_offset + (y - min_y) * scale)
        for node, (x, y) in coordinates.items()
    }

    # D is synthetic: map coordinates belong only to real road nodes. Place the
    # depot after projecting the map, at a small fixed screen-space distance
    # outside the network, so it neither pretends to have a geographic location
    # nor changes the scale used for the real geometry.
    adjacent_x, adjacent_y = positions[geometry.deposit_adjacent]
    center_x = sum(x for x, _ in positions.values()) / len(positions)
    center_y = sum(y for _, y in positions.values()) / len(positions)
    direction_x, direction_y = adjacent_x - center_x, adjacent_y - center_y
    direction_length = math.hypot(direction_x, direction_y)
    if direction_length < 1e-9:
        direction_x, direction_y, direction_length = -1.0, -1.0, math.sqrt(2.0)
    visual_offset = 18.0
    positions["D"] = (
        adjacent_x + direction_x / direction_length * visual_offset,
        adjacent_y + direction_y / direction_length * visual_offset,
    )
    return positions


def project_edges(edges: list[Edge], assignments: list[int],
                  geometry: CoordinateGeometry) -> tuple[list[Edge], list[int]]:
    projected_edges: list[Edge] = []
    projected_assignments: list[int] = []
    for edge, cluster in zip(edges, assignments):
        try:
            source = "D" if edge.source == "D" else geometry.virtual_to_original[edge.source]
            target = "D" if edge.target == "D" else geometry.virtual_to_original[edge.target]
        except KeyError as error:
            raise ValueError(f"nodo virtual inexistente en el grafo: {error.args[0]}") from error
        # Turn connectors begin and end at virtual nodes of the same original
        # intersection, so they have no geometric road segment to draw.
        if source == target:
            continue
        projected_edges.append(replace(edge, source=source, target=target))
        projected_assignments.append(cluster)
    if not projected_edges:
        raise ValueError("no hay aristas recorridas proyectables sobre las coordenadas")
    return projected_edges, projected_assignments


def cluster_color(cluster: int) -> str:
    if cluster <= len(CLUSTER_COLORS):
        return CLUSTER_COLORS[cluster - 1]
    hue = ((cluster - 1) * 137.508) % 360
    return f"hsl({hue:.1f} 68% 42%)"


def write_svg(path: Path, edges: list[Edge], assignments: list[int],
              geometry: CoordinateGeometry | None = None,
              neutral_nodes: set[str] | None = None) -> None:
    display_edges, display_assignments = (
        project_edges(edges, assignments, geometry) if geometry is not None
        else (edges, assignments)
    )
    node_clusters = primary_node_clusters(display_edges, display_assignments)
    neutral = neutral_nodes or set()
    positions = (
        coordinate_layout(geometry) if geometry is not None and geometry.coordinates
        else graph_layout(display_edges, display_assignments, node_clusters)
    )
    cluster_count = max(assignments)
    cluster_sizes = [0] * cluster_count
    for cluster in assignments:
        cluster_sizes[cluster - 1] += 1
    legend_columns = max(1, math.ceil(cluster_count / 24))
    width = 820 + 155 * legend_columns
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} 850">',
        f'<rect width="{width}" height="850" fill="white"/>',
        '<text x="40" y="35" font-family="sans-serif" font-size="22" '
        'font-weight="bold" fill="#111827">Clusters BFS del recorrido RCPP</text>',
        '<text x="40" y="58" font-family="sans-serif" font-size="13" '
        f'fill="#4b5563">Color: cluster · Grosor: pasadas · Punteada: cruce entre clusters'
        f'{" · Naranja: nodo neutro" if neutral else ""}</text>',
        '<rect x="25" y="70" width="750" height="745" rx="10" '
        'fill="#f9fafb" stroke="#d1d5db"/>',
    ]

    if geometry is not None and geometry.coordinates:
        for source, target in geometry.original_edges:
            x1, y1 = positions[source]
            x2, y2 = positions[target]
            svg.append(
                f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
                'stroke="#d1d5db" stroke-width="1.2" opacity="0.8"/>'
            )

    parallel: dict[tuple[str, str], list[int]] = defaultdict(list)
    for position, edge in enumerate(display_edges):
        key = tuple(sorted((edge.source, edge.target), key=node_sort_key))
        parallel[key].append(position)
    parallel_rank = {
        position: (rank, len(positions_for_pair))
        for positions_for_pair in parallel.values()
        for rank, position in enumerate(positions_for_pair)
    }

    for position, (edge, cluster) in enumerate(zip(display_edges, display_assignments)):
        x1, y1 = positions[edge.source]
        x2, y2 = positions[edge.target]
        color = cluster_color(cluster)
        crosses_clusters = node_clusters[edge.source] != node_clusters[edge.target]
        dash = ' stroke-dasharray="8 6"' if crosses_clusters else ''
        stroke_width = 2.0 + min(4.0, math.log2(max(1, edge.passages)))
        rank, parallel_count = parallel_rank[position]
        title = escape(
            f"Arista {edge.index}: {edge.source} -> {edge.target}; "
            f"cluster {cluster}; vehiculo {edge.vehicle}; pasadas {edge.passages}"
        )
        if edge.source == edge.target:
            svg.append(
                f'<circle cx="{x1 + 10:.2f}" cy="{y1 - 10:.2f}" r="15" fill="none" '
                f'stroke="{color}" stroke-width="{stroke_width:.2f}" opacity="0.82">'
                f'<title>{title}</title></circle>'
            )
            continue
        dx, dy = x2 - x1, y2 - y1
        distance = max(math.hypot(dx, dy), 0.01)
        offset = (rank - (parallel_count - 1) / 2.0) * 12.0
        control_x = (x1 + x2) / 2.0 - dy / distance * offset
        control_y = (y1 + y2) / 2.0 + dx / distance * offset
        svg.append(
            f'<path d="M {x1:.2f} {y1:.2f} Q {control_x:.2f} {control_y:.2f} '
            f'{x2:.2f} {y2:.2f}" fill="none" stroke="{color}" '
            f'stroke-width="{stroke_width:.2f}" opacity="0.78"{dash}>'
            f'<title>{title}</title></path>'
        )

    show_all_labels = len(positions) <= 80
    node_radius = 6 if show_all_labels else 3
    for node in sorted(positions, key=node_sort_key):
        x, y = positions[node]
        if node == "D":
            svg.append(f'<rect x="{x - 7:.2f}" y="{y - 7:.2f}" width="14" height="14" '
                       'rx="2" fill="#111827" stroke="white" stroke-width="2"/>')
        else:
            is_neutral = node in neutral
            radius = 7 if is_neutral else node_radius
            fill = "#f59e0b" if is_neutral else "#111827"
            label = "Nodo neutro" if is_neutral else "Nodo"
            svg.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius}" '
                       f'fill="{fill}" stroke="white" stroke-width="1" '
                       f'data-neutral="{str(is_neutral).lower()}">'
                       f'<title>{label} {escape(node)}</title></circle>')
        if show_all_labels or node == "D":
            svg.append(f'<text x="{x + 9:.2f}" y="{y - 8:.2f}" font-family="sans-serif" '
                       f'font-size="11" fill="#111827">{escape(node)}</text>')

    for cluster, size in enumerate(cluster_sizes, start=1):
        column, row = divmod(cluster - 1, 24)
        x, y = 805 + column * 155, 95 + row * 29
        color = cluster_color(cluster)
        svg.append(f'<line x1="{x}" y1="{y}" x2="{x + 24}" y2="{y}" '
                   f'stroke="{color}" stroke-width="5"/>')
        svg.append(f'<text x="{x + 32}" y="{y + 4}" font-family="sans-serif" '
                   f'font-size="13" fill="#111827">Cluster {cluster} ({size})</text>')
    svg.append('</svg>')
    try:
        path.write_text("\n".join(svg) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def write_png(svg_path: Path, png_path: Path) -> None:
    renderer = shutil.which("rsvg-convert")
    command = [renderer, "--output", str(png_path), str(svg_path)] if renderer else None
    if command is None:
        renderer = shutil.which("magick")
        command = [renderer, str(svg_path), str(png_path)] if renderer else None
    if command is None:
        try:
            import cairosvg
        except ImportError as error:
            raise ValueError(
                "no se pudo generar PNG: instale rsvg-convert, ImageMagick o CairoSVG"
            ) from error
        try:
            cairosvg.svg2png(url=str(svg_path), write_to=str(png_path))
        except Exception as error:
            raise ValueError(f"no se pudo generar {png_path}: {error}") from error
        return
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise ValueError(f"no se pudo generar {png_path}: {error}") from error
    if result.returncode != 0:
        diagnostic = result.stderr.strip() or result.stdout.strip() or "fallo del renderizador"
        raise ValueError(f"no se pudo generar {png_path}: {diagnostic}")


def infer_node_count(solution_path: Path) -> int:
    match = OUTPUT_NAME.fullmatch(solution_path.name)
    if match is None:
        raise ValueError(
            "no se pudo inferir N: use min_dist_N.dat o indique --nodes N"
        )
    return int(match.group(1))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=("Agrupa las aristas del grafo antes del solver mediante "
                     "BFS o caminos divisores y proyecta la particion sobre "
                     "una solucion RCPP.")
    )
    parser.add_argument("solution", type=Path, nargs="?",
                        help="archivo data/N/min_dist_N.dat (omitido con --graph-only)")
    parser.add_argument(
        "--percentage", type=float,
        help="porcentaje objetivo inicial de aristas originales distintas por cluster (k)",
    )
    parser.add_argument("--nodes", type=int, help="cantidad de nodos originales (N)")
    parser.add_argument("--output", type=Path, help="ruta de salida opcional")
    parser.add_argument("--graph", type=Path, required=True,
                        help="data/N/graph_N.dat usado para agrupar el grafo original")
    parser.add_argument(
        "--graph-only", action="store_true",
        help="genera clusters para todas las aristas del grafo, sin una solucion previa",
    )
    parser.add_argument(
        "--graph-clusters", type=Path,
        help=("particion pre-solver que se proyecta sobre la solucion, sin volver "
              "a ejecutar BFS"),
    )
    parser.add_argument("--coords", type=Path,
                        help="data/N/graph_N.coords.csv para ubicar los nodos")
    parser.add_argument(
        "--neutral-nodes", type=Path,
        help=("lista node_id/reasons exportada por el generador OSM; los clusters "
              "no crecen a traves de esos nodos"),
    )
    parser.add_argument(
        "--neutral-nodes-output", type=Path,
        help=("salida con los nodos neutros efectivos, incluidos los caminos "
              "divisores creados por la clusterizacion"),
    )
    parser.add_argument(
        "--osm", type=Path,
        help="OSM exportado para priorizar la continuidad del nombre de calle",
    )
    parser.add_argument(
        "--node-mapping", type=Path,
        help="mapeo nodo OSM/nodo interno usado junto con --osm",
    )
    parser.add_argument(
        "--strategy", choices=("bfs", "straight-dividers"), default="bfs",
        help=("bfs tradicional o division recursiva mediante calles rectas "
              "entre nodos neutros"),
    )
    return parser.parse_args()


def main() -> int:
    options = parse_arguments()
    try:
        if options.graph_only and options.solution is not None:
            raise ValueError("--graph-only no acepta un archivo de solucion")
        if options.graph_only and options.graph_clusters is not None:
            raise ValueError("--graph-only no acepta --graph-clusters")
        if not options.graph_only and options.neutral_nodes is not None:
            raise ValueError("--neutral-nodes solo se usa con --graph-only")
        if not options.graph_only and options.neutral_nodes_output is not None:
            raise ValueError("--neutral-nodes-output solo se usa con --graph-only")
        if (options.osm is None) != (options.node_mapping is None):
            raise ValueError("--osm y --node-mapping deben indicarse juntos")
        if options.osm is not None and (
                not options.graph_only or options.strategy != "straight-dividers"):
            raise ValueError(
                "--osm y --node-mapping solo se usan con --graph-only "
                "--strategy straight-dividers"
            )
        if not options.graph_only and options.strategy != "bfs":
            raise ValueError("--strategy solo se usa con --graph-only")
        if not options.graph_only and options.solution is None:
            raise ValueError("falta el archivo de solucion (o use --graph-only)")
        if options.graph_only and options.percentage is None:
            raise ValueError("--graph-only requiere --percentage")
        if (not options.graph_only and options.graph_clusters is None and
                options.percentage is None):
            raise ValueError("indique --graph-clusters o --percentage")
        geometry = read_coordinate_geometry(options.graph, options.coords)
        node_count = options.nodes
        if node_count is None:
            node_count = (geometry.node_count if options.graph_only
                          else infer_node_count(options.solution))
        if node_count <= 0:
            raise ValueError("--nodes debe ser un entero positivo")
        if geometry.node_count != node_count:
            raise ValueError("la cantidad de nodos del grafo no coincide con N")
        output_path = options.output or Path("data") / str(node_count) / f"clusters_{node_count}.dat"
        if options.graph_only:
            neutral_node_data = (
                read_neutral_nodes(options.neutral_nodes, node_count)
                if options.neutral_nodes is not None else {}
            )
            required_edges = required_graph_edges(geometry)
            required_nodes = {
                int(node)
                for edge in required_edges
                for node in (edge.source, edge.target)
            }
            neutral_node_data = {
                node: reasons for node, reasons in neutral_node_data.items()
                if node in required_nodes
            }
            required_edge_count = len(required_edges)
            divider_count = 0
            if options.strategy == "straight-dividers":
                if options.neutral_nodes is None:
                    raise ValueError(
                        "--strategy straight-dividers requiere --neutral-nodes"
                    )
                street_names = (
                    read_osm_street_names(
                        options.osm, options.node_mapping, geometry
                    )
                    if options.osm is not None else None
                )
                edges, assignments, maximum_cluster_size, neutral_node_data, \
                    divider_count = straight_divider_clusters(
                        geometry, options.percentage, neutral_node_data,
                        street_names,
                    )
            else:
                edges, assignments, maximum_cluster_size = cluster_graph(
                    geometry, options.percentage,
                    {str(node) for node in neutral_node_data},
                )
            write_clusters(output_path, edges, assignments, options.percentage,
                           required_edge_count, maximum_cluster_size,
                           len(neutral_node_data), options.strategy,
                           divider_count)
            if options.neutral_nodes_output is not None:
                write_neutral_nodes(
                    options.neutral_nodes_output, neutral_node_data, node_count
                )
            print(
                f"Clusters pre-solver guardados en {output_path} "
                f"({max(assignments)} clusters, {required_edge_count} aristas "
                f"originales, {len(neutral_node_data)} nodos neutros efectivos, "
                f"{divider_count} caminos divisores)"
            )
            return 0

        edges = read_solution(options.solution)
        percentage = options.percentage
        if options.graph_clusters is not None:
            graph_assignments, stored_percentage, maximum_cluster_size = read_graph_clusters(
                options.graph_clusters, geometry
            )
            assignments = project_graph_clusters(edges, geometry, graph_assignments)
            original_edge_count = len(graph_assignments)
            if percentage is None:
                percentage = stored_percentage
            if percentage is None:
                raise ValueError(
                    "falta porcentaje_objetivo en --graph-clusters; indique --percentage"
                )
        else:
            assignments, original_edge_count, maximum_cluster_size = cluster_original_graph(
                edges, percentage, geometry
            )
        if not math.isfinite(percentage) or percentage <= 0 or percentage > 100:
            raise ValueError(
                "--percentage debe ser mayor que 0 y menor o igual que 100"
            )
        write_clusters(output_path, edges, assignments, percentage,
                       original_edge_count, maximum_cluster_size)
        print(
            f"Clusters guardados en {output_path} "
            f"({max(assignments)} clusters, {original_edge_count} aristas originales, "
            f"{len(edges)} aristas activas del supergrafo)"
        )
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
