#!/usr/bin/env python3
"""Raise the cost of graph records whose endpoints belong to different clusters."""

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass, replace
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
        except ValueError as error:
            raise ValueError(f"{path}: fila {line_number} invalida") from error
        if not 1 <= edge_id <= len(graph.records) or assignments[edge_id - 1] != 0:
            raise ValueError(f"{path}: identificador de arista invalido o duplicado")
        record = graph.records[edge_id - 1]
        if (source, target) != (record.source, record.target):
            raise ValueError(f"{path}: fila {line_number}: extremos no coinciden con el grafo")
        if cluster <= 0:
            raise ValueError(f"{path}: fila {line_number}: cluster debe ser positivo")
        assignments[edge_id - 1] = cluster
    if any(cluster == 0 for cluster in assignments):
        raise ValueError(f"{path}: faltan asignaciones para aristas del grafo")
    return assignments


def primary_node_clusters(graph: GraphData, assignments: list[int]) -> dict[int, int]:
    votes: dict[int, dict[int, int]] = defaultdict(lambda: defaultdict(int))
    for record, cluster in zip(graph.records, assignments):
        votes[record.source][cluster] += 1
        votes[record.target][cluster] += 1
    return {
        node: min(counts, key=lambda cluster: (-counts[cluster], cluster))
        for node, counts in votes.items()
    }


def tune_crossings(graph: GraphData,
                   assignments: list[int]) -> tuple[GraphData, list[int], Decimal]:
    if len(assignments) != len(graph.records):
        raise ValueError("la cantidad de clusters no coincide con el grafo")
    node_clusters = primary_node_clusters(graph, assignments)
    maximum_cost = max(record.cost for record in graph.records)
    crossing_cost = maximum_cost * Decimal(100)
    if not crossing_cost.is_finite():
        raise ValueError("el doble del costo maximo no es finito")
    crossing_ids = [
        index
        for index, record in enumerate(graph.records, start=1)
        if node_clusters[record.source] != node_clusters[record.target]
    ]
    crossing_set = set(crossing_ids)
    records = [
        replace(record, cost=crossing_cost) if index in crossing_set else record
        for index, record in enumerate(graph.records, start=1)
    ]
    return replace(graph, records=records), crossing_ids, crossing_cost


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


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=("Duplica el costo maximo del grafo sobre las aristas cuyos "
                     "extremos pertenecen a clusters distintos.")
    )
    parser.add_argument("graph", type=Path, help="instancia .dat original")
    parser.add_argument("clusters", type=Path, help="clusters pre-solver del grafo")
    parser.add_argument("output", type=Path, help="instancia .dat tuneada")
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_arguments(arguments)
    try:
        if options.graph.resolve() == options.output.resolve():
            raise ValueError("la salida debe ser distinta del grafo original")
        graph = read_graph(options.graph)
        assignments = read_edge_clusters(options.clusters, graph)
        tuned, crossing_ids, crossing_cost = tune_crossings(graph, assignments)
        write_graph(options.output, tuned)
        print(
            f"Grafo tuneado guardado en {options.output}: "
            f"{len(crossing_ids)} cruces con costo {format_number(crossing_cost)}"
        )
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
