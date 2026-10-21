#!/usr/bin/env python3
"""Expand neutral cluster boundaries before solving an RCPP instance.

A neutral component is copied once for every cluster that can reach it. Moving
inside one copy keeps the physical graph costs. Changing from the copy for
cluster ``i`` to the copy for cluster ``j`` uses a synthetic directed arc whose
cost is the sum of the non-neutral edge costs in both clusters. Edges joining
two neutral nodes do not belong to any cluster.
"""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
from html import escape
import math
from pathlib import Path
import sys
import tempfile
from itertools import product

from clusterGeneration.generate_clusters import (
    CoordinateGeometry,
    Edge,
    cluster_color,
    coordinate_layout,
    graph_layout,
    primary_node_clusters,
    read_coordinate_geometry,
    write_png,
)


CLUSTER_HEADER = "arista cluster origen destino vehiculo servicio recorridos pasadas"
IN_OUT_HEADER = "source_cluster target_cluster in_node out_node"
RECORD_CLUSTERS_HEADER = "record cluster"


@dataclass(frozen=True)
class GraphRecord:
    source: int
    target: int
    zone: int
    cost: Decimal
    demand: Decimal


@dataclass(frozen=True)
class GraphData:
    vehicles: int
    nodes: int
    deposit_nodes: list[int]
    edge_count: int
    arc_count: int
    records: list[GraphRecord]


@dataclass(frozen=True)
class Expansion:
    graph: GraphData
    neutral_nodes: tuple[int, ...]
    node_copies: dict[tuple[int, int], int]
    transition_count: int
    transition_clusters: tuple[tuple[int, int], ...]
    cluster_costs: dict[int, Decimal]
    record_clusters: tuple[int | None, ...]
    record_origins: tuple[int | None, ...]
    expanded_record_clusters: tuple[int | None, ...]
    virtual_node_mapping: dict[int, int | None]
    original_node_copies: dict[int, tuple[int, ...]]


def read_graph(path: Path) -> GraphData:
    try:
        tokens = path.read_text(encoding="utf-8").split()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if len(tokens) < 5:
        raise ValueError(f"{path}: cabecera incompleta")
    try:
        vehicles, nodes, deposit_count, edge_count, arc_count = map(int, tokens[:5])
    except ValueError as error:
        raise ValueError(f"{path}: cabecera invalida") from error
    if vehicles <= 0 or nodes <= 0 or not 0 <= deposit_count <= nodes:
        raise ValueError(f"{path}: cantidades invalidas")
    if edge_count < 0 or arc_count < 0:
        raise ValueError(f"{path}: cantidades invalidas")
    expected = 5 + deposit_count + 5 * (edge_count + arc_count)
    if len(tokens) != expected:
        raise ValueError(f"{path}: cantidad de campos invalida")
    try:
        deposit_nodes = [int(token) for token in tokens[5:5 + deposit_count]]
    except ValueError as error:
        raise ValueError(f"{path}: nodo de deposito invalido") from error
    if any(node < 1 or node > nodes for node in deposit_nodes):
        raise ValueError(f"{path}: nodo de deposito fuera de rango")

    records = []
    offset = 5 + deposit_count
    for position in range(edge_count + arc_count):
        fields = tokens[offset + 5 * position:offset + 5 * (position + 1)]
        try:
            source, target, zone = map(int, fields[:3])
            cost, demand = map(Decimal, fields[3:])
        except (ValueError, InvalidOperation) as error:
            raise ValueError(f"{path}: registro {position + 1} invalido") from error
        if not (1 <= source <= nodes and 1 <= target <= nodes):
            raise ValueError(f"{path}: registro {position + 1}: nodo fuera de rango")
        if zone < -1 or zone > vehicles:
            raise ValueError(f"{path}: registro {position + 1}: zona fuera de rango")
        if not cost.is_finite() or cost <= 0:
            raise ValueError(f"{path}: registro {position + 1}: costo invalido")
        if not demand.is_finite() or demand < 0:
            raise ValueError(f"{path}: registro {position + 1}: demanda invalida")
        records.append(GraphRecord(source, target, zone, cost, demand))
    return GraphData(vehicles, nodes, deposit_nodes, edge_count, arc_count, records)


def read_edge_clusters(path: Path, graph: GraphData) -> list[int]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    rows = [line.strip() for line in lines if line.strip() and not line.lstrip().startswith("#")]
    if not rows or rows[0] != CLUSTER_HEADER:
        raise ValueError(f"{path}: cabecera invalida")
    assignments = [0] * len(graph.records)
    for line_number, row in enumerate(rows[1:], start=2):
        fields = row.split()
        if len(fields) != 8:
            raise ValueError(f"{path}: fila {line_number} invalida")
        try:
            edge_id, cluster = int(fields[0]), int(fields[1])
            source, target = int(fields[2]), int(fields[3])
            vehicle, service, deadhead, passages = map(int, fields[4:])
        except ValueError as error:
            raise ValueError(f"{path}: fila {line_number} invalida") from error
        if not 1 <= edge_id <= len(graph.records) or assignments[edge_id - 1] != 0:
            raise ValueError(f"{path}: identificador de arista invalido o duplicado")
        record = graph.records[edge_id - 1]
        if (source, target) != (record.source, record.target):
            raise ValueError(f"{path}: fila {line_number}: extremos no coinciden con el grafo")
        if cluster <= 0:
            raise ValueError(f"{path}: fila {line_number}: cluster debe ser positivo")
        if vehicle <= 0 or service != 0 or deadhead != 0 or passages != 0:
            raise ValueError(f"{path}: fila {line_number}: se requieren clusters pre-solver")
        assignments[edge_id - 1] = cluster
    if any(cluster == 0 for cluster in assignments):
        raise ValueError(f"{path}: faltan asignaciones para aristas del grafo")
    return assignments


def _neutral_components(graph: GraphData, assignments: list[int]) -> tuple[set[int], list[set[int]]]:
    incident_clusters: dict[int, set[int]] = defaultdict(set)
    for record, cluster in zip(graph.records, assignments):
        incident_clusters[record.source].add(cluster)
        incident_clusters[record.target].add(cluster)
    neutral = {node for node, clusters in incident_clusters.items() if len(clusters) > 1}

    adjacency: dict[int, set[int]] = defaultdict(set)
    for record in graph.records:
        if record.source in neutral and record.target in neutral:
            adjacency[record.source].add(record.target)
            adjacency[record.target].add(record.source)

    components = []
    unseen = set(neutral)
    while unseen:
        start = min(unseen)
        component = {start}
        pending = deque([start])
        unseen.remove(start)
        while pending:
            node = pending.popleft()
            for neighbour in adjacency[node]:
                if neighbour in unseen:
                    unseen.remove(neighbour)
                    component.add(neighbour)
                    pending.append(neighbour)
        components.append(component)
    return neutral, components


def expand_neutral_nodes(graph: GraphData, assignments: list[int]) -> Expansion:
    """Expand every neutral component into one layer per adjacent cluster."""
    if len(assignments) != len(graph.records):
        raise ValueError("la cantidad de clusters no coincide con el grafo")
    if any(cluster <= 0 for cluster in assignments):
        raise ValueError("los identificadores de cluster deben ser positivos")

    neutral, components = _neutral_components(graph, assignments)
    # Once the boundary nodes are known, a record joining two of them belongs
    # to the neutral component rather than to the arbitrary BFS cluster that
    # reached it first.
    record_clusters: tuple[int | None, ...] = tuple(
        None if record.source in neutral and record.target in neutral else cluster
        for record, cluster in zip(graph.records, assignments)
    )
    cluster_costs: dict[int, Decimal] = defaultdict(Decimal)
    for record, cluster in zip(graph.records, record_clusters):
        if cluster is not None:
            cluster_costs[cluster] += record.cost

    component_for_node = {
        node: component_index
        for component_index, component in enumerate(components)
        for node in component
    }
    component_clusters: list[set[int]] = [set() for _ in components]
    for record, cluster in zip(graph.records, record_clusters):
        if cluster is None:
            continue
        if record.source in neutral:
            component_clusters[component_for_node[record.source]].add(cluster)
        if record.target in neutral:
            component_clusters[component_for_node[record.target]].add(cluster)
    # A closed neutral component can contain no clustered boundary record at
    # all. It still needs one structural layer so its physical records are not
    # dropped from the expanded graph.
    for component_index, component in enumerate(components):
        if component_clusters[component_index]:
            continue
        component_clusters[component_index].add(min(
            cluster
            for record, cluster in zip(graph.records, assignments)
            if record.source in component or record.target in component
        ))

    node_copies: dict[tuple[int, int], int] = {}
    next_node = graph.nodes + 1
    for component_index, component in enumerate(components):
        clusters = sorted(component_clusters[component_index])
        for node in sorted(component):
            node_copies[(node, clusters[0])] = node
            for cluster in clusters[1:]:
                node_copies[(node, cluster)] = next_node
                next_node += 1

    def mapped(node: int, cluster: int) -> int:
        return node_copies.get((node, cluster), node)

    expanded_edges: list[GraphRecord] = []
    expanded_arcs: list[GraphRecord] = []
    edge_origins: list[int | None] = []
    arc_origins: list[int | None] = []
    edge_clusters: list[int] = []
    arc_clusters: list[int] = []
    for record_index, (record, cluster) in enumerate(zip(graph.records, record_clusters)):
        destination = expanded_edges if record_index < graph.edge_count else expanded_arcs
        origin_destination = edge_origins if record_index < graph.edge_count else arc_origins
        cluster_destination = edge_clusters if record_index < graph.edge_count else arc_clusters
        both_neutral = record.source in neutral and record.target in neutral
        if both_neutral:
            component_index = component_for_node[record.source]
            if component_index != component_for_node[record.target]:
                raise ValueError("una arista neutra conecta componentes distintas")
            layers = sorted(component_clusters[component_index])
            for layer_index, layer in enumerate(layers):
                # A neutral record has no cluster. Keep a single required copy
                # only because the expanded RCPP graph must preserve service.
                required_copy = layer_index == 0
                destination.append(GraphRecord(
                    mapped(record.source, layer), mapped(record.target, layer),
                    record.zone if required_copy else 0,
                    record.cost,
                    record.demand if required_copy else Decimal(0),
                ))
                origin_destination.append(record_index)
                cluster_destination.append(layer)
        else:
            if cluster is None:
                raise ValueError("una arista no neutra quedo sin cluster")
            destination.append(GraphRecord(
                mapped(record.source, cluster), mapped(record.target, cluster),
                record.zone, record.cost, record.demand,
            ))
            origin_destination.append(record_index)
            cluster_destination.append(cluster)

    transitions: list[GraphRecord] = []
    transition_clusters: list[tuple[int, int]] = []
    for node in sorted(neutral):
        clusters = sorted(component_clusters[component_for_node[node]])
        for source_cluster in clusters:
            for target_cluster in clusters:
                if source_cluster == target_cluster:
                    continue
                transitions.append(GraphRecord(
                    mapped(node, source_cluster), mapped(node, target_cluster), 0,
                    cluster_costs[source_cluster] + cluster_costs[target_cluster],
                    Decimal(0),
                ))
                transition_clusters.append((source_cluster, target_cluster))

    record_origins = tuple(edge_origins + arc_origins + [None] * len(transitions))
    expanded_record_clusters = tuple(
        edge_clusters + arc_clusters + [None] * len(transitions)
    )

    expanded_deposit_nodes = []
    for node in graph.deposit_nodes:
        if node not in neutral:
            expanded_deposit_nodes.append(node)
            continue
        clusters = sorted(component_clusters[component_for_node[node]])
        expanded_deposit_nodes.extend(mapped(node, cluster) for cluster in clusters)

    expanded_graph = GraphData(
        vehicles=graph.vehicles,
        nodes=next_node - 1,
        deposit_nodes=expanded_deposit_nodes,
        edge_count=len(expanded_edges),
        arc_count=len(expanded_arcs) + len(transitions),
        records=expanded_edges + expanded_arcs + transitions,
    )

    virtual_node_mapping: dict[int, int | None] = {}
    for expanded_index, original_index in enumerate(record_origins):
        expanded_pairs = virtual_node_pairs(expanded_index, expanded_graph.edge_count)
        if original_index is None:
            for pair in expanded_pairs:
                for node in pair:
                    virtual_node_mapping[node] = None
            continue
        original_pairs = virtual_node_pairs(original_index, graph.edge_count)
        for expanded_pair, original_pair in zip(expanded_pairs, original_pairs):
            virtual_node_mapping.update(zip(expanded_pair, original_pair))

    return Expansion(
        graph=expanded_graph,
        neutral_nodes=tuple(sorted(neutral)),
        node_copies=node_copies,
        transition_count=len(transitions),
        transition_clusters=tuple(transition_clusters),
        cluster_costs=dict(cluster_costs),
        record_clusters=record_clusters,
        record_origins=record_origins,
        expanded_record_clusters=expanded_record_clusters,
        virtual_node_mapping=virtual_node_mapping,
        original_node_copies={
            node: tuple(sorted(
                expanded for (original, _cluster), expanded in node_copies.items()
                if original == node
            )) or (node,)
            for node in range(1, graph.nodes + 1)
        },
    )


def virtual_node_pairs(
    record_index: int, edge_count: int
) -> tuple[tuple[int, int], ...]:
    """Return the one-based in/out pairs created by SuperGraph for a record."""
    if record_index < edge_count:
        first = 4 * record_index + 1
        return ((first, first + 1), (first + 2, first + 3))
    first = 4 * edge_count + 2 * (record_index - edge_count) + 1
    return ((first, first + 1),)


def tune_crossings(graph: GraphData, assignments: list[int]) -> GraphData:
    """Compatibility entry point for callers of the former cost tuner."""
    return expand_neutral_nodes(graph, assignments).graph


def format_number(value: Decimal) -> str:
    return str(value)


def write_graph(path: Path, graph: GraphData) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"{graph.vehicles} {graph.nodes} {len(graph.deposit_nodes)} "
        f"{graph.edge_count} {graph.arc_count}"
    ]
    lines.extend(str(node) for node in graph.deposit_nodes)
    lines.extend(
        f"{record.source} {record.target} {record.zone} "
        f"{format_number(record.cost)} {format_number(record.demand)}"
        for record in graph.records
    )
    try:
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def write_virtual_node_mapping(path: Path, mapping: dict[int, int | None]) -> None:
    """Write one-based expanded-to-original SuperGraph in/out node IDs."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["expanded_node original_node"]
    lines.extend(
        f"{expanded} {'-' if original is None else original}"
        for expanded, original in sorted(mapping.items())
    )
    try:
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def write_in_out_edges(path: Path, expansion: Expansion) -> None:
    """Write synthetic cluster-transition arcs using SuperGraph in/out IDs."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [IN_OUT_HEADER]
    transition_start = len(expansion.graph.records) - expansion.transition_count
    for offset, (source_cluster, target_cluster) in enumerate(
        expansion.transition_clusters
    ):
        record_index = transition_start + offset
        lines.extend(
            f"{source_cluster} {target_cluster} {in_node} {out_node}"
            for in_node, out_node in virtual_node_pairs(
                record_index, expansion.graph.edge_count
            )
        )
    try:
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def write_record_clusters(path: Path, expansion: Expansion) -> None:
    """Write the cluster layer of every expanded graph record."""
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [RECORD_CLUSTERS_HEADER]
    lines.extend(
        f"{record} {cluster or 0}"
        for record, cluster in enumerate(
            expansion.expanded_record_clusters, start=1
        )
    )
    try:
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def _expanded_record_clusters(expansion: Expansion) -> list[int | None]:
    """Return each expanded record's cluster, or None when neutral/synthetic."""
    return [
        None if origin is None else expansion.record_clusters[origin]
        for origin in expansion.record_origins
    ]


def _expansion_positions(
    expansion: Expansion,
    geometry: CoordinateGeometry | None,
    layout_edges: list[Edge],
    layout_clusters: list[int],
) -> dict[str, tuple[float, float]]:
    """Place expanded copies around their original geographic coordinate."""
    if geometry is None or not geometry.coordinates:
        node_clusters = primary_node_clusters(layout_edges, layout_clusters)
        return graph_layout(layout_edges, layout_clusters, node_clusters)

    original_positions = coordinate_layout(geometry)
    positions: dict[str, tuple[float, float]] = {}
    for original in range(1, geometry.node_count + 1):
        copies = expansion.original_node_copies[original]
        base_x, base_y = original_positions[str(original)]
        if len(copies) == 1:
            positions[str(copies[0])] = (base_x, base_y)
            continue
        radius = 7.0
        for index, expanded in enumerate(copies):
            angle = 2 * math.pi * index / len(copies) - math.pi / 2
            positions[str(expanded)] = (
                base_x + radius * math.cos(angle),
                base_y + radius * math.sin(angle),
            )
    return positions


def write_expansion_svg(
    path: Path,
    expansion: Expansion,
    geometry: CoordinateGeometry | None = None,
) -> None:
    """Draw cluster layers, neutral records/nodes and synthetic transitions."""
    graph = expansion.graph
    record_clusters = _expanded_record_clusters(expansion)
    node_layers = {
        expanded: cluster
        for (_original, cluster), expanded in expansion.node_copies.items()
    }
    layout_edges = [
        Edge(index + 1, str(record.source), str(record.target), 1, 0, 1)
        for index, record in enumerate(graph.records)
    ]
    layout_clusters = [
        cluster or node_layers.get(record.source) or node_layers.get(record.target) or 1
        for record, cluster in zip(graph.records, record_clusters)
    ]
    positions = _expansion_positions(
        expansion, geometry, layout_edges, layout_clusters
    )
    cluster_ids = tuple(sorted(expansion.cluster_costs))
    legend_rows = len(cluster_ids) + 3
    height = max(850, 115 + 29 * legend_rows)
    width = 1040
    svg = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        '<text x="40" y="35" font-family="sans-serif" font-size="22" '
        'font-weight="bold" fill="#111827">Grafo expandido por clusters</text>',
        '<text x="40" y="58" font-family="sans-serif" font-size="13" '
        'fill="#4b5563">Color: cluster · Gris discontinuo: arista neutra · '
        'Naranja punteado: transición sintética</text>',
        '<rect x="25" y="70" width="750" height="745" rx="10" '
        'fill="#f9fafb" stroke="#d1d5db"/>',
    ]

    if geometry is not None and geometry.coordinates:
        original_positions = coordinate_layout(geometry)
        for source, target in geometry.original_edges:
            x1, y1 = original_positions[source]
            x2, y2 = original_positions[target]
            svg.append(
                f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" '
                f'y2="{y2:.2f}" stroke="#d1d5db" stroke-width="1.2" '
                'opacity="0.8" data-map-background="true"/>'
            )

    parallel: dict[tuple[int, int], list[int]] = defaultdict(list)
    for index, record in enumerate(graph.records):
        parallel[tuple(sorted((record.source, record.target)))].append(index)
    parallel_rank = {
        index: (rank, len(indices))
        for indices in parallel.values()
        for rank, index in enumerate(indices)
    }
    transition_start = len(graph.records) - expansion.transition_count
    # Paint the numerous synthetic transitions first so the physical cluster
    # and neutral records remain readable above them.
    drawing_order = sorted(
        range(len(graph.records)), key=lambda index: index < transition_start
    )
    for index in drawing_order:
        record = graph.records[index]
        cluster = record_clusters[index]
        x1, y1 = positions[str(record.source)]
        x2, y2 = positions[str(record.target)]
        is_transition = index >= transition_start
        is_neutral = cluster is None and not is_transition
        if is_transition:
            color, dash, width_px, opacity, kind = (
                "#f59e0b", ' stroke-dasharray="4 5"', 1.7, 0.55,
                "transición sintética",
            )
        elif is_neutral:
            color, dash, width_px, opacity, kind = (
                "#6b7280", ' stroke-dasharray="9 6"', 3.2, 0.90, "arista neutra",
            )
        else:
            color, dash, width_px, opacity, kind = (
                cluster_color(cluster), "", 3.0, 0.84, f"cluster {cluster}",
            )
        rank, count = parallel_rank[index]
        dx, dy = x2 - x1, y2 - y1
        distance = max(math.hypot(dx, dy), 0.01)
        offset = (rank - (count - 1) / 2.0) * 13.0
        control_x = (x1 + x2) / 2.0 - dy / distance * offset
        control_y = (y1 + y2) / 2.0 + dx / distance * offset
        title = escape(
            f"Registro {index + 1}: {record.source} → {record.target}; {kind}"
        )
        svg.append(
            f'<path d="M {x1:.2f} {y1:.2f} Q {control_x:.2f} {control_y:.2f} '
            f'{x2:.2f} {y2:.2f}" fill="none" stroke="{color}" '
            f'stroke-width="{width_px}" opacity="{opacity}"{dash} '
            f'data-kind="{escape(kind)}"><title>{title}</title></path>'
        )

    neutral_expanded_nodes = set(expansion.node_copies.values())
    expanded_node_details = {
        expanded: (original, cluster)
        for (original, cluster), expanded in expansion.node_copies.items()
    }
    show_all_labels = graph.nodes <= 80
    for node in range(1, graph.nodes + 1):
        x, y = positions[str(node)]
        is_neutral = node in neutral_expanded_nodes
        radius = 7 if is_neutral else (5 if show_all_labels else 3)
        fill = "#f59e0b" if is_neutral else "#111827"
        node_title = f"Nodo {node}"
        if is_neutral:
            original, layer = expanded_node_details[node]
            node_title += f"; neutro; original {original}; cluster {layer}"
        svg.append(
            f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius}" fill="{fill}" '
            f'stroke="white" stroke-width="1.5" data-neutral="{str(is_neutral).lower()}">'
            f'<title>{escape(node_title)}</title></circle>'
        )
        if show_all_labels:
            label = str(node)
            if is_neutral:
                label += f" (N/C{expanded_node_details[node][1]})"
            svg.append(
                f'<text x="{x + 9:.2f}" y="{y - 8:.2f}" font-family="sans-serif" '
                f'font-size="11" fill="#111827">{escape(label)}</text>'
            )

    legend_y = 95
    for cluster in cluster_ids:
        color = cluster_color(cluster)
        svg.append(
            f'<line x1="805" y1="{legend_y}" x2="829" y2="{legend_y}" '
            f'stroke="{color}" stroke-width="5"/>'
        )
        svg.append(
            f'<text x="837" y="{legend_y + 4}" font-family="sans-serif" '
            f'font-size="13" fill="#111827">Cluster {cluster}</text>'
        )
        legend_y += 29
    svg.extend([
        f'<circle cx="817" cy="{legend_y}" r="7" fill="#f59e0b"/>',
        f'<text x="837" y="{legend_y + 4}" font-family="sans-serif" font-size="13" '
        'fill="#111827">Nodo neutro (N)</text>',
        f'<line x1="805" y1="{legend_y + 29}" x2="829" y2="{legend_y + 29}" '
        'stroke="#6b7280" stroke-width="3.2" stroke-dasharray="9 6"/>',
        f'<text x="837" y="{legend_y + 33}" font-family="sans-serif" font-size="13" '
        'fill="#111827">Arista neutra</text>',
        f'<line x1="805" y1="{legend_y + 58}" x2="829" y2="{legend_y + 58}" '
        'stroke="#f59e0b" stroke-width="2.4" stroke-dasharray="4 5"/>',
        f'<text x="837" y="{legend_y + 62}" font-family="sans-serif" font-size="13" '
        'fill="#111827">Transición</text>',
        '</svg>',
    ])
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        path.write_text("\n".join(svg) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def write_expansion_png(
    path: Path,
    expansion: Expansion,
    geometry: CoordinateGeometry | None = None,
) -> None:
    """Render the expansion preview without leaving an intermediate SVG."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".expanded-preview-", dir=path.parent) as directory:
        svg_path = Path(directory) / "preview.svg"
        write_expansion_svg(svg_path, expansion, geometry)
        write_png(svg_path, path)


def read_turns(path: Path, node_count: int) -> tuple[list[tuple[int, int, int]],
                                                       list[tuple[int, int, int]]]:
    try:
        tokens = path.read_text(encoding="utf-8").split()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if len(tokens) < 2:
        raise ValueError(f"{path}: cabecera incompleta")
    try:
        turn_count, illegal_count = map(int, tokens[:2])
    except ValueError as error:
        raise ValueError(f"{path}: cabecera invalida") from error
    if (turn_count < 0 or illegal_count < 0 or
            len(tokens) != 2 + 3 * (turn_count + illegal_count)):
        raise ValueError(f"{path}: cantidad de campos invalida")
    triples = []
    for offset in range(2, len(tokens), 3):
        try:
            triple = tuple(map(int, tokens[offset:offset + 3]))
        except ValueError as error:
            raise ValueError(f"{path}: giro invalido") from error
        if any(node < 1 or node > node_count for node in triple):
            raise ValueError(f"{path}: giro con nodo fuera de rango")
        triples.append(triple)
    return triples[:turn_count], triples[turn_count:]


def expand_turns(
    turns: list[tuple[int, int, int]],
    original_node_copies: dict[int, tuple[int, ...]],
) -> list[tuple[int, int, int]]:
    """Translate original turn triples to every combination of expanded copies."""
    expanded = set()
    for triple in turns:
        copies = (original_node_copies[node] for node in triple)
        expanded.update(product(*copies))
    return sorted(expanded)


def write_turns(path: Path, turns: list[tuple[int, int, int]],
                illegal_turns: list[tuple[int, int, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [f"{len(turns)} {len(illegal_turns)}"]
    lines.extend(" ".join(map(str, triple)) for triple in turns + illegal_turns)
    try:
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=("Expande las componentes neutras por cluster y agrega "
                     "transiciones con costo S_i + S_j.")
    )
    parser.add_argument("graph", type=Path, help="instancia .dat original")
    parser.add_argument("clusters", type=Path, help="clusters pre-solver del grafo")
    parser.add_argument("output", type=Path, help="instancia .dat expandida")
    parser.add_argument(
        "mapping", type=Path, nargs="?",
        help=("mapeo de nodos virtuales in/out; por defecto reemplaza "
              ".dat por .mapping.dat"),
    )
    parser.add_argument(
        "--in-out", type=Path,
        help=("transiciones in/out agrupadas por cluster de origen; por defecto "
              "reemplaza .dat por .in-out.dat"),
    )
    parser.add_argument(
        "--record-clusters", type=Path,
        help=("cluster de cada registro del grafo expandido; por defecto "
              "reemplaza .dat por .record-clusters.dat"),
    )
    parser.add_argument("--turns", type=Path, help="giros del grafo original")
    parser.add_argument(
        "--coords", type=Path,
        help="graph_N.coords.csv para ubicar la expansión sobre el mapa original",
    )
    parser.add_argument(
        "--expanded-turns", type=Path,
        help="salida de giros traducidos a los nodos expandidos",
    )
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_arguments(arguments)
    try:
        if options.graph.resolve() == options.output.resolve():
            raise ValueError("la salida debe ser distinta del grafo original")
        graph = read_graph(options.graph)
        geometry = (
            read_coordinate_geometry(options.graph, options.coords)
            if options.coords is not None else None
        )
        assignments = read_edge_clusters(options.clusters, graph)
        expansion = expand_neutral_nodes(graph, assignments)
        mapping_path = options.mapping or options.output.with_suffix(".mapping.dat")
        in_out_path = options.in_out or options.output.with_suffix(".in-out.dat")
        record_clusters_path = (
            options.record_clusters
            or options.output.with_suffix(".record-clusters.dat")
        )
        png_path = options.output.with_suffix(".png")
        if (options.turns is None) != (options.expanded_turns is None):
            raise ValueError("--turns y --expanded-turns deben usarse juntos")
        output_paths = [
            options.output, mapping_path, in_out_path, record_clusters_path,
            png_path,
        ]
        if options.expanded_turns is not None:
            output_paths.append(options.expanded_turns)
        resolved_outputs = [path.resolve() for path in output_paths]
        if (len(set(resolved_outputs)) != len(resolved_outputs) or
                options.graph.resolve() in resolved_outputs):
            raise ValueError("el grafo y cada salida deben usar archivos distintos")
        write_graph(options.output, expansion.graph)
        write_virtual_node_mapping(mapping_path, expansion.virtual_node_mapping)
        write_in_out_edges(in_out_path, expansion)
        write_record_clusters(record_clusters_path, expansion)
        write_expansion_png(png_path, expansion, geometry)
        if options.turns is not None:
            turns, illegal_turns = read_turns(options.turns, graph.nodes)
            write_turns(
                options.expanded_turns,
                expand_turns(turns, expansion.original_node_copies),
                expand_turns(illegal_turns, expansion.original_node_copies),
            )
        print(
            f"Grafo expandido guardado en {options.output}: "
            f"{len(expansion.neutral_nodes)} nodos neutros, "
            f"{expansion.transition_count} transiciones entre clusters; "
            f"mapeo guardado en {mapping_path}; transiciones in/out guardadas en "
            f"{in_out_path}; clusters de registros guardados en "
            f"{record_clusters_path}; vista guardada en {png_path}"
        )
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
