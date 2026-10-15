#!/usr/bin/env python3
"""Expand neutral cluster boundaries before solving an RCPP instance.

A neutral component is copied once for every cluster that can reach it. Moving
inside one copy keeps the physical graph costs. Changing from the copy for
cluster ``i`` to the copy for cluster ``j`` uses a synthetic directed arc whose
cost is the sum of all original edge costs in both clusters.
"""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
from pathlib import Path
import sys


CLUSTER_HEADER = "arista cluster origen destino vehiculo servicio recorridos pasadas"


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
    cluster_costs: dict[int, Decimal]
    record_origins: tuple[int | None, ...]
    virtual_node_mapping: dict[int, int | None]


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

    cluster_costs: dict[int, Decimal] = defaultdict(Decimal)
    for record, cluster in zip(graph.records, assignments):
        cluster_costs[cluster] += record.cost

    neutral, components = _neutral_components(graph, assignments)
    component_for_node = {
        node: component_index
        for component_index, component in enumerate(components)
        for node in component
    }
    component_clusters: list[set[int]] = [set() for _ in components]
    for record, cluster in zip(graph.records, assignments):
        if record.source in neutral:
            component_clusters[component_for_node[record.source]].add(cluster)
        if record.target in neutral:
            component_clusters[component_for_node[record.target]].add(cluster)

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
    for record_index, (record, cluster) in enumerate(zip(graph.records, assignments)):
        destination = expanded_edges if record_index < graph.edge_count else expanded_arcs
        origin_destination = edge_origins if record_index < graph.edge_count else arc_origins
        both_neutral = record.source in neutral and record.target in neutral
        if both_neutral:
            component_index = component_for_node[record.source]
            if component_index != component_for_node[record.target]:
                raise ValueError("una arista neutra conecta componentes distintas")
            for layer in sorted(component_clusters[component_index]):
                required_copy = layer == cluster
                destination.append(GraphRecord(
                    mapped(record.source, layer), mapped(record.target, layer),
                    record.zone if required_copy else 0,
                    record.cost,
                    record.demand if required_copy else Decimal(0),
                ))
                origin_destination.append(record_index)
        else:
            destination.append(GraphRecord(
                mapped(record.source, cluster), mapped(record.target, cluster),
                record.zone, record.cost, record.demand,
            ))
            origin_destination.append(record_index)

    transitions: list[GraphRecord] = []
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

    record_origins = tuple(edge_origins + arc_origins + [None] * len(transitions))

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

    def virtual_nodes(record_index: int, edge_count: int) -> tuple[tuple[int, int], ...]:
        """Return the one-based in/out node pairs created by SuperGraph."""
        if record_index < edge_count:
            first = 4 * record_index + 1
            return ((first, first + 1), (first + 2, first + 3))
        first = 4 * edge_count + 2 * (record_index - edge_count) + 1
        return ((first, first + 1),)

    virtual_node_mapping: dict[int, int | None] = {}
    for expanded_index, original_index in enumerate(record_origins):
        expanded_pairs = virtual_nodes(expanded_index, expanded_graph.edge_count)
        if original_index is None:
            for pair in expanded_pairs:
                for node in pair:
                    virtual_node_mapping[node] = None
            continue
        original_pairs = virtual_nodes(original_index, graph.edge_count)
        for expanded_pair, original_pair in zip(expanded_pairs, original_pairs):
            virtual_node_mapping.update(zip(expanded_pair, original_pair))

    return Expansion(
        graph=expanded_graph,
        neutral_nodes=tuple(sorted(neutral)),
        node_copies=node_copies,
        transition_count=len(transitions),
        cluster_costs=dict(cluster_costs),
        record_origins=record_origins,
        virtual_node_mapping=virtual_node_mapping,
    )


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
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_arguments(arguments)
    try:
        if options.graph.resolve() == options.output.resolve():
            raise ValueError("la salida debe ser distinta del grafo original")
        graph = read_graph(options.graph)
        assignments = read_edge_clusters(options.clusters, graph)
        expansion = expand_neutral_nodes(graph, assignments)
        mapping_path = options.mapping or options.output.with_suffix(".mapping.dat")
        if mapping_path.resolve() in {options.graph.resolve(), options.output.resolve()}:
            raise ValueError("el mapeo debe escribirse en un archivo distinto")
        write_graph(options.output, expansion.graph)
        write_virtual_node_mapping(mapping_path, expansion.virtual_node_mapping)
        print(
            f"Grafo expandido guardado en {options.output}: "
            f"{len(expansion.neutral_nodes)} nodos neutros, "
            f"{expansion.transition_count} transiciones entre clusters; "
            f"mapeo guardado en {mapping_path}"
        )
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
