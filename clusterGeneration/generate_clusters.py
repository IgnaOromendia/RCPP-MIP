#!/usr/bin/env python3
"""Generate BFS/street partitions and project them onto solution passages."""

import argparse
import math
from pathlib import Path
import sys

try:
    from .bfs_impl import bfs_clusters
    from .divider_impl import straight_divider_clusters
    from .io_impl import (
        infer_node_count, read_coordinate_geometry, read_graph_clusters,
        read_neutral_nodes, read_osm_street_names, read_solution,
        write_clusters, write_neutral_nodes,
    )
    from .model import CoordinateGeometry, Edge
except ImportError:  # Direct execution from the repository root.
    from bfs_impl import bfs_clusters
    from divider_impl import straight_divider_clusters
    from io_impl import (
        infer_node_count, read_coordinate_geometry, read_graph_clusters,
        read_neutral_nodes, read_osm_street_names, read_solution,
        write_clusters, write_neutral_nodes,
    )
    from model import CoordinateGeometry, Edge


def _arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Clusteriza un grafo mediante BFS o calles divisorias.",
    )
    parser.add_argument("solution", type=Path, nargs="?")
    parser.add_argument("--graph", type=Path, required=True)
    parser.add_argument("--coords", type=Path)
    parser.add_argument("--percentage", type=float)
    parser.add_argument("--nodes", type=int)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--graph-only", action="store_true")
    parser.add_argument("--strategy", choices=("bfs", "streets"), default="streets")
    parser.add_argument("--graph-clusters", type=Path)
    parser.add_argument("--neutral-nodes", type=Path)
    parser.add_argument("--neutral-nodes-output", type=Path)
    parser.add_argument("--osm", type=Path)
    parser.add_argument("--node-mapping", type=Path)
    return parser.parse_args()


def _project(
    edges: list[Edge], geometry: CoordinateGeometry,
    graph_clusters: dict[int, int],
) -> list[int]:
    """Translate an already generated graph partition to solution arcs."""
    result = []
    for edge in edges:
        try:
            source = None if edge.source == "D" else geometry.virtual_to_edge[edge.source]
            target = None if edge.target == "D" else geometry.virtual_to_edge[edge.target]
        except KeyError as error:
            raise ValueError(f"nodo virtual inexistente: {error.args[0]}") from error
        original_edge = target if target is not None else source
        if original_edge is None:
            raise ValueError("arista virtual sin arista original adyacente")
        result.append(graph_clusters[original_edge])
    return result


def _graph_only(options: argparse.Namespace, geometry: CoordinateGeometry,
                node_count: int, output: Path) -> None:
    if options.solution is not None or options.graph_clusters is not None:
        raise ValueError("--graph-only no acepta solucion ni --graph-clusters")
    if options.percentage is None:
        raise ValueError("--graph-only requiere --percentage")
    neutral = (
        read_neutral_nodes(options.neutral_nodes, node_count)
        if options.neutral_nodes else {}
    )
    if (options.osm is None) != (options.node_mapping is None):
        raise ValueError("--osm y --node-mapping deben indicarse juntos")
    names = (
        read_osm_street_names(options.osm, options.node_mapping, geometry)
        if options.osm else None
    )
    edges, assignments, maximum, effective_neutral, divider_count = (
        bfs_clusters(geometry, options.percentage)
        if options.strategy == "bfs" else
        straight_divider_clusters(geometry, options.percentage, neutral, names)
    )
    required_count = sum(
        zone != 0 for zone in (geometry.zones or [1] * len(edges))
    )
    if options.neutral_nodes_output and options.strategy == "bfs":
        incident = {}
        for edge, cluster in zip(edges, assignments):
            for node in (int(edge.source), int(edge.target)):
                incident.setdefault(node, set()).add(cluster)
        effective_neutral = {node: ("cluster_boundary",) for node, clusters in incident.items()
                             if len(clusters) > 1}
    write_clusters(
        output, edges, assignments, options.percentage, required_count, maximum,
        len(effective_neutral), divider_count,
    )
    if options.neutral_nodes_output:
        write_neutral_nodes(options.neutral_nodes_output, effective_neutral, node_count)


def _solution(options: argparse.Namespace, geometry: CoordinateGeometry,
              output: Path) -> None:
    if options.solution is None or options.graph_clusters is None:
        raise ValueError("indique una solucion y --graph-clusters")
    graph_clusters, stored_percentage, maximum = read_graph_clusters(
        options.graph_clusters, geometry
    )
    percentage = options.percentage if options.percentage is not None else stored_percentage
    if percentage is None or not math.isfinite(percentage) or not 0 < percentage <= 100:
        raise ValueError("falta un porcentaje objetivo valido")
    edges = read_solution(options.solution)
    write_clusters(
        output, edges, _project(edges, geometry, graph_clusters), percentage,
        len(graph_clusters), maximum,
    )


def main() -> int:
    options = _arguments()
    try:
        geometry = read_coordinate_geometry(options.graph, options.coords)
        node_count = options.nodes
        if node_count is None:
            if options.graph_only:
                node_count = geometry.node_count
            elif options.solution is None:
                raise ValueError("indique una solucion y --graph-clusters")
            else:
                node_count = infer_node_count(options.solution)
        if node_count != geometry.node_count:
            raise ValueError("la cantidad de nodos del grafo no coincide con N")
        output = options.output or Path("data") / str(node_count) / f"clusters_{node_count}.dat"
        if options.graph_only:
            _graph_only(options, geometry, node_count, output)
        else:
            _solution(options, geometry, output)
        print(f"Clusters guardados en {output}")
        return 0
    except (KeyError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
