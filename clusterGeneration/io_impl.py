"""Parsing and serialization implementation for cluster data."""

from __future__ import annotations

from collections import defaultdict
import csv
from pathlib import Path
import math
import xml.etree.ElementTree as ET

try:
    from .model import *
except ImportError:
    from model import *

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

def write_clusters(path: Path, edges: list[Edge], assignments: list[int], percentage: float,
                   clustered_edge_count: int | None = None,
                   maximum_cluster_size: int | None = None,
                   neutral_node_count: int = 0, divider_count: int = 0) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    clustered_edge_count = len(edges) if clustered_edge_count is None else clustered_edge_count
    target_size = max(1, math.floor(clustered_edge_count * percentage / 100.0))
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
            output.write(f"# max_aristas_por_cluster {maximum_cluster_size}\n")
            output.write(f"# nodos_neutros_forzados {neutral_node_count}\n")
            output.write(f"# caminos_divisores {divider_count}\n")
            output.write("arista cluster origen destino vehiculo servicio recorridos pasadas\n")
            for edge, cluster in zip(edges, assignments):
                output.write(
                    f"{edge.index} {cluster} {edge.source} {edge.target} {edge.vehicle} "
                    f"{edge.service_count} {edge.traversal_count} {edge.passages}\n"
                )
    except OSError as error:
        raise ValueError(f"no se pudo escribir {path}: {error}") from error

def infer_node_count(solution_path: Path) -> int:
    match = OUTPUT_NAME.fullmatch(solution_path.name)
    if match is None:
        raise ValueError(
            "no se pudo inferir N: use min_dist_N.dat o indique --nodes N"
        )
    return int(match.group(1))

