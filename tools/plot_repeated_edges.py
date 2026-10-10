#!/usr/bin/env python3
"""Plot original graph edges, highlighting repeated passages in red."""

from __future__ import annotations

import argparse
from collections import defaultdict
from html import escape
import math
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "clusterGeneration"))

from generate_clusters import (  # noqa: E402
    CoordinateGeometry,
    Edge,
    coordinate_layout,
    graph_layout,
    read_coordinate_geometry,
    read_solution,
    write_png,
)


BACKGROUND_COLOR = "#d1d5db"
SINGLE_COLOR = "#2563eb"
REPEATED_COLOR = "#dc2626"


def passages_by_original_edge(
    edges: list[Edge], geometry: CoordinateGeometry
) -> dict[int, int]:
    """Sum X + Y over every orientation and vehicle of each original edge."""
    passages: dict[int, int] = defaultdict(int)
    for edge in edges:
        if edge.source == "D" or edge.target == "D":
            continue
        try:
            source_edge = geometry.virtual_to_edge[edge.source]
            target_edge = geometry.virtual_to_edge[edge.target]
        except KeyError as error:
            raise ValueError(
                f"nodo virtual inexistente en el grafo: {error.args[0]}"
            ) from error

        # A road super-arc has two virtual endpoints belonging to the same
        # original edge. Turn connectors join different original edges and do
        # not represent a road segment, so they must not add passages here.
        if source_edge == target_edge:
            passages[source_edge] += edge.passages
    return dict(passages)


def original_graph_layout(
    geometry: CoordinateGeometry,
) -> dict[str, tuple[float, float]]:
    if geometry.coordinates:
        return coordinate_layout(geometry)

    layout_edges = [
        Edge(index + 1, source, target, 0, 0, 0)
        for index, (source, target) in enumerate(geometry.original_edges)
    ]
    if not layout_edges:
        raise ValueError("el grafo original no contiene aristas para dibujar")
    nodes = {node for edge in layout_edges for node in (edge.source, edge.target)}
    return graph_layout(layout_edges, [1] * len(layout_edges), {node: 1 for node in nodes})


def write_svg(
    path: Path,
    geometry: CoordinateGeometry,
    passages: dict[int, int],
) -> None:
    positions = original_graph_layout(geometry)
    repeated_count = sum(value > 1 for value in passages.values())
    active_count = sum(value > 0 for value in passages.values())
    svg = [
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 980 850">',
        '<rect width="980" height="850" fill="white"/>',
        '<text x="40" y="35" font-family="sans-serif" font-size="22" '
        'font-weight="bold" fill="#111827">Pasadas por arista original</text>',
        '<text x="40" y="58" font-family="sans-serif" font-size="13" '
        'fill="#4b5563">Rojo: más de una pasada · Azul: una pasada · Gris: sin pasar</text>',
        '<rect x="25" y="70" width="750" height="745" rx="10" '
        'fill="#f9fafb" stroke="#d1d5db"/>',
    ]

    # Draw inactive edges first so active and repeated edges remain visible at
    # intersections and when the input contains parallel edges.
    draw_order = sorted(
        range(len(geometry.original_edges)),
        key=lambda edge_id: (passages.get(edge_id, 0) > 0,
                             passages.get(edge_id, 0) > 1, edge_id),
    )
    repeated_labels: list[tuple[float, float, int]] = []
    for edge_id in draw_order:
        source, target = geometry.original_edges[edge_id]
        x1, y1 = positions[source]
        x2, y2 = positions[target]
        count = passages.get(edge_id, 0)
        edge_class = "repeated" if count > 1 else ("single" if count == 1 else "unused")
        color = REPEATED_COLOR if count > 1 else (SINGLE_COLOR if count == 1 else BACKGROUND_COLOR)
        width = 2.5 + min(4.5, math.log2(count)) if count > 0 else 1.2
        opacity = 0.92 if count > 1 else (0.78 if count == 1 else 0.72)
        title = escape(
            f"Arista {edge_id + 1}: {source} - {target}; pasadas totales {count}"
        )
        svg.append(
            f'<line class="{edge_class}" data-edge-id="{edge_id}" '
            f'data-passages="{count}" x1="{x1:.2f}" y1="{y1:.2f}" '
            f'x2="{x2:.2f}" y2="{y2:.2f}" stroke="{color}" '
            f'stroke-width="{width:.2f}" stroke-linecap="round" opacity="{opacity}">'
            f'<title>{title}</title></line>'
        )
        if count > 1:
            repeated_labels.append(((x1 + x2) / 2.0, (y1 + y2) / 2.0, count))

    show_all_labels = geometry.node_count <= 80
    node_radius = 5 if show_all_labels else 2.5
    for node in sorted(positions, key=lambda value: int(value) if value != "D" else -1):
        if node == "D":
            continue
        x, y = positions[node]
        svg.append(
            f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{node_radius}" '
            'fill="#111827" stroke="white" stroke-width="1"/>'
        )
        if show_all_labels:
            svg.append(
                f'<text x="{x + 8:.2f}" y="{y - 7:.2f}" font-family="sans-serif" '
                f'font-size="11" fill="#111827">{escape(node)}</text>'
            )

    for x, y, count in repeated_labels:
        label = f"×{count}"
        label_width = 16 + 7 * len(str(count))
        svg.append(
            f'<rect x="{x - label_width / 2:.2f}" y="{y - 11:.2f}" '
            f'width="{label_width}" height="18" rx="8" fill="white" '
            'stroke="#dc2626" stroke-width="1"/>'
        )
        svg.append(
            f'<text x="{x:.2f}" y="{y + 2.5:.2f}" text-anchor="middle" '
            'font-family="sans-serif" font-size="11" font-weight="bold" '
            f'fill="#991b1b">{label}</text>'
        )

    legend = [
        (REPEATED_COLOR, "Más de una pasada"),
        (SINGLE_COLOR, "Una pasada"),
        (BACKGROUND_COLOR, "Sin pasar"),
    ]
    for index, (color, label) in enumerate(legend):
        y = 105 + index * 34
        svg.append(
            f'<line x1="805" y1="{y}" x2="835" y2="{y}" stroke="{color}" '
            f'stroke-width="{5 if index == 0 else 3}" stroke-linecap="round"/>'
        )
        svg.append(
            f'<text x="845" y="{y + 4}" font-family="sans-serif" '
            f'font-size="13" fill="#111827">{label}</text>'
        )
    svg.append(
        '<text x="805" y="225" font-family="sans-serif" font-size="13" '
        f'fill="#374151">Aristas activas: {active_count}</text>'
    )
    svg.append(
        '<text x="805" y="249" font-family="sans-serif" font-size="13" '
        f'fill="#374151">Repetidas: {repeated_count}</text>'
    )
    svg.append('</svg>')

    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        path.write_text("\n".join(svg) + "\n", encoding="utf-8")
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error


def output_paths(output: Path | None, node_count: int) -> tuple[Path, Path]:
    svg_path = output or Path("data") / str(node_count) / f"repeated_edges_{node_count}.svg"
    if svg_path.suffix == "":
        svg_path = svg_path.with_suffix(".svg")
    elif svg_path.suffix.lower() != ".svg":
        raise ValueError("--output debe terminar en .svg o no tener extension")
    return svg_path, svg_path.with_suffix(".png")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Dibuja el grafo original y marca en rojo las aristas con mas de una "
            "pasada total (X + Y)."
        )
    )
    parser.add_argument("solution", type=Path, help="archivo data/N/min_dist_N.dat")
    parser.add_argument("--graph", type=Path, required=True,
                        help="instancia original data/N/graph_N.dat")
    parser.add_argument("--coords", type=Path,
                        help="CSV node_id,x,y para conservar la geometria original")
    parser.add_argument("--output", type=Path,
                        help="ruta SVG (por defecto data/N/repeated_edges_N.svg)")
    return parser.parse_args()


def main() -> int:
    options = parse_arguments()
    try:
        geometry = read_coordinate_geometry(options.graph, options.coords)
        edges = read_solution(options.solution)
        passages = passages_by_original_edge(edges, geometry)
        svg_path, png_path = output_paths(options.output, geometry.node_count)
        write_svg(svg_path, geometry, passages)
        write_png(svg_path, png_path)
        repeated_count = sum(value > 1 for value in passages.values())
        print(
            f"Plot guardado en {svg_path} y {png_path} "
            f"({repeated_count} aristas con mas de una pasada)"
        )
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
