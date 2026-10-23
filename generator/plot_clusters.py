#!/usr/bin/env python3
"""Visualiza clusters viales sobre un mapa Folium interactivo."""

from __future__ import annotations

import argparse
from collections import Counter
from html import escape
from pathlib import Path
import sys
import tempfile

import folium  # type: ignore


CLUSTER_COLORS = (
    "#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c", "#0891b2",
    "#db2777", "#65a30d", "#4f46e5", "#ca8a04", "#0f766e", "#c026d3",
    "#0369a1", "#be123c", "#15803d", "#7e22ce", "#c2410c", "#0e7490",
    "#a21caf", "#4d7c0f", "#4338ca", "#a16207", "#047857", "#9d174d",
)


def cluster_color(cluster: int) -> str:
    if cluster < 1:
        raise ValueError("el cluster debe ser positivo")
    return CLUSTER_COLORS[(cluster - 1) % len(CLUSTER_COLORS)]


def read_coordinates(path: Path) -> dict[int, tuple[float, float]]:
    try:
        rows = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if not rows or rows[0].strip() != "node_id,x,y":
        raise ValueError(f"{path}: cabecera de coordenadas invalida")
    coordinates = {}
    for line_number, row in enumerate(rows[1:], start=2):
        if not row.strip():
            continue
        fields = row.split(",")
        if len(fields) != 3:
            raise ValueError(f"{path}: linea {line_number}: coordenada invalida")
        try:
            node = int(fields[0])
            latitude, longitude = float(fields[1]), float(fields[2])
        except ValueError as error:
            raise ValueError(
                f"{path}: linea {line_number}: coordenada invalida"
            ) from error
        if (node <= 0 or node in coordinates or not -90 <= latitude <= 90
                or not -180 <= longitude <= 180):
            raise ValueError(f"{path}: linea {line_number}: coordenada fuera de rango")
        coordinates[node] = (latitude, longitude)
    if not coordinates:
        raise ValueError(f"{path}: no contiene coordenadas")
    return coordinates


def read_clusters(path: Path) -> list[tuple[int, int, int, int]]:
    expected = "arista cluster origen destino vehiculo servicio recorridos pasadas"
    try:
        rows = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    header_found = False
    result = []
    seen = set()
    for line_number, raw_row in enumerate(rows, start=1):
        row = raw_row.strip()
        if not row or row.startswith("#"):
            continue
        if not header_found:
            if row != expected:
                raise ValueError(f"{path}: linea {line_number}: cabecera invalida")
            header_found = True
            continue
        fields = row.split()
        if len(fields) != 8:
            raise ValueError(f"{path}: linea {line_number}: cluster invalido")
        try:
            edge, cluster, source, target = map(int, fields[:4])
        except ValueError as error:
            raise ValueError(
                f"{path}: linea {line_number}: cluster invalido"
            ) from error
        if min(edge, cluster, source, target) <= 0 or edge in seen:
            raise ValueError(f"{path}: linea {line_number}: cluster fuera de rango")
        seen.add(edge)
        result.append((edge, cluster, source, target))
    if not header_found or not result:
        raise ValueError(f"{path}: no contiene clusters")
    return result


def read_required_edge_ids(path: Path) -> set[int]:
    """Read record zones and return only required graph records (zone != 0)."""
    try:
        tokens = path.read_text(encoding="utf-8").split()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if len(tokens) < 5:
        raise ValueError(f"{path}: cabecera incompleta")
    try:
        _vehicles, _nodes, deposit_count, edge_count, arc_count = map(
            int, tokens[:5]
        )
    except ValueError as error:
        raise ValueError(f"{path}: cabecera invalida") from error
    record_count = edge_count + arc_count
    offset = 5 + deposit_count
    if (deposit_count < 0 or edge_count < 0 or arc_count < 0
            or len(tokens) != offset + 5 * record_count):
        raise ValueError(f"{path}: cantidad de campos invalida")
    required = set()
    for position in range(record_count):
        try:
            zone = int(tokens[offset + 5 * position + 2])
        except ValueError as error:
            raise ValueError(
                f"{path}: zona invalida en la arista {position + 1}"
            ) from error
        if zone != 0:
            required.add(position + 1)
    if not required:
        raise ValueError(f"{path}: no contiene aristas obligatorias")
    return required


def read_neutral_nodes(path: Path) -> dict[int, tuple[str, ...]]:
    try:
        rows = [row.strip() for row in path.read_text(encoding="utf-8").splitlines()
                if row.strip()]
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if not rows or rows[0] != "node_id reasons":
        raise ValueError(f"{path}: cabecera de nodos neutros invalida")
    result = {}
    for line_number, row in enumerate(rows[1:], start=2):
        fields = row.split()
        if len(fields) != 2:
            raise ValueError(f"{path}: linea {line_number}: nodo neutro invalido")
        try:
            node = int(fields[0])
        except ValueError as error:
            raise ValueError(
                f"{path}: linea {line_number}: nodo neutro invalido"
            ) from error
        reasons = tuple(sorted(set(fields[1].split(","))))
        if node <= 0 or node in result or any(not reason for reason in reasons):
            raise ValueError(f"{path}: linea {line_number}: nodo neutro invalido")
        result[node] = reasons
    return result


def _legend(cluster_sizes: Counter[int]) -> str:
    items = "".join(
        f'<div style="display:flex;align-items:center;gap:7px;margin:3px 0">'
        f'<span style="width:18px;height:4px;background:{cluster_color(cluster)};'
        f'display:inline-block"></span>Cluster {cluster} ({size} tramos)</div>'
        for cluster, size in sorted(cluster_sizes.items())
    )
    return (
        '<div style="position:fixed;z-index:9999;right:12px;top:12px;'
        'max-height:45vh;overflow:auto;background:rgba(255,255,255,.94);'
        'border:1px solid #94a3b8;border-radius:8px;padding:10px 12px;'
        'font:12px/1.25 sans-serif;box-shadow:0 2px 10px #0002">'
        '<div style="font-weight:700;margin-bottom:6px">Clusters</div>'
        f'{items}'
        '<div style="display:flex;align-items:center;gap:7px;margin-top:7px;'
        'padding-top:7px;border-top:1px solid #cbd5e1">'
        '<span style="width:10px;height:10px;border-radius:50%;background:#f59e0b;'
        'border:1px solid #92400e;display:inline-block"></span>Nodo neutro</div>'
        '<div style="display:flex;align-items:center;gap:7px;margin-top:5px">'
        '<span style="width:18px;border-top:4px solid #64748b;display:inline-block">'
        '</span>Frontera vial original</div>'
        '<div style="display:flex;align-items:center;gap:7px;margin-top:5px">'
        '<span style="width:18px;border-top:5px dashed #111827;display:inline-block">'
        '</span>Calle divisoria elegida</div>'
        '</div>'
    )


def build_map(
    coordinates: dict[int, tuple[float, float]],
    clusters: list[tuple[int, int, int, int]],
    neutral_nodes: dict[int, tuple[str, ...]],
) -> folium.Map:
    used_nodes = {node for _, _, source, target in clusters for node in (source, target)}
    neutral_nodes = {
        node: reasons for node, reasons in neutral_nodes.items()
        if node in used_nodes
    }
    missing = sorted(used_nodes - coordinates.keys())
    if missing:
        raise ValueError(f"faltan coordenadas para el nodo {missing[0]}")
    missing_neutral = sorted(neutral_nodes.keys() - coordinates.keys())
    if missing_neutral:
        raise ValueError(f"falta coordenada para el nodo neutro {missing_neutral[0]}")

    latitude = sum(coordinates[node][0] for node in used_nodes) / len(used_nodes)
    longitude = sum(coordinates[node][1] for node in used_nodes) / len(used_nodes)
    cluster_map = folium.Map(
        location=(latitude, longitude), zoom_start=14,
        tiles="OpenStreetMap", prefer_canvas=True, control_scale=True,
    )
    line_layer = folium.FeatureGroup(name="Clusters", show=True)
    boundary_layer = folium.FeatureGroup(name="Fronteras viales", show=True)
    divider_layer = folium.FeatureGroup(name="Calles divisorias", show=True)
    neutral_layer = folium.FeatureGroup(name="Nodos neutros", show=True)
    cluster_sizes = Counter()

    for edge, cluster, source, target in clusters:
        source_reasons = set(neutral_nodes.get(source, ()))
        target_reasons = set(neutral_nodes.get(target, ()))
        neutral_corridor = bool(source_reasons and target_reasons)
        divider = (
            "divider" in source_reasons and "divider" in target_reasons
        )
        if divider:
            folium.PolyLine(
                locations=(coordinates[source], coordinates[target]),
                color="#111827", weight=5, opacity=0.95, dash_array="8 6",
                tooltip=(f"Calle divisoria · Tramo {edge} · "
                         f"{source} → {target}"),
            ).add_to(divider_layer)
        elif neutral_corridor:
            folium.PolyLine(
                locations=(coordinates[source], coordinates[target]),
                color="#64748b", weight=4, opacity=0.82,
                tooltip=(f"Frontera vial · Tramo {edge} · "
                         f"{source} → {target}"),
            ).add_to(boundary_layer)
        else:
            cluster_sizes[cluster] += 1
            folium.PolyLine(
                locations=(coordinates[source], coordinates[target]),
                color=cluster_color(cluster), weight=3, opacity=0.78,
                tooltip=(f"Cluster {cluster} · Tramo {edge} · "
                         f"{source} → {target}"),
            ).add_to(line_layer)

    for node, reasons in sorted(neutral_nodes.items()):
        reason_text = ", ".join(reason.replace("_", " ") for reason in reasons)
        is_divider = "divider" in reasons
        folium.CircleMarker(
            location=coordinates[node], radius=4,
            color="#111827" if is_divider else "#92400e", weight=1,
            fill=True, fill_color="#facc15" if is_divider else "#f59e0b",
            fill_opacity=0.92,
            tooltip=f"Nodo neutro {node} · {reason_text}",
            popup=f"<b>Nodo neutro {node}</b><br>{escape(reason_text)}",
        ).add_to(neutral_layer)

    line_layer.add_to(cluster_map)
    boundary_layer.add_to(cluster_map)
    divider_layer.add_to(cluster_map)
    neutral_layer.add_to(cluster_map)
    folium.LayerControl(collapsed=False).add_to(cluster_map)
    cluster_map.get_root().html.add_child(folium.Element(_legend(cluster_sizes)))
    bounds = [coordinates[node] for node in used_nodes]
    cluster_map.fit_bounds(bounds, padding=(20, 20))
    return cluster_map


def save_map(cluster_map: folium.Map, path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_name = None
    try:
        with tempfile.NamedTemporaryFile(
            suffix=".html", dir=path.parent, delete=False
        ) as temporary:
            temporary_name = Path(temporary.name)
        cluster_map.save(str(temporary_name))
        temporary_name.replace(path)
    finally:
        if temporary_name is not None:
            temporary_name.unlink(missing_ok=True)
    return path


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(
        description="Genera un mapa HTML interactivo de clusters OSM."
    )
    parser.add_argument("--clusters", type=Path, required=True)
    parser.add_argument("--graph", type=Path, required=True)
    parser.add_argument("--coords", type=Path, required=True)
    parser.add_argument("--neutral-nodes", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args(arguments)


def main(arguments=None) -> int:
    options = parse_arguments(arguments)
    try:
        coordinates = read_coordinates(options.coords)
        required_edge_ids = read_required_edge_ids(options.graph)
        clusters = [
            row for row in read_clusters(options.clusters)
            if row[0] in required_edge_ids
        ]
        if not clusters:
            raise ValueError("el archivo de clusters no contiene aristas obligatorias")
        neutral_nodes = read_neutral_nodes(options.neutral_nodes)
        output = save_map(
            build_map(coordinates, clusters, neutral_nodes), options.output
        )
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    print(f"Mapa interactivo de clusters guardado en {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
