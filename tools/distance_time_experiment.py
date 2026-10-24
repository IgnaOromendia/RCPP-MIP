#!/usr/bin/env python3
"""Experimento 1: tiempos de modelos de distancia sobre mapas OSM."""

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

from experiment_utils import ROOT, SELECTION_STRATEGIES, parse_experiment_name, write_log


MAPS = ("acassusoA", "acassusoB", "martinezA", "martinezB")
MODELS = ("mip", *(f"fixAndOptimize:{s}" for s in SELECTION_STRATEGIES),
          "bestClusterPath")
FIELDS = ["map", "clustering", "percentage", "nodes", "edges", "required_edges",
          "model", "repetition", "signature", "elapsed_ms", "wall_ms",
          "has_solution", "optimal", "returncode", "outcome"]
RESULT = re.compile(
    r"^(RCPP_RESULT|BEST_CLUSTER_PATH_RESULT) elapsed_ms=(\S+) "
    r"has_solution=(true|false) optimal=(true|false) objective=\S+(?: .*)?$",
    re.MULTILINE,
)


def run_command(command, directory, log):
    result = subprocess.run([str(part) for part in command], cwd=directory,
                            capture_output=True, text=True)
    write_log(log, json.dumps([str(part) for part in command]),
              result.stdout, result.stderr)
    if result.returncode != 0:
        raise RuntimeError(f"Fallo de preparacion (exit {result.returncode}); ver {log}")
    return result


def source_paths(data_root, name):
    return {"graph": data_root / "input" / f"{name}.dat",
            "turns": data_root / "input" / f"{name}.turns.dat",
            "coordinates": data_root / "coordinates" / f"nodes_{name}.dat",
            "mapping": data_root / "mappings" / f"nodes_{name}.dat",
            "neutral": data_root / "mappings" / f"neutral_nodes_{name}.dat",
            "osm": data_root / "osm" / f"{name}.osm"}


def prepare_instance(source, directory, clustering, percentage):
    """Prepare once per map/partition; every model receives identical inputs."""
    directory.mkdir(parents=True, exist_ok=True)
    graph, turns = directory / "graph.dat", directory / "turns.dat"
    shutil.copyfile(source["graph"], graph)
    shutil.copyfile(source["turns"], turns)
    header = list(map(int, graph.read_text().splitlines()[0].split()))
    _, nodes, deposits, undirected, directed = header
    tokens = graph.read_text().split()[5 + deposits:]
    edge_count = undirected + directed
    if len(tokens) != 5 * edge_count:
        raise ValueError(f"Cantidad de aristas incompatible en {graph}")
    required = sum(int(tokens[index * 5 + 2]) != 0 for index in range(edge_count))
    mapping = dict(line.split() for line in source["mapping"].read_text().splitlines()
                   if line.strip())
    coordinates = directory / "coords.csv"
    with coordinates.open("w", newline="", encoding="utf-8") as target:
        writer = csv.writer(target)
        writer.writerow(["node_id", "x", "y"])
        for line in source["coordinates"].read_text().splitlines():
            if line.strip():
                node, x, y = line.split()
                writer.writerow([mapping[node], x, y])
    clusters = directory / "clusters.dat"
    neutral = directory / "neutral.dat"
    cluster_command = [sys.executable, ROOT / "clusterGeneration/generate_clusters.py",
                       "--graph-only", "--strategy", clustering,
                       "--graph", graph, "--coords", coordinates,
                       "--percentage", str(percentage), "--output", clusters]
    if clustering == "streets":
        cluster_command += ["--neutral-nodes", source["neutral"],
                            "--neutral-nodes-output", neutral,
                            "--osm", source["osm"], "--node-mapping", source["mapping"]]
    run_command(cluster_command, ROOT, directory / "clustering.log")
    tuned = directory / "graph.tuned.dat"
    expanded_turns = directory / "graph.tuned.turns.dat"
    tune_command = [sys.executable, ROOT / "tune_cluster_crossing.py", graph,
                    clusters, tuned, "--coords", coordinates, "--turns", turns,
                    "--expanded-turns", expanded_turns]
    if clustering == "streets":
        tune_command += ["--neutral-nodes", neutral]
    run_command(tune_command, ROOT, directory / "tuning.log")
    return {"graph": tuned, "turns": expanded_turns,
            "in_out": directory / "graph.tuned.in-out.dat",
            "arc_clusters": directory / "graph.tuned.arc-clusters.dat",
            "nodes": nodes, "edges": edge_count, "required_edges": required}


def solver_command(model, instance, solver, best_solver):
    if model == "bestClusterPath":
        return [best_solver, instance["graph"], instance["turns"],
                instance["in_out"], instance["arc_clusters"]]
    strategy, *selection = model.split(":")
    return [solver, instance["graph"], instance["turns"], strategy, *selection]


def signature(instance, binary):
    digest = hashlib.sha256()
    for path in [instance[key] for key in ("graph", "turns", "in_out", "arc_clusters")] + [binary]:
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def measure(command, directory, model):
    directory.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    result = subprocess.run([str(part) for part in command], cwd=directory,
                            capture_output=True, text=True)
    wall_ms = (time.perf_counter() - started) * 1000
    write_log(directory / "solver.log", json.dumps([str(part) for part in command]),
              result.stdout, result.stderr)
    prefix = "BEST_CLUSTER_PATH_RESULT" if model == "bestClusterPath" else "RCPP_RESULT"
    matches = [match for match in RESULT.finditer(result.stdout) if match[1] == prefix]
    match = matches[-1] if matches else None
    elapsed = float(match[2]) if match else None
    valid = elapsed is not None and math.isfinite(elapsed) and elapsed >= 0
    completed = valid and result.returncode in (0, 2)
    return {"elapsed_ms": f"{elapsed:.6f}" if valid else "",
            "wall_ms": f"{wall_ms:.6f}", "has_solution": match[3] if match else "false",
            "optimal": match[4] if match else "false", "returncode": result.returncode,
            "outcome": "completed" if completed else "error"}


def read_rows(path):
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames != FIELDS:
            raise ValueError(f"Columnas incompatibles en {path}")
        return list(reader)


def row_key(row):
    return tuple(str(row[key]) for key in
                 ("map", "clustering", "percentage", "model", "repetition"))


def save_row(path, rows, row):
    rows[:] = [old for old in rows if row_key(old) != row_key(row)] + [row]
    temporary = path.with_suffix(".tmp")
    with temporary.open("w", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
        target.flush()
        os.fsync(target.fileno())
    temporary.replace(path)


def plot_results(rows, output, maps, percentage):
    os.environ.setdefault("MPLCONFIGDIR", str(output / ".matplotlib"))
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise RuntimeError("Instale matplotlib para generar el plot.") from error
    selected = [row for row in rows if row["map"] in maps
                and float(row["percentage"]) == percentage]
    if not selected:
        raise ValueError("No hay resultados para graficar.")
    paths = []
    for clustering in ("bfs", "streets"):
        group = [row for row in selected if row["clustering"] == clustering]
        if not group:
            continue
        counts = {row["map"]: int(row["edges"]) for row in group}
        names = sorted(counts, key=lambda name: (counts[name], name))
        fig, ax = plt.subplots(figsize=(11, 6))
        for series_index, model in enumerate(MODELS):
            measurements = [[float(row["elapsed_ms"]) / 1000 for row in group
                             if row["map"] == name and row["model"] == model
                             and row["outcome"] == "completed" and row["elapsed_ms"]]
                            for name in names]
            values = [statistics.median(values) if values else float("nan")
                      for values in measurements]
            if any(math.isfinite(value) for value in values):
                ax.plot(range(len(names)), values, marker="o", label=model,
                        color=f"C{series_index}")
        ax.set_xticks(range(len(names)), [f"{name}\n{counts[name]} aristas" for name in names])
        ax.set_xlabel("Mapas ordenados por cantidad de aristas originales (E + A)")
        ax.set_ylabel("Tiempo total del ejecutable (s; mediana)")
        ax.set_ylim(bottom=0)
        ax.set_title(f"Experimento 1 — {clustering.upper()} — clusters {percentage:g}%")
        ax.grid(alpha=0.3)
        if ax.lines:
            ax.legend()
        ax.text(0, -0.23, "Incluye corridas sin solución; corridas con error se omiten.",
                transform=ax.transAxes, fontsize=9)
        fig.tight_layout()
        path = output / f"experimento_1_tiempos_{clustering}.png"
        fig.savefig(path, dpi=180)
        plt.close(fig)
        paths.append(path)
    return paths


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiment_name", nargs="?", default="experimento_1",
                        type=parse_experiment_name)
    parser.add_argument("--maps", nargs="+", default=list(MAPS))
    parser.add_argument("--cluster-percentage", type=float, default=20.0)
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument("--data-root", type=Path, default=ROOT / "data/generator")
    parser.add_argument("--solver", type=Path, default=ROOT / "solverExec")
    parser.add_argument("--best-solver", type=Path, default=ROOT / "bestClusterPathExec")
    parser.add_argument("--plot-only", action="store_true")
    parser.add_argument("--no-plots", action="store_true",
                        help="guardar resultados CSV sin generar graficos")
    parser.add_argument("--rerun", action="store_true")
    return parser.parse_args(arguments)


def main(arguments=None):
    options = parse_arguments(arguments)
    if options.plot_only and options.no_plots:
        raise ValueError("--plot-only y --no-plots no se pueden combinar")
    if not math.isfinite(options.cluster_percentage) or not 0 < options.cluster_percentage <= 100:
        raise ValueError("cluster-percentage debe estar entre 0 y 100")
    if options.repetitions < 1:
        raise ValueError("repetitions debe ser >= 1")
    if any(not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", name) for name in options.maps):
        raise ValueError("Nombre de mapa invalido")
    output = ROOT / "experiments" / options.experiment_name
    output.mkdir(parents=True, exist_ok=True)
    csv_path = output / "resultados.csv"
    rows = read_rows(csv_path)
    errors = False
    if not options.plot_only:
        solver, best_solver = options.solver.resolve(), options.best_solver.resolve()
        for binary in (solver, best_solver):
            if not binary.is_file() or not os.access(binary, os.X_OK):
                raise ValueError(f"No existe el ejecutable {binary}; ejecute make mip mip-best-cluster")
        data_root = options.data_root.resolve()
        for name in dict.fromkeys(options.maps):
            source = source_paths(data_root, name)
            if not all(path.is_file() for path in source.values()):
                run_command([sys.executable, ROOT / "generator/mapToOsmGraph.py", name,
                             "--sin-grilla", "--data-root", data_root], ROOT,
                            output / f"generacion_{name}.log")
            for clustering in ("bfs", "streets"):
                base = output / name / clustering / f"p_{options.cluster_percentage:g}"
                instance = prepare_instance(source, base / "instance", clustering,
                                            options.cluster_percentage)
                for model in MODELS:
                    binary = best_solver if model == "bestClusterPath" else solver
                    fingerprint = signature(instance, binary)
                    for repetition in range(1, options.repetitions + 1):
                        row = {"map": name, "clustering": clustering,
                               "percentage": str(options.cluster_percentage),
                               **{key: instance[key] for key in ("nodes", "edges", "required_edges")},
                               "model": model, "repetition": repetition, "signature": fingerprint}
                        if not options.rerun and any(row_key(old) == row_key(row)
                                and old["signature"] == fingerprint
                                and old["outcome"] == "completed" for old in rows):
                            print(f"Omitido: {name} {clustering} {model} rep={repetition}", flush=True)
                            continue
                        print(f"Ejecutando: {name} {clustering} {model} rep={repetition}", flush=True)
                        directory = base / "runs" / model.replace(":", "_") / f"rep_{repetition}"
                        # Isolate outputs even when resuming a previous experiment.
                        with tempfile.TemporaryDirectory(prefix="work-", dir=base) as work:
                            measurement = measure(solver_command(model, instance, solver, best_solver),
                                                  Path(work), model)
                            directory.mkdir(parents=True, exist_ok=True)
                            shutil.copyfile(Path(work) / "solver.log", directory / "solver.log")
                        row.update(measurement)
                        save_row(csv_path, rows, row)
                        errors |= row["outcome"] == "error"
                        print(f"  {row['outcome']}: {row['elapsed_ms']} ms", flush=True)
    if not options.no_plots:
        for path in plot_results(rows, output, options.maps, options.cluster_percentage):
            print(f"Plot: {path}")
    print(f"CSV: {csv_path}")
    if errors:
        raise RuntimeError("Hubo corridas con errores; consulte los logs y reejecute para reintentarlas")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
