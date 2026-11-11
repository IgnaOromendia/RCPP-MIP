#!/usr/bin/env python3
"""Compare bestClusterPath with distance RCPP followed by cluster sorting."""

import argparse
from collections import defaultdict
import csv
from datetime import datetime
import math
from pathlib import Path
import shlex
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from clusterGeneration.io_impl import (read_coordinate_geometry, read_graph_clusters,
                                       read_neutral_nodes, write_neutral_nodes)

FIELDS = ["map", "sorter_crossings", "best_crossings", "reduction",
          "sorter_source", "best_optimal", "sorter_seconds", "best_seconds", "error"]


def run(command, directory, label, timeout):
    log = directory / f"{label}.log"
    command = [str(part) for part in command]
    print(f"  {label} (log: {log})", flush=True)
    with log.open("w", encoding="utf-8") as target:
        target.write(shlex.join(command) + "\n\n")
        target.flush()
        try:
            result = subprocess.run(command, cwd=directory, stdout=target,
                                    stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{label}: timeout de {timeout}s; ver {log}") from error
    if result.returncode:
        raise RuntimeError(f"{label}: exit {result.returncode}; ver {log}")
    return log.read_text(encoding="utf-8")


def result_fields(text, prefix):
    lines = [line for line in text.splitlines() if line.startswith(prefix + " ")]
    if not lines:
        raise ValueError(f"Falta {prefix} en el log")
    fields = dict(token.split("=", 1) for token in lines[-1].split()[1:])
    if fields.get("has_solution") != "true":
        raise ValueError(f"{prefix}: no hay solucion factible")
    return fields


def count_crossings(path):
    """Count physical cluster changes in route order, excluding the depot join."""
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    if not rows:
        raise ValueError(f"Ruta vacia: {path}")
    if {row["vehiculo"] for row in rows} != {"1"}:
        raise ValueError("La comparacion requiere un unico vehiculo (1)")
    rows.sort(key=lambda row: int(row["orden"]))
    positions = [int(row["orden"]) for row in rows]
    if len(set(positions)) != len(positions):
        raise ValueError(f"Posiciones duplicadas: {path}")
    clusters = [int(row["cluster"]) for row in rows]
    if min(clusters) < 1:
        raise ValueError(f"Cluster no positivo: {path}")
    return sum(left != right for left, right in zip(clusters, clusters[1:]))


def inputs(data_root, name, variant):
    directory = data_root / name / variant
    graphs = [path for path in directory.glob("graph_*.dat")
              if path.stem.removeprefix("graph_").isdigit()]
    if len(graphs) != 1:
        raise ValueError(f"Se esperaba un unico graph_N.dat en {directory}")
    graph = graphs[0]
    with graph.open(encoding="utf-8") as source:
        header = source.readline().split()
    if len(header) != 5:
        raise ValueError(f"Cabecera de grafo invalida: {graph}")
    nodes = int(header[1])
    if int(graph.stem.removeprefix("graph_")) != nodes:
        raise ValueError(f"Cantidad de nodos incompatible: {graph}")
    if int(header[0]) != 1:
        raise ValueError(f"Se requiere un vehiculo: {graph}")
    paths = {"graph": graph,
             "turns": directory / f"graph_{nodes}.turns.dat",
             "clusters": directory / f"graph_clusters_{nodes}.dat",
             "neutral": directory / f"graph_{nodes}.cluster-neutral-nodes.dat"}
    for path in paths.values():
        if not path.is_file():
            raise ValueError(f"Falta {path}; prepare primero el mapa y sus clusters")
    return nodes, paths


def prepare_partition(nodes, paths, directory):
    """Accept historical vehicle=1 only for records with no solution counts."""
    directory.mkdir(parents=True)
    normalized = directory / "graph_clusters.dat"
    lines = []
    converted = 0
    for line in paths["clusters"].read_text(encoding="utf-8").splitlines():
        fields = line.split()
        if (len(fields) == 8 and fields[4:] == ["1", "0", "0", "0"]):
            fields[4] = "0"
            line = " ".join(fields)
            converted += 1
        lines.append(line)
    normalized.write_text("\n".join(lines) + "\n", encoding="utf-8")
    # Keep the strict shared readers: reject wrong endpoints, missing/duplicate
    # records and actual solution passages instead of silently discarding them.
    geometry = read_coordinate_geometry(paths["graph"])
    assignments, percentage, _ = read_graph_clusters(normalized, geometry)
    if percentage is None:
        raise ValueError(f"Falta porcentaje_objetivo en {normalized}")
    neutral = read_neutral_nodes(paths["neutral"], nodes)
    incident = defaultdict(set)
    for edge_id, (source, target) in enumerate(geometry.original_edges):
        for node in (int(source), int(target)):
            incident[node].add(assignments[edge_id])
    missing = {node for node, clusters in incident.items()
               if len(clusters) > 1 and node not in neutral}
    for node in missing:
        neutral[node] = ("cluster_boundary",)
    neutral_path = directory / "neutral_nodes.dat"
    write_neutral_nodes(neutral_path, neutral, nodes)
    if missing:
        print(f"  Nodos neutros: se completan {len(missing)} fronteras omitidas; "
              f"copia: {neutral_path}", flush=True)
    if converted:
        print(f"  Particion antigua: vehiculo 1 -> 0 en {converted} registros sin pasadas; "
              f"copia: {normalized}", flush=True)
    return {**paths, "clusters": normalized, "neutral": neutral_path}


def compare(name, nodes, paths, output, binaries, timeout):
    row = dict.fromkeys(FIELDS, "")
    row["map"] = name
    errors = []
    print(f"\n{name}: {nodes} nodos", flush=True)
    for method in ("sorter", "best"):
        directory = output / name / method
        directory.mkdir(parents=True)
        started = time.monotonic()
        try:
            if method == "sorter":
                # Plain SuperGraph output is required by the sorter; the
                # --expanded-graph output uses contracted IDs and cannot be read.
                log = run([binaries / "solverExec", paths["graph"], paths["turns"],
                           "mip"], directory, "distance", timeout)
                result_fields(log, "RCPP_RESULT")
                solution = directory / "data" / str(nodes) / f"min_dist_{nodes}.dat"
                clusters = directory / "clusters.dat"
                run([sys.executable, ROOT / "clusterGeneration/generate_clusters.py",
                     solution, "--graph", paths["graph"], "--graph-clusters",
                     paths["clusters"], "--nodes", nodes, "--output", clusters],
                    directory, "project_clusters", timeout)
                segments = directory / "segments.csv"
                log = run([binaries / "pathSortClusterExec", paths["graph"], paths["turns"],
                           solution, clusters, directory / "order.dat", segments],
                          directory, "sort_clusters", timeout)
                row["sorter_crossings"] = count_crossings(segments)
                row["sorter_source"] = ("hierholzer" if
                    "se usa el orden factible de Hierholzer" in log else "mip")
            else:
                log = run([binaries / "bestClusterPathExec", paths["graph"], paths["turns"],
                           "--clusters", paths["clusters"], "--neutral-nodes", paths["neutral"],
                           "--output", directory / "solution.dat"],
                          directory, "bestClusterPath", timeout)
                fields = result_fields(log, "BEST_CLUSTER_PATH_RESULT")
                crossings = float(fields["crossings"])
                if not math.isfinite(crossings) or crossings < 0 or not crossings.is_integer():
                    raise ValueError(f"Cantidad de cruces invalida: {fields['crossings']}")
                row["best_crossings"] = int(crossings)
                row["best_optimal"] = fields["optimal"]
        except (OSError, ValueError, RuntimeError, KeyError) as error:
            errors.append(f"{method}: {error}")
            print(f"  ERROR: {error}", flush=True)
        row[f"{method}_seconds"] = f"{time.monotonic() - started:.3f}"
    if row["sorter_crossings"] != "" and row["best_crossings"] != "":
        row["reduction"] = row["sorter_crossings"] - row["best_crossings"]
    row["error"] = "; ".join(errors)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--maps", nargs="+", choices=("martinezA", "martinezB"),
                        default=["martinezA", "martinezB"])
    parser.add_argument("--data-root", type=Path, default=ROOT / "data")
    parser.add_argument("--variant", default="streets", help="subcarpeta de datos (default: streets)")
    parser.add_argument("--bin-dir", type=Path, default=ROOT)
    parser.add_argument("--output-dir", type=Path,
                        default=ROOT / "experiments/cluster_crossings" /
                        datetime.now().strftime("%Y%m%d_%H%M%S_%f"))
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--timeout", type=float,
                        help="limite de segundos por comando; sin limite por defecto")
    options = parser.parse_args()
    if options.timeout is not None and (not math.isfinite(options.timeout) or options.timeout <= 0):
        parser.error("--timeout debe ser positivo y finito")
    if len(set(options.maps)) != len(options.maps):
        parser.error("--maps contiene mapas repetidos")
    try:
        prepared = [(name, *inputs(options.data_root.resolve(), name, options.variant))
                    for name in options.maps]
        output = options.output_dir.resolve()
        output.mkdir(parents=True, exist_ok=False)
        prepared = [(name, nodes, prepare_partition(nodes, paths, output / name / "inputs"))
                    for name, nodes, paths in prepared]
        if not options.skip_build:
            binaries = options.bin_dir.resolve()
            binaries.mkdir(parents=True, exist_ok=True)
            run(["make", "-s", "-C", ROOT, "mip-best-cluster", "path-clusters",
                 f"BIN={binaries / 'solverExec'}",
                 f"BEST_CLUSTER_PATH_BIN={binaries / 'bestClusterPathExec'}",
                 f"PATH_CLUSTER_BIN={binaries / 'pathSortClusterExec'}"],
                output, "build", options.timeout)
        rows = []
        for name, nodes, paths in prepared:
            rows.append(compare(name, nodes, paths, output,
                                options.bin_dir.resolve(), options.timeout))
            with (output / "summary.csv").open("w", newline="", encoding="utf-8") as target:
                writer = csv.DictWriter(target, fieldnames=FIELDS)
                writer.writeheader()
                writer.writerows(rows)
        print("\nMapa        pathSort-clusters  bestClusterPath  Reduccion  Origen sorter")
        for row in rows:
            values = [str(row[key]) if row[key] != "" else "ERROR"
                      for key in ("sorter_crossings", "best_crossings", "reduction")]
            print(f"{row['map']:<12}{values[0]:>17}{values[1]:>17}{values[2]:>11}  "
                  f"{row['sorter_source'] or 'ERROR'}")
        print("Cruces: cambios entre pasadas fisicas consecutivas; sin cierre por deposito.")
        print("Reduccion positiva = menos cruces con bestClusterPath.")
        print(f"Detalle, tiempos y estado de cada solver: {output / 'summary.csv'}")
        return int(any(row["error"] for row in rows))
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
