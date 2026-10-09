#!/usr/bin/env python3
"""Generate original-graph edge clusters and apply them to a supergraph solution."""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
import csv
from dataclasses import dataclass, replace
from html import escape
from pathlib import Path
import math
import re
import shutil
import subprocess
import sys


SECTION_HEADERS = {
    "---- X ----": "service",
    "---- Y ----": "traversals",
    "---- YDK & YKD ----": "deposit",
    "---- F ----": "ignored",
    "---- FDK ----": "ignored",
}
VARIABLE = re.compile(r"^([XY])_([^_]+)_([^_]+)_([1-9][0-9]*)\s*=\s*([0-9]+)$")
OUTPUT_NAME = re.compile(r"^out_([1-9][0-9]*)\.dat$")
CLUSTER_COLORS = (
    "#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c", "#0891b2",
    "#db2777", "#65a30d", "#4f46e5", "#ca8a04", "#0f766e", "#c026d3",
)


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


def node_sort_key(node: str) -> tuple[int, int]:
    return (0, 0) if node == "D" else (1, int(node))


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
    virtual_to_original: dict[str, str] = {}
    virtual_to_edge: dict[str, int] = {}
    offset = 5 + deposit_count
    orientation = 0
    for edge_index in range(edge_count + arc_count):
        record = tokens[offset + 5 * edge_index:offset + 5 * (edge_index + 1)]
        try:
            source, target = int(record[0]), int(record[1])
        except ValueError as error:
            raise ValueError(f"{graph_path}: extremos de arista invalidos") from error
        if not (1 <= source <= node_count and 1 <= target <= node_count):
            raise ValueError(f"{graph_path}: extremo de arista fuera de rango")
        original_edges.append((str(source), str(target)))

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
            virtual_to_edge, str(deposit_nodes[0])
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
        virtual_to_edge, str(deposit_nodes[0])
    )


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


def bfs_clusters(edges: list[Edge], percentage: float) -> list[int]:
    """Assign each edge to an edge-connected BFS cluster of bounded size."""
    if not math.isfinite(percentage) or percentage <= 0 or percentage > 100:
        raise ValueError("k debe ser un porcentaje mayor que 0 y menor o igual que 100")
    target_size = max(1, math.ceil(len(edges) * percentage / 100.0))
    incident: dict[str, list[int]] = defaultdict(list)
    for position, edge in enumerate(edges):
        incident[edge.source].append(position)
        if edge.target != edge.source:
            incident[edge.target].append(position)

    assignments = [0] * len(edges)
    cluster = 0
    for seed in range(len(edges)):
        if assignments[seed] != 0:
            continue
        cluster += 1
        queue: deque[str] = deque()
        queued_nodes: set[str] = set()

        def add_edge(position: int) -> None:
            assignments[position] = cluster
            edge = edges[position]
            for node in (edge.source, edge.target):
                if node not in queued_nodes:
                    queued_nodes.add(node)
                    queue.append(node)

        add_edge(seed)
        assigned = 1
        while queue and assigned < target_size:
            node = queue.popleft()
            for position in incident[node]:
                if assignments[position] != 0:
                    continue
                add_edge(position)
                assigned += 1
                if assigned == target_size:
                    break
    return assignments


def cluster_original_graph(edges: list[Edge], percentage: float,
                           geometry: CoordinateGeometry) -> tuple[list[int], int]:
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
    return expanded_assignments, len(original_edges)


def write_clusters(path: Path, edges: list[Edge], assignments: list[int], percentage: float,
                   clustered_edge_count: int | None = None) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    clustered_edge_count = len(edges) if clustered_edge_count is None else clustered_edge_count
    target_size = max(1, math.ceil(clustered_edge_count * percentage / 100.0))
    try:
        with path.open("w", encoding="utf-8") as output:
            output.write(f"# porcentaje_objetivo {percentage:g}\n")
            # Keep the legacy count for readers that consume this comment.
            output.write(f"# aristas_activas {len(edges)}\n")
            output.write(f"# aristas_originales_activas {clustered_edge_count}\n")
            output.write(f"# aristas_supergrafo_activas {len(edges)}\n")
            output.write(f"# max_aristas_por_cluster {target_size}\n")
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
              geometry: CoordinateGeometry | None = None) -> None:
    display_edges, display_assignments = (
        project_edges(edges, assignments, geometry) if geometry is not None
        else (edges, assignments)
    )
    node_clusters = primary_node_clusters(display_edges, display_assignments)
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
        'fill="#4b5563">Color: cluster · Grosor: pasadas · Punteada: cruce entre clusters</text>',
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
            svg.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{node_radius}" '
                       f'fill="#111827" stroke="white" stroke-width="1">'
                       f'<title>Nodo {escape(node)}</title></circle>')
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
            "no se pudo inferir N: use un archivo out_N.dat o indique --nodes N"
        )
    return int(match.group(1))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Agrupa por BFS las aristas recorridas de una solucion RCPP."
    )
    parser.add_argument("solution", type=Path, help="archivo output/dist/out_N.dat")
    parser.add_argument(
        "--percentage", type=float, required=True,
        help="porcentaje maximo de aristas originales distintas por cluster (k)",
    )
    parser.add_argument("--nodes", type=int, help="cantidad de nodos originales (N)")
    parser.add_argument("--output", type=Path, help="ruta de salida opcional")
    parser.add_argument("--graph", type=Path, required=True,
                        help="input/graph_N.dat usado para agrupar el grafo original")
    parser.add_argument("--coords", type=Path,
                        help="data/coords/graph_N.coords.csv para ubicar los nodos")
    return parser.parse_args()


def main() -> int:
    options = parse_arguments()
    try:
        node_count = options.nodes if options.nodes is not None else infer_node_count(options.solution)
        if node_count <= 0:
            raise ValueError("--nodes debe ser un entero positivo")
        geometry = read_coordinate_geometry(options.graph, options.coords)
        if geometry.node_count != node_count:
            raise ValueError("la cantidad de nodos del grafo no coincide con N")
        edges = read_solution(options.solution)
        assignments, original_edge_count = cluster_original_graph(
            edges, options.percentage, geometry
        )
        output_path = options.output or Path("data/clusters") / f"clusters_{node_count}.dat"
        write_clusters(output_path, edges, assignments, options.percentage,
                       original_edge_count)
        svg_path = output_path.with_suffix(".svg")
        write_svg(svg_path, edges, assignments, geometry)
        png_path = output_path.with_suffix(".png")
        write_png(svg_path, png_path)
        print(
            f"Clusters guardados en {output_path}, {svg_path} y {png_path} "
            f"({max(assignments)} clusters, {original_edge_count} aristas originales, "
            f"{len(edges)} aristas activas del supergrafo)"
        )
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
